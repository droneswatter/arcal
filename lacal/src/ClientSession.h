#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace arcal::lacal {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

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

class Client : public std::enable_shared_from_this<Client> {
public:
    Client(tcp::socket socket, uint16_t port);

    void start();
    bool sendLine(const std::string& line);
    bool ensureConcreteSubscription(const std::string& streamId,
                                    const std::string& topic,
                                    const std::string& messageName,
                                    std::string& subscriptionId);

    std::atomic<bool> open{true};
    bool initialized = false;
    bool verbose = true;
    std::string serviceId;
    std::mutex stateMutex;
    std::vector<XSub> xsubs;
    std::unordered_map<std::string, StandardSub> standardSubs;
    std::unordered_map<std::string, std::string> concreteSubs;
    uint64_t nextSubId = 1;

private:
    void readHttpRequest();
    void acceptWebSocket();
    void readNext();
    void handleLine(const std::string& payload);
    void writeNext();

    websocket::stream<tcp::socket> ws_;
    uint16_t port_;
    beast::flat_buffer httpBuffer_;
    beast::flat_buffer readBuffer_;
    http::request<http::string_body> request_;
    std::deque<std::string> writeQueue_;
};

class ClientRegistry {
public:
    void add(const std::shared_ptr<Client>& client);
    void removeClosed();
    std::vector<std::shared_ptr<Client>> snapshot();

private:
    std::mutex mutex_;
    std::vector<std::weak_ptr<Client>> clients_;
};

} // namespace arcal::lacal
