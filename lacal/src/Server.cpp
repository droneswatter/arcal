#include "Server.h"

#include "ClientSession.h"
#include "TopicMonitor.h"
#include "uci/base/Externalizer.h"
#include "uci/base/ExternalizerLoader.h"

#include <boost/asio.hpp>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace arcal::lacal {
namespace {

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

int runServer(const Options& opts) {
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
        std::atomic<bool> running{true};
        ClientRegistry clients;
        TopicMonitor monitor(clients, jsonExt.get(), opts.domain);
        monitor.start();

        asio::io_context io;
        auto acceptor = createAcceptor(io, opts);
        asio::signal_set signals(io, SIGINT, SIGTERM);
        std::cout << "[arlacal] listening on ws://" << opts.host << ":" << opts.port
                  << " subprotocol=owp domain=" << opts.domain << "\n";

        signals.async_wait([&](boost::system::error_code ec, int) {
            if (ec) return;
            running = false;
            monitor.stop();
            boost::system::error_code closeEc;
            acceptor.close(closeEc);
        });

        std::function<void()> doAccept;
        doAccept = [&] {
            acceptor.async_accept([&](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto client = std::make_shared<Client>(std::move(socket), opts.port);
                    clients.add(client);
                    client->start();
                } else if (ec != asio::error::operation_aborted) {
                    std::cerr << "[arlacal] accept failed: " << ec.message() << "\n";
                }
                if (running && acceptor.is_open()) doAccept();
            });
        };

        doAccept();
        io.run();
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

} // namespace arcal::lacal
