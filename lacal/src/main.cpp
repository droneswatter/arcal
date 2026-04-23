#include "arcal/AccessorFactory.h"
#include "arcal/CdrBridge.h"
#include "arcal_payload.h"
#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"

#include <dds/dds.h>
#include <nlohmann/json.hpp>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

constexpr const char* kArcalTypeName = "arcal_dds::OpaquePayload";

std::atomic<bool> g_running{true};

std::string trim(std::string s) {
    auto isWs = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && isWs(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && isWs(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

bool validToken(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '-' || c == '.';
    });
}

bool globMatch(const std::string& pattern, const std::string& text) {
    std::size_t p = 0, t = 0, star = std::string::npos, match = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p; ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            match = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++match;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

std::optional<std::string> firstField(const std::string& line, std::string& rest) {
    std::string s = trim(line);
    if (s.empty()) return std::nullopt;
    auto pos = s.find(' ');
    if (pos == std::string::npos) {
        rest.clear();
        return s;
    }
    rest = trim(s.substr(pos + 1));
    return s.substr(0, pos);
}

std::vector<std::string> splitFields(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    std::string field;
    while (in >> field) out.push_back(field);
    return out;
}

std::string makeInfoJson(const std::string& serviceId, uint16_t port) {
    nlohmann::json info;
    info["version"] = "1.0";
    info["server_id"] = "arlacal-server";
    info["system_label"] = "arcal";
    info["service_id"] = serviceId;
    info["connect_urls"] = {std::string("ws://127.0.0.1:") + std::to_string(port)};
    info["extensions"] = {"arcal.xsub.v1"};
    info["uuids"] = {
        {"system", "00000000-0000-0000-0000-000000000000"},
        {"service", "00000000-0000-0000-0000-000000000000"}
    };
    return info.dump();
}

struct XSub {
    std::string streamId;
    std::string topicPattern;
    std::string messagePattern{"*"};
};

struct StandardSub {
    std::string subscriptionId;
    std::string messageName;
    std::string topic;
};

struct Client : std::enable_shared_from_this<Client> {
    explicit Client(std::shared_ptr<websocket::stream<tcp::socket>> wsIn)
        : ws(std::move(wsIn)) {}

    bool sendLine(const std::string& line) {
        std::lock_guard<std::mutex> lock(sendMutex);
        if (!open) return false;
        boost::system::error_code ec;
        ws->text(true);
        ws->write(asio::buffer(line), ec);
        if (ec) {
            open = false;
            return false;
        }
        return true;
    }

    bool ensureConcreteSubscription(const std::string& streamId,
                                    const std::string& topic,
                                    const std::string& messageName,
                                    std::string& subscriptionId) {
        const std::string key = streamId + "\n" + topic + "\n" + messageName;
        std::lock_guard<std::mutex> lock(stateMutex);
        auto it = concreteSubs.find(key);
        if (it != concreteSubs.end()) {
            subscriptionId = it->second;
            return false;
        }
        subscriptionId = "xsub." + std::to_string(nextSubId++);
        concreteSubs[key] = subscriptionId;
        return true;
    }

    std::shared_ptr<websocket::stream<tcp::socket>> ws;
    std::atomic<bool> open{true};
    bool initialized = false;
    bool verbose = true;
    std::string serviceId;
    std::mutex sendMutex;
    std::mutex stateMutex;
    std::vector<XSub> xsubs;
    std::unordered_map<std::string, StandardSub> standardSubs;
    std::unordered_map<std::string, std::string> concreteSubs;
    uint64_t nextSubId = 1;
};

class ClientRegistry {
public:
    void add(const std::shared_ptr<Client>& client) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.push_back(client);
    }

    void removeClosed() {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
            [](const std::weak_ptr<Client>& weak) {
                auto c = weak.lock();
                return !c || !c->open;
            }), clients_.end());
    }

    std::vector<std::shared_ptr<Client>> snapshot() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Client>> out;
        for (auto it = clients_.begin(); it != clients_.end();) {
            if (auto c = it->lock()) {
                if (c->open) out.push_back(c);
                ++it;
            } else {
                it = clients_.erase(it);
            }
        }
        return out;
    }

private:
    std::mutex mutex_;
    std::vector<std::weak_ptr<Client>> clients_;
};

