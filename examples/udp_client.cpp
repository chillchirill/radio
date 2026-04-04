#include <boost/asio.hpp>

#include "udp_protocol.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace net = boost::asio;
using udp = net::ip::udp;

namespace {

constexpr std::string_view kDefaultHost = "127.0.0.1";
constexpr unsigned short kDefaultPort = 9000;
constexpr std::string_view kDefaultPassword = "secret";

bool ParsePort(std::string_view text, unsigned short& port) {
    unsigned value = 0;
    const auto [ptr, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc{} || ptr != text.data() + text.size() || value > UINT16_MAX) {
        return false;
    }

    port = static_cast<unsigned short>(value);
    return true;
}

void PrintUsage(const char* exe_name) {
    std::cout
        << "Usage: " << exe_name << " [password] [host] [port]\n"
        << "Defaults: password=\"" << kDefaultPassword
        << "\" host=" << kDefaultHost
        << " port=" << kDefaultPort
        << "\n";
}

class UdpPasswordClient {
public:
    UdpPasswordClient(net::io_context& io_context, std::string_view host, unsigned short port)
        : socket_(io_context) {
        udp::resolver resolver(io_context);
        const auto endpoints = resolver.resolve(udp::v4(), std::string(host), std::to_string(port));

        if (endpoints.empty()) {
            throw std::runtime_error("No UDP endpoint resolved.");
        }

        socket_.open(udp::v4());
        socket_.connect(endpoints.begin()->endpoint());
    }

    bool Authorize(std::string_view password) {
        socket_.send(net::buffer(password.data(), password.size()));

        std::array<char, 16> response{};
        const std::size_t received = socket_.receive(net::buffer(response));
        const std::string_view reply(response.data(), received);

        if (reply == udp_example::kAcceptedResponse) {
            std::cout << "Password accepted, waiting for packets..." << std::endl;
            return true;
        }

        if (reply == udp_example::kRejectedResponse) {
            std::cout << "Password rejected by server." << std::endl;
            return false;
        }

        std::cout << "Unexpected response from server: " << reply << std::endl;
        return false;
    }

    void ReceiveLoop() {
        for (;;) {
            udp_example::PacketBuffer packet{};
            const std::size_t received = socket_.receive(net::buffer(packet));

            if (received != udp_example::kDataPacketSize) {
                std::cout << "Ignoring datagram with unexpected size: " << received << std::endl;
                continue;
            }
            //тут отримали данні
            const auto payload = udp_example::DecodePayload(packet);
            std::cout
                << static_cast<unsigned int>(payload.value1) << ' '
                << static_cast<unsigned int>(payload.value2) << ' '
                << static_cast<unsigned int>(payload.value3) << ' '
                << static_cast<unsigned int>(payload.value4) << ' '
                << static_cast<unsigned int>(payload.elapsed_ms)
                << std::endl;
        }
    }

private:
    udp::socket socket_;
};

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h")) {
        PrintUsage(argv[0]);
        return 0;
    }

    if (argc > 4) {
        PrintUsage(argv[0]);
        return 1;
    }

    const std::string password = argc >= 2 ? argv[1] : std::string(kDefaultPassword);
    const std::string host = argc >= 3 ? argv[2] : std::string(kDefaultHost);

    unsigned short port = kDefaultPort;
    if (argc >= 4 && !ParsePort(argv[3], port)) {
        std::cerr << "Invalid port: " << argv[3] << std::endl;
        return 1;
    }

    try {
        net::io_context io_context;
        UdpPasswordClient client(io_context, host, port);

        if (!client.Authorize(password)) {
            return 1;
        }

        client.ReceiveLoop();
    } catch (const std::exception& error) {
        std::cerr << "UDP client failed: " << error.what() << std::endl;
        return 1;
    }
}

// пароль ip port