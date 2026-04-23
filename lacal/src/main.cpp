#include "arcal/AccessorFactory.h"
#include "arcal/CdrBridge.h"
#include "arcal_payload.h"
#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"

#include <dds/dds.h>
#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
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

constexpr const char* kArcalTypeName = "arcal_dds::OpaquePayload";
constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::atomic<bool> g_running{true};

uint32_t rol(uint32_t value, unsigned bits) {
    return (value << bits) | (value >> (32 - bits));
}

std::array<uint8_t, 20> sha1(const std::string& input) {
    uint64_t bitLen = static_cast<uint64_t>(input.size()) * 8;
    std::vector<uint8_t> data(input.begin(), input.end());
    data.push_back(0x80);
    while ((data.size() % 64) != 56) data.push_back(0);
    for (int i = 7; i >= 0; --i)
        data.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xff));

    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xefcdab89;
    uint32_t h2 = 0x98badcfe;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xc3d2e1f0;

    for (std::size_t chunk = 0; chunk < data.size(); chunk += 64) {
        uint32_t w[80] = {};
        for (int i = 0; i < 16; ++i) {
            const std::size_t j = chunk + static_cast<std::size_t>(i) * 4;
            w[i] = (uint32_t(data[j]) << 24) | (uint32_t(data[j + 1]) << 16) |
                   (uint32_t(data[j + 2]) << 8) | uint32_t(data[j + 3]);
        }
        for (int i = 16; i < 80; ++i)
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f = 0;
            uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdc;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6;
            }
            uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<uint8_t, 20> out{};
    const uint32_t h[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
        out[static_cast<std::size_t>(i) * 4] = static_cast<uint8_t>((h[i] >> 24) & 0xff);
        out[static_cast<std::size_t>(i) * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xff);
        out[static_cast<std::size_t>(i) * 4 + 2] = static_cast<uint8_t>((h[i] >> 8) & 0xff);
        out[static_cast<std::size_t>(i) * 4 + 3] = static_cast<uint8_t>(h[i] & 0xff);
    }
    return out;
}