class TopicMonitor {
public:
    TopicMonitor(ClientRegistry& clients, uci::base::Externalizer& jsonExt, int domainId)
        : clients_(clients), jsonExt_(jsonExt) {
        participant_ = dds_create_participant(static_cast<dds_domainid_t>(domainId), nullptr, nullptr);
        if (participant_ < 0)
            throw std::runtime_error("failed to create DDS participant");

        builtinReader_ = dds_create_reader(participant_, DDS_BUILTIN_TOPIC_DCPSPUBLICATION,
                                           nullptr, nullptr);
        if (builtinReader_ < 0)
            throw std::runtime_error("failed to create DDS builtin publication reader");

        builtinCond_ = dds_create_readcondition(builtinReader_, DDS_ANY_STATE);
        waitset_ = dds_create_waitset(participant_);
        dds_waitset_attach(waitset_, builtinCond_, static_cast<dds_attach_t>(builtinCond_));
    }

    ~TopicMonitor() {
        stop();
        if (thread_.joinable()) thread_.join();
        for (auto& [_, cond] : conditions_) dds_delete(cond);
        for (auto& [_, reader] : readers_) dds_delete(reader);
        if (waitset_ >= 0) dds_delete(waitset_);
        if (builtinCond_ >= 0) dds_delete(builtinCond_);
        if (builtinReader_ >= 0) dds_delete(builtinReader_);
        if (participant_ >= 0) dds_delete(participant_);
    }

    void start() {
        running_ = true;
        thread_ = std::thread([this] { run(); });
    }

    void stop() {
        running_ = false;
    }

private:
    void run() {
        constexpr int kMaxTriggers = 64;
        dds_attach_t triggered[kMaxTriggers];
        while (running_) {
            int n = dds_waitset_wait(waitset_, triggered, kMaxTriggers, DDS_MSECS(500));
            if (n < 0) break;
            for (int i = 0; i < n; ++i) {
                auto cond = static_cast<dds_entity_t>(triggered[i]);
                if (cond == builtinCond_) {
                    pollBuiltin();
                } else {
                    auto it = condTopics_.find(cond);
                    if (it != condTopics_.end()) drainReader(it->second);
                }
            }
        }
    }

    void pollBuiltin() {
        static constexpr int kBatch = 32;
        void* ptrs[kBatch] = {};
        dds_sample_info_t info[kBatch];
        int n = dds_take(builtinReader_, ptrs, info, kBatch, kBatch);
        for (int i = 0; i < n; ++i) {
            if (!info[i].valid_data) continue;
            auto* ep = static_cast<dds_builtintopic_endpoint_t*>(ptrs[i]);
            if (ep->topic_name && ep->type_name &&
                std::strcmp(ep->type_name, kArcalTypeName) == 0) {
                ensureReader(ep->topic_name);
            }
        }
        dds_return_loan(builtinReader_, ptrs, n);
    }

    void ensureReader(const std::string& topicName) {
        if (readers_.count(topicName)) return;

        dds_entity_t topic = dds_create_topic(participant_, &arcal_dds_OpaquePayload_desc,
                                              topicName.c_str(), nullptr, nullptr);
        if (topic < 0) {
            std::cerr << "[arlacal] failed to create topic reader for " << topicName << "\n";
            return;
        }

        dds_entity_t reader = dds_create_reader(participant_, topic, nullptr, nullptr);
        dds_delete(topic);
        if (reader < 0) {
            std::cerr << "[arlacal] failed to create reader for " << topicName << "\n";
            return;
        }

        dds_entity_t cond = dds_create_readcondition(reader, DDS_ANY_STATE);
        dds_waitset_attach(waitset_, cond, static_cast<dds_attach_t>(cond));
        readers_[topicName] = reader;
        conditions_[topicName] = cond;
        condTopics_[cond] = topicName;
        std::cout << "[arlacal] discovered topic=" << topicName << "\n";
    }

    void drainReader(const std::string& topicName) {
        auto readerIt = readers_.find(topicName);
        if (readerIt == readers_.end()) return;

        static constexpr int kBatch = 16;
        void* ptrs[kBatch] = {};
        dds_sample_info_t info[kBatch];
        int n = dds_take(readerIt->second, ptrs, info, kBatch, kBatch);
        for (int i = 0; i < n; ++i) {
            if (!info[i].valid_data) continue;
            auto* sample = static_cast<arcal_dds_OpaquePayload*>(ptrs[i]);
            std::vector<uint8_t> data(sample->data._buffer,
                                      sample->data._buffer + sample->data._length);
            publishDecoded(topicName, sample->type_tag, data);
        }
        dds_return_loan(readerIt->second, ptrs, n);
    }

