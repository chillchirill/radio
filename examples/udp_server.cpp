#include <boost/asio.hpp>

#include "udp_protocol.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace net = boost::asio;
using udp = net::ip::udp;

namespace {

using Clock = std::chrono::steady_clock;

constexpr unsigned short kDefaultPort = 9000;
constexpr unsigned int kDefaultIntervalMs = 500;
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

bool ParseUnsignedInt(std::string_view text, unsigned int& value) {
    unsigned parsed = 0;
    const auto [ptr, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);

    if (error != std::errc{} || ptr != text.data() + text.size()) {
        return false;
    }

    value = parsed;
    return true;
}

void PrintUsage(const char* exe_name) {
    std::cout
        << "Usage: " << exe_name << " [password] [port] [interval_ms]\n"
        << "Defaults: password=\"" << kDefaultPassword
        << "\" port=" << kDefaultPort
        << " interval_ms=" << kDefaultIntervalMs
        << "\n";
}

struct ClientSession {
    udp::endpoint endpoint;
    const Clock::time_point connected_at;
};

class UdpPasswordServer {
public:
//Створення об'єкта сокета щоб слухати вхідні UDP пакети
    UdpPasswordServer(net::io_context& io_context, unsigned short port, std::string password)
        : socket_(io_context, udp::endpoint(udp::v4(), port)),
          password_(std::move(password)) {}

    void WaitForAuthorizedClient() {
        std::array<char, 256> buffer{};

        std::cout << "Waiting for UDP password on port " << socket_.local_endpoint().port() << std::endl;

        while (!session_.has_value()) {
            udp::endpoint remote_endpoint;
            const std::size_t received = socket_.receive_from(net::buffer(buffer), remote_endpoint);
            const std::string_view password_attempt(buffer.data(), received);

            if (password_attempt == password_) {
                session_.emplace(ClientSession{ remote_endpoint, Clock::now() });
                socket_.connect(remote_endpoint);
                socket_.send(net::buffer(udp_example::kAcceptedResponse));

                std::cout
                    << "Authorized client "
                    << remote_endpoint.address().to_string()
                    << ":"
                    << remote_endpoint.port()
                    << std::endl;
                return;
            }

            socket_.send_to(net::buffer(udp_example::kRejectedResponse), remote_endpoint);

            std::cout
                << "Rejected password from "
                << remote_endpoint.address().to_string()
                << ":"
                << remote_endpoint.port()
                << std::endl;
        }
    }

    void SendValues(
        unsigned short value1,
        unsigned short value2,
        unsigned short value3,
        unsigned short value4) {
        if (!session_.has_value()) {
            throw std::logic_error("SendValues() called before client authorization.");
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - session_->connected_at);

        const udp_example::Payload payload{
            static_cast<std::uint16_t>(value1),
            static_cast<std::uint16_t>(value2),
            static_cast<std::uint16_t>(value3),
            static_cast<std::uint16_t>(value4),
            udp_example::ClampToUint32(static_cast<unsigned long long>(elapsed.count()))
        };

        const auto packet = udp_example::EncodePayload(payload);
        socket_.send(net::buffer(packet));
    }

private:
    udp::socket socket_;
    std::string password_;
    std::optional<ClientSession> session_;
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
    //по якому порту слухати
    unsigned short port = kDefaultPort;
    if (argc >= 3 && !ParsePort(argv[2], port)) {
        std::cerr << "Invalid port: " << argv[2] << std::endl;
        return 1;
    }
    //інтервал відправки в мс
    unsigned int interval_ms = kDefaultIntervalMs;
    if (argc >= 4 && !ParseUnsignedInt(argv[3], interval_ms)) {
        std::cerr << "Invalid interval: " << argv[3] << std::endl;
        return 1;
    }
    // тут сама відправка
    try {
        net::io_context io_context;
        UdpPasswordServer server(io_context, port, password);
        server.WaitForAuthorizedClient();

        std::uint16_t counter = 1;
        for (;;) {
            server.SendValues(
                counter,
                static_cast<unsigned short>(counter + 1),
                static_cast<unsigned short>(counter + 2),
                static_cast<unsigned short>(counter + 3));

            counter = static_cast<std::uint16_t>(counter + 4);
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    } catch (const std::exception& error) {
        std::cerr << "UDP server failed: " << error.what() << std::endl;
        return 1;
    }
}

// пароль порт інтервал_ms