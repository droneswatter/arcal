#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

namespace {

void require(bool cond, const char* what) {
    if (!cond) {
        std::cerr << "FAIL: " << what << "\n";
        std::exit(1);
    }
}

uint16_t reserveFreePort() {
    asio::io_context io;
    tcp::acceptor acceptor(io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    return acceptor.local_endpoint().port();
}

class ChildProcess {
public:
    ChildProcess(const std::string& exe, uint16_t port) {
        const std::string portStr = std::to_string(port);
        pid_ = ::fork();
        if (pid_ < 0)
            throw std::runtime_error(std::string("fork failed: ") + std::strerror(errno));

        if (pid_ == 0) {
            ::execl(exe.c_str(), exe.c_str(),
                    "--host", "127.0.0.1",
                    "--port", portStr.c_str(),
                    "--domain", "0",
                    static_cast<char*>(nullptr));
            std::cerr << "exec failed: " << std::strerror(errno) << "\n";
            std::_Exit(127);
        }
    }

    ~ChildProcess() {
        stop();
    }

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    void stop() {
        if (pid_ <= 0) return;
        int status = 0;
        if (::waitpid(pid_, &status, WNOHANG) == 0) {
            ::kill(pid_, SIGTERM);
            for (int i = 0; i < 20; ++i) {
                if (::waitpid(pid_, &status, WNOHANG) == pid_) {
                    pid_ = -1;
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            ::kill(pid_, SIGKILL);
            ::waitpid(pid_, &status, 0);
        }
        pid_ = -1;
    }

private:
    pid_t pid_ = -1;
};

tcp::socket connectWithRetry(asio::io_context& io, uint16_t port) {
    tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), port);
    for (int i = 0; i < 100; ++i) {
        tcp::socket socket(io);
        boost::system::error_code ec;
        socket.connect(endpoint, ec);
        if (!ec)
            return socket;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    throw std::runtime_error("server did not accept connections");
}

std::string readText(websocket::stream<tcp::socket>& ws) {
    beast::flat_buffer buffer;
    ws.read(buffer);
    return beast::buffers_to_string(buffer.data());
}

} // namespace

int main(int argc, char** argv) {
    require(argc == 2, "expected arlacal-server path argument");

    const std::string serverPath = argv[1];
    const uint16_t port = reserveFreePort();
    ChildProcess server(serverPath, port);

    asio::io_context io;
    websocket::stream<tcp::socket> ws(connectWithRetry(io, port));
    ws.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(http::field::sec_websocket_protocol, "owp");
        }));

    websocket::response_type response;
    ws.handshake(response, "127.0.0.1:" + std::to_string(port), "/");
    require(response[http::field::sec_websocket_protocol] == "owp",
            "server selected owp subprotocol");

    const std::string init =
        R"(INIT {"versions":["1.0"],"schema":"2.5.0","verbose":true,"service_id":"lacal-test"})";
    ws.write(asio::buffer(init));

    const std::string info = readText(ws);
    require(info.rfind("INFO ", 0) == 0, "INIT returned INFO");
    require(info.find(R"("extensions":["arcal.xsub.v1"])") != std::string::npos,
            "INFO advertises XSUB extension");
    require(readText(ws) == "+OK", "INIT returned +OK");

    ws.write(asio::buffer(std::string("XSUB smoke *")));
    require(readText(ws) == "+OK", "XSUB returned +OK");

    ws.write(asio::buffer(std::string("SUB sub1 PositionReportMT PositionReport")));
    require(readText(ws) == "+OK", "SUB returned +OK");

    ws.write(asio::buffer(std::string("UNSUB sub1")));
    require(readText(ws) == "+OK", "UNSUB returned +OK");

    ws.write(asio::buffer(std::string("XUNSUB smoke")));
    require(readText(ws) == "+OK", "XUNSUB returned +OK");

    boost::system::error_code ec;
    ws.close(websocket::close_code::normal, ec);

    std::cout << "PASS lacal_owp_smoke_test\n";
    return 0;
}