    void publishDecoded(const std::string& topicName, uint32_t tag,
                        const std::vector<uint8_t>& data) {
        uci::base::Accessor* raw = arcal::arcalCreateAccessor(tag);
        if (!raw) {
            std::cerr << "[arlacal] unknown type tag 0x" << std::hex << tag
                      << std::dec << " topic=" << topicName << "\n";
            return;
        }
        std::unique_ptr<uci::base::Accessor, void(*)(uci::base::Accessor*)>
            acc(raw, arcal::arcalDestroyAccessor);

        try {
            arcal::cdrDeserialize(tag, data, *acc);
            std::string json;
            jsonExt_.write(*acc, json);
            const std::string messageName = acc->typeName();
            deliver(topicName, messageName, json);
        } catch (const std::exception& e) {
            std::cerr << "[arlacal] decode failed topic=" << topicName
                      << " tag=0x" << std::hex << tag << std::dec
                      << ": " << e.what() << "\n";
        }
    }

    void deliver(const std::string& topicName,
                 const std::string& messageName,
                 const std::string& json) {
        for (auto& client : clients_.snapshot()) {
            std::vector<XSub> xsubs;
            std::vector<StandardSub> standardSubs;
            {
                std::lock_guard<std::mutex> lock(client->stateMutex);
                xsubs = client->xsubs;
                for (const auto& [_, sub] : client->standardSubs)
                    standardSubs.push_back(sub);
            }

            for (const StandardSub& sub : standardSubs) {
                if (sub.topic == topicName && sub.messageName == messageName)
                    client->sendLine("MSG " + sub.subscriptionId + " " + json);
            }

            for (const XSub& xsub : xsubs) {
                if (!globMatch(xsub.topicPattern, topicName) ||
                    !globMatch(xsub.messagePattern, messageName)) {
                    continue;
                }

                std::string subId;
                const bool isNew = client->ensureConcreteSubscription(
                    xsub.streamId, topicName, messageName, subId);
                if (isNew) {
                    client->sendLine("XSUBINFO " + xsub.streamId + " " + subId + " " +
                                     topicName + " " + messageName);
                }
                client->sendLine("MSG " + subId + " " + json);
            }
        }
        clients_.removeClosed();
    }

    ClientRegistry& clients_;
    uci::base::Externalizer& jsonExt_;
    dds_entity_t participant_ = -1;
    dds_entity_t builtinReader_ = -1;
    dds_entity_t builtinCond_ = -1;
    dds_entity_t waitset_ = -1;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::unordered_map<std::string, dds_entity_t> readers_;
    std::unordered_map<std::string, dds_entity_t> conditions_;
    std::unordered_map<dds_entity_t, std::string> condTopics_;
};

struct Options {
    std::string host{"127.0.0.1"};
    uint16_t port = 8766;
    int domain = 0;
};

struct ExternalizerHandle {
    uci::base::ExternalizerLoader* loader = nullptr;
    uci::base::Externalizer* ext = nullptr;

    ExternalizerHandle(uci::base::ExternalizerLoader* loaderIn,
                       uci::base::Externalizer* extIn)
        : loader(loaderIn), ext(extIn) {}

    ~ExternalizerHandle() {
        reset();
    }

    ExternalizerHandle(const ExternalizerHandle&) = delete;
    ExternalizerHandle& operator=(const ExternalizerHandle&) = delete;

    uci::base::Externalizer& get() const { return *ext; }
    explicit operator bool() const { return ext != nullptr; }

    void reset() {
        if (loader && ext) {
            loader->destroyExternalizer(ext);
            ext = nullptr;
        }
    }
};

void usage(const char* prog) {
    std::cerr << "usage: " << prog
              << " [--host ADDR] [--port PORT] [--domain ID]\n";
}

Options parseArgs(int argc, char** argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            opts.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            opts.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--domain" && i + 1 < argc) {
            opts.domain = std::stoi(argv[++i]);
        } else {
            usage(argv[0]);
            std::exit(1);
        }
    }
    return opts;
}

bool headerContainsToken(const std::string& value, const std::string& token) {
    std::istringstream in(value);
    std::string part;
    while (std::getline(in, part, ',')) {
        if (trim(part) == token) return true;
    }
    return false;
}

