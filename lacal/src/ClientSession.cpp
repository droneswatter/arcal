#include "ClientSession.h"

#include "Protocol.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>

namespace arcal::lacal {
namespace {

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

} // namespace

Client::Client(tcp::socket socket, uint16_t port)
    : ws_(std::move(socket)), port_(port) {}

void Client::start() {
    readHttpRequest();
}

bool Client::sendLine(const std::string& line) {
    if (!open) return false;
    asio::post(ws_.get_executor(), [self = shared_from_this(), line] {
        const bool writeInProgress = !self->writeQueue_.empty();
        self->writeQueue_.push_back(line);
        if (!writeInProgress) self->writeNext();
    });
    return true;
}

bool Client::ensureConcreteSubscription(const std::string& streamId,
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

void Client::readHttpRequest() {
    auto self = shared_from_this();
    http::async_read(ws_.next_layer(), httpBuffer_, request_,
        [self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                self->open = false;
                return;
            }
            self->acceptWebSocket();
        });
}

void Client::acceptWebSocket() {
    const auto protocolView = request_[http::field::sec_websocket_protocol];
    const std::string protocol(protocolView.data(), protocolView.size());
    if (!headerContainsToken(protocol, "owp")) {
        auto res = std::make_shared<http::response<http::string_body>>(
            http::status::bad_request, request_.version());
        res->set(http::field::server, "arlacal-server");
        res->set(http::field::connection, "close");
        res->body() = "Sec-WebSocket-Protocol: owp required\n";
        res->prepare_payload();
        auto self = shared_from_this();
        http::async_write(ws_.next_layer(), *res,
            [self, res](boost::system::error_code, std::size_t) {
                self->open = false;
            });
        return;
    }

    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, "arlacal-server");
            res.set(http::field::sec_websocket_protocol, "owp");
        }));
    ws_.text(true);
    auto self = shared_from_this();
    ws_.async_accept(request_, [self](boost::system::error_code ec) {
        if (ec) {
            self->open = false;
            return;
        }
        self->readNext();
    });
}

void Client::readNext() {
    readBuffer_.consume(readBuffer_.size());
    auto self = shared_from_this();
    ws_.async_read(readBuffer_, [self](boost::system::error_code ec, std::size_t) {
        if (ec == websocket::error::closed) {
            self->open = false;
            return;
        }
        if (ec) {
            std::cerr << "[arlacal] client read error: " << ec.message() << "\n";
            self->open = false;
            return;
        }
        if (self->ws_.got_text()) {
            self->handleLine(beast::buffers_to_string(self->readBuffer_.data()));
        }
        if (self->open) self->readNext();
    });
}

void Client::handleLine(const std::string& payload) {
    std::string rest;
    auto op = firstField(payload, rest);
    if (!op) return;

    if (*op == "INIT") {
        if (!handleInit(*this, rest, port_)) open = false;
    } else if (!initialized) {
        sendLine("-ERR Illegal-State INIT must be first operation");
        open = false;
    } else if (*op == "XSUB") {
        handleXSub(*this, rest);
    } else if (*op == "XUNSUB") {
        handleXUnsub(*this, rest);
    } else if (*op == "SUB") {
        handleSub(*this, rest);
    } else if (*op == "UNSUB") {
        handleUnsub(*this, rest);
    } else if (*op == "PUB") {
        sendLine("-ERR Illegal-State operation not implemented in this arlacal-server build");
    } else {
        sendLine("-ERR Illegal-Argument unknown operation");
    }
}

void Client::writeNext() {
    auto self = shared_from_this();
    ws_.text(true);
    ws_.async_write(asio::buffer(writeQueue_.front()),
        [self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                self->open = false;
                return;
            }
            self->writeQueue_.pop_front();
            if (!self->writeQueue_.empty()) self->writeNext();
        });
}

void ClientRegistry::add(const std::shared_ptr<Client>& client) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.push_back(client);
}

void ClientRegistry::removeClosed() {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
        [](const std::weak_ptr<Client>& weak) {
            auto c = weak.lock();
            return !c || !c->open;
        }), clients_.end());
}

std::vector<std::shared_ptr<Client>> ClientRegistry::snapshot() {
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

} // namespace arcal::lacal