std::string base64(const uint8_t* data, std::size_t len) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < len; i += 3) {
        uint32_t v = uint32_t(data[i]) << 16;
        if (i + 1 < len) v |= uint32_t(data[i + 1]) << 8;
        if (i + 2 < len) v |= uint32_t(data[i + 2]);
        out.push_back(table[(v >> 18) & 0x3f]);
        out.push_back(table[(v >> 12) & 0x3f]);
        out.push_back(i + 1 < len ? table[(v >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < len ? table[v & 0x3f] : '=');
    }
    return out;
}

std::string websocketAccept(const std::string& key) {
    auto digest = sha1(key + kWebSocketGuid);
    return base64(digest.data(), digest.size());
}

bool sendAll(int fd, const uint8_t* data, std::size_t len) {
    while (len > 0) {
        ssize_t n = ::send(fd, data, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        data += n;
        len -= static_cast<std::size_t>(n);
    }
    return true;
}

bool sendAll(int fd, const std::string& data) {
    return sendAll(fd, reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool recvExact(int fd, uint8_t* data, std::size_t len) {
    while (len > 0) {
        ssize_t n = ::recv(fd, data, len, 0);
        if (n == 0) return false;
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        data += n;
        len -= static_cast<std::size_t>(n);
    }
    return true;
}

std::string trim(std::string s) {
    auto isWs = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && isWs(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && isWs(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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

struct WebSocketFrame {
    uint8_t opcode = 0;
    std::string payload;
};

bool readFrame(int fd, WebSocketFrame& frame) {
    uint8_t header[2] = {};
    if (!recvExact(fd, header, 2)) return false;
    frame.opcode = header[0] & 0x0f;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t len = header[1] & 0x7f;
    if (len == 126) {
        uint8_t ext[2] = {};
        if (!recvExact(fd, ext, 2)) return false;
        len = (uint64_t(ext[0]) << 8) | uint64_t(ext[1]);
    } else if (len == 127) {
        uint8_t ext[8] = {};
        if (!recvExact(fd, ext, 8)) return false;
        len = 0;
        for (uint8_t b : ext) len = (len << 8) | b;
    }
    if (len > 16 * 1024 * 1024) return false;
    uint8_t mask[4] = {};
    if (masked && !recvExact(fd, mask, 4)) return false;
    frame.payload.assign(static_cast<std::size_t>(len), '\0');
    if (len > 0 && !recvExact(fd, reinterpret_cast<uint8_t*>(&frame.payload[0]),
                              static_cast<std::size_t>(len))) {
        return false;
    }
    if (masked) {
        for (std::size_t i = 0; i < frame.payload.size(); ++i)
            frame.payload[i] = static_cast<char>(frame.payload[i] ^ mask[i % 4]);
    }
    return true;
}

bool sendTextFrame(int fd, const std::string& payload) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81);
    if (payload.size() < 126) {
        frame.push_back(static_cast<uint8_t>(payload.size()));
    } else if (payload.size() <= 0xffff) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xff));
        frame.push_back(static_cast<uint8_t>(payload.size() & 0xff));
    } else {
        frame.push_back(127);
        uint64_t len = payload.size();
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xff));
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    return sendAll(fd, frame.data(), frame.size());
}

bool sendCloseFrame(int fd) {
    const uint8_t frame[2] = {0x88, 0x00};
    return sendAll(fd, frame, sizeof(frame));
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
    explicit Client(int socketFd) : fd(socketFd) {}
    ~Client() {
        if (fd >= 0) ::close(fd);
    }

    bool sendLine(const std::string& line) {
        std::lock_guard<std::mutex> lock(sendMutex);
        if (!open) return false;
        if (!sendTextFrame(fd, line)) {
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

    int fd = -1;
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

std::unordered_map<std::string, std::string> readHttpHeaders(int fd, std::string& requestLine) {
    std::string request;
    char ch = 0;
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 64 * 1024) {
        ssize_t n = ::recv(fd, &ch, 1, 0);
        if (n <= 0) throw std::runtime_error("client disconnected during handshake");
        request.push_back(ch);
    }

    std::istringstream in(request);
    std::getline(in, requestLine);
    requestLine = trim(requestLine);
    std::unordered_map<std::string, std::string> headers;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) break;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    }
    return headers;
}

bool headerContainsToken(const std::string& value, const std::string& token) {
    std::istringstream in(value);
    std::string part;
    while (std::getline(in, part, ',')) {
        if (trim(part) == token) return true;
    }
    return false;
}

bool handshake(int fd) {
    std::string requestLine;
    auto headers = readHttpHeaders(fd, requestLine);
    auto keyIt = headers.find("sec-websocket-key");
    if (keyIt == headers.end()) return false;

    auto protoIt = headers.find("sec-websocket-protocol");
    if (protoIt == headers.end() || !headerContainsToken(protoIt->second, "owp")) {
        const std::string bad = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
        sendAll(fd, bad);
        return false;
    }

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << websocketAccept(keyIt->second) << "\r\n"
             << "Sec-WebSocket-Protocol: owp\r\n"
             << "\r\n";
    return sendAll(fd, response.str());
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
        if (!handshake(client->fd)) {
            client->open = false;
            return;
        }
        while (client->open && g_running) {
            WebSocketFrame frame;
            if (!readFrame(client->fd, frame)) break;
            if (frame.opcode == 0x8) break;
            if (frame.opcode == 0x9) continue; // ping ignored for now
            if (frame.opcode != 0x1) {
                client->sendLine("-ERR Illegal-Argument expected text frame");
                continue;
            }

            std::string rest;
            auto op = firstField(frame.payload, rest);
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
        sendCloseFrame(client->fd);
    } catch (const std::exception& e) {
        std::cerr << "[arlacal] client error: " << e.what() << "\n";
    }
    client->open = false;
}

int createListenSocket(const Options& opts) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket failed");
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(opts.port);
    if (::inet_pton(AF_INET, opts.host.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        throw std::runtime_error("invalid IPv4 host address");
    }
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error(std::string("bind failed: ") + std::strerror(errno));
    }
    if (::listen(fd, 16) < 0) {
        ::close(fd);
        throw std::runtime_error("listen failed");
    }
    return fd;
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

        int listenFd = createListenSocket(opts);
        std::cout << "[arlacal] listening on ws://" << opts.host << ":" << opts.port
                  << " subprotocol=owp domain=" << opts.domain << "\n";

        while (g_running) {
            sockaddr_in peer{};
            socklen_t peerLen = sizeof(peer);
            int fd = ::accept(listenFd, reinterpret_cast<sockaddr*>(&peer), &peerLen);
            if (fd < 0) {
                if (errno == EINTR) continue;
                break;
            }
            auto client = std::make_shared<Client>(fd);
            clients.add(client);
            std::thread(clientLoop, client, opts.port).detach();
        }

        ::close(listenFd);
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