bool handshake(websocket::stream<tcp::socket>& ws) {
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    http::read(ws.next_layer(), buffer, req);

    const auto protocolView = req[http::field::sec_websocket_protocol];
    const std::string protocol(protocolView.data(), protocolView.size());
    if (!headerContainsToken(protocol, "owp")) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, "arlacal-server");
        res.set(http::field::connection, "close");
        res.body() = "Sec-WebSocket-Protocol: owp required\n";
        res.prepare_payload();
        http::write(ws.next_layer(), res);
        return false;
    }

    ws.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, "arlacal-server");
            res.set(http::field::sec_websocket_protocol, "owp");
        }));
    ws.accept(req);
    ws.text(true);
    return true;
}

bool handleInit(Client& client, const std::string& rest, uint16_t port) {
    if (client.initialized) {
        client.sendLine("-ERR Illegal-State INIT already received");
        return false;
    }
    try {
        auto init = nlohmann::json::parse(rest);
        if (!init.contains("versions") || !init.contains("schema") || !init.contains("service_id")) {
            client.sendLine("-ERR Illegal-Argument INIT missing required field");
            return false;
        }
        bool supports10 = false;
        for (const auto& version : init["versions"])
            supports10 = supports10 || version.get<std::string>() == "1.0";
        if (!supports10) {
            client.sendLine("-ERR Unsupported-Version no supported OWP version");
            return false;
        }
        client.verbose = init.value("verbose", true);
        client.serviceId = init["service_id"].get<std::string>();
        client.initialized = true;
        client.sendLine("INFO " + makeInfoJson(client.serviceId, port));
        if (client.verbose) client.sendLine("+OK");
        return true;
    } catch (const std::exception& e) {
        client.sendLine(std::string("-ERR Illegal-Argument ") + e.what());
        return false;
    }
}

void handleXSub(Client& client, const std::string& rest) {
    auto fields = splitFields(rest);
    if (fields.size() < 2 || fields.size() > 3) {
        client.sendLine("-ERR Illegal-Argument XSUB requires stream-id topic-pattern [message-pattern]");
        return;
    }
    if (!validToken(fields[0])) {
        client.sendLine("-ERR Illegal-Argument invalid stream-id");
        return;
    }
    XSub xsub;
    xsub.streamId = fields[0];
    xsub.topicPattern = fields[1];
    if (fields.size() == 3) xsub.messagePattern = fields[2];

    {
        std::lock_guard<std::mutex> lock(client.stateMutex);
        client.xsubs.push_back(xsub);
    }
    if (client.verbose) client.sendLine("+OK");
}

void handleSub(Client& client, const std::string& rest) {
    auto fields = splitFields(rest);
    if (fields.size() < 3 || fields.size() > 4) {
        client.sendLine("-ERR Illegal-Argument SUB requires subscription-id message-name topic [group]");
        return;
    }
    if (!validToken(fields[0]) || !validToken(fields[2]) ||
        (fields.size() == 4 && !validToken(fields[3]))) {
        client.sendLine("-ERR Illegal-Argument invalid SUB token");
        return;
    }

    StandardSub sub;
    sub.subscriptionId = fields[0];
    sub.messageName = fields[1];
    sub.topic = fields[2];

    {
        std::lock_guard<std::mutex> lock(client.stateMutex);
        if (client.standardSubs.count(sub.subscriptionId)) {
            client.sendLine("-ERR Illegal-State subscription-id already in use");
            return;
        }
        client.standardSubs.emplace(sub.subscriptionId, sub);
    }
    if (client.verbose) client.sendLine("+OK");
}

void handleUnsub(Client& client, const std::string& rest) {
    auto fields = splitFields(rest);
    if (fields.size() != 1 || !validToken(fields[0])) {
        client.sendLine("-ERR Illegal-Argument UNSUB requires subscription-id");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(client.stateMutex);
        auto erased = client.standardSubs.erase(fields[0]);
        if (erased == 0) {
            client.sendLine("-ERR Illegal-State subscription-id is not active");
            return;
        }
    }
    if (client.verbose) client.sendLine("+OK");
}

void handleXUnsub(Client& client, const std::string& rest) {
    auto fields = splitFields(rest);
    if (fields.size() != 1 || !validToken(fields[0])) {
        client.sendLine("-ERR Illegal-Argument XUNSUB requires stream-id");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(client.stateMutex);
        const std::string streamId = fields[0];
        client.xsubs.erase(std::remove_if(client.xsubs.begin(), client.xsubs.end(),
            [&](const XSub& xsub) { return xsub.streamId == streamId; }),
            client.xsubs.end());
        for (auto it = client.concreteSubs.begin(); it != client.concreteSubs.end();) {
            if (it->first.rfind(streamId + "\n", 0) == 0) it = client.concreteSubs.erase(it);
            else ++it;
        }
    }
    if (client.verbose) client.sendLine("+OK");
}

void clientLoop(std::shared_ptr<Client> client, uint16_t port) {
    try {
        if (!handshake(*client->ws)) {
            client->open = false;
            return;
        }
        while (client->open && g_running) {
            beast::flat_buffer buffer;
            boost::system::error_code ec;
            client->ws->read(buffer, ec);
            if (ec == websocket::error::closed) break;
            if (ec) throw boost::system::system_error(ec);
            if (!client->ws->got_text())
                continue;

            std::string rest;
            auto payload = beast::buffers_to_string(buffer.data());
            auto op = firstField(payload, rest);
            if (!op) continue;

            if (*op == "INIT") {
                if (!handleInit(*client, rest, port)) break;
            } else if (!client->initialized) {
                client->sendLine("-ERR Illegal-State INIT must be first operation");
                break;
            } else if (*op == "XSUB") {
                handleXSub(*client, rest);
            } else if (*op == "XUNSUB") {
                handleXUnsub(*client, rest);
            } else if (*op == "SUB") {
                handleSub(*client, rest);
            } else if (*op == "UNSUB") {
                handleUnsub(*client, rest);
            } else if (*op == "PUB") {
                client->sendLine("-ERR Illegal-State operation not implemented in this arlacal-server build");
            } else {
                client->sendLine("-ERR Illegal-Argument unknown operation");
            }
        }
        boost::system::error_code ec;
        client->ws->close(websocket::close_code::normal, ec);
    } catch (const std::exception& e) {
        std::cerr << "[arlacal] client error: " << e.what() << "\n";
    }
    client->open = false;
}

tcp::acceptor createAcceptor(asio::io_context& io, const Options& opts) {
    boost::system::error_code ec;
    auto address = asio::ip::make_address(opts.host, ec);
    if (ec) throw std::runtime_error("invalid host address: " + ec.message());

    tcp::endpoint endpoint(address, opts.port);
    tcp::acceptor acceptor(io);
    acceptor.open(endpoint.protocol());
    acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(asio::socket_base::max_listen_connections);
    return acceptor;
}

} // namespace

int main(int argc, char** argv) {
    Options opts = parseArgs(argc, argv);

    auto* loader = uci_getExternalizerLoader();
    if (!loader) {
        std::cerr << "[arlacal] failed to create externalizer loader\n";
        return 1;
    }

    ExternalizerHandle jsonExt(loader, loader->getExternalizer("JSON", "2.5.0", "2.5.0"));
    if (!jsonExt) {
        std::cerr << "[arlacal] JSON externalizer unavailable\n";
        uci_destroyExternalizerLoader(loader);
        return 1;
    }

    try {
        ClientRegistry clients;
        TopicMonitor monitor(clients, jsonExt.get(), opts.domain);
        monitor.start();

        asio::io_context io;
        auto acceptor = createAcceptor(io, opts);
        std::cout << "[arlacal] listening on ws://" << opts.host << ":" << opts.port
                  << " subprotocol=owp domain=" << opts.domain << "\n";

        while (g_running) {
            tcp::socket socket(io);
            boost::system::error_code ec;
            acceptor.accept(socket, ec);
            if (ec) {
                std::cerr << "[arlacal] accept failed: " << ec.message() << "\n";
                continue;
            }
            auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
            auto client = std::make_shared<Client>(std::move(ws));
            clients.add(client);
            std::thread(clientLoop, client, opts.port).detach();
        }

        monitor.stop();
    } catch (const std::exception& e) {
        std::cerr << "[arlacal] fatal: " << e.what() << "\n";
        jsonExt.reset();
        uci_destroyExternalizerLoader(loader);
        return 1;
    }

    jsonExt.reset();
    uci_destroyExternalizerLoader(loader);
    return 0;
}
