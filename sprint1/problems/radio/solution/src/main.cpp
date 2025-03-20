#include "audio.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstdint>
#include <exception>
#include <iostream>
#include <boost/asio.hpp>
#include <sstream>

namespace net = boost::asio;
using net::ip::udp;

using namespace std::literals;

void StartServer(uint16_t port);
void StartClient(uint16_t port);

int main(int argc, char** argv) {

    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <client or server> <port connection>" << std::endl;
        return 1;
    }

    std::string usage = argv[1];
    std::istringstream num(argv[2]);
    int port;

    if (!(num >> port)) {
        std::cerr << "Wrong number port!!! Port 0" << std::endl;
        return 1;
    }

    if (usage == "client") {
        StartClient(port);

    } else if (usage == "server") {
        StartServer(port);
    }    

    return 0;
}

void StartServer(uint16_t port) {
    static const size_t max_buffer_size = 65000;
    Player player(ma_format_u8, 1);

    try {
        net::io_context io_context;
        udp::socket socket(io_context, udp::endpoint(udp::v4(), port));

        for(;;) {
            std::cout << "Start server" << std::endl;
            std::vector<char> recv_buf(max_buffer_size);
            udp::endpoint remote_endpoint;

            auto size = socket.receive_from(net::buffer(recv_buf), remote_endpoint);

            if (size > 0) {
                auto len = size / player.GetFrameSize();
                player.PlayBuffer(recv_buf.data(), len, 1.5s); 
            }
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

void StartClient(uint16_t port) {
    static const size_t max_buffer_size = 65000;
    Recorder recorder(ma_format_u8, 1);

    try {
        net::io_context io_context;

        udp::socket socket(io_context, udp::v4());
        std::string ip_address;
        while(true) {
            std::cout << "Enter server IP address (or Q to exit): ";
            if (!std::getline(std::cin, ip_address)) {
                std::cerr << "Error reading input!" << std::endl;
                break;
            }

            if (ip_address.empty()) {
                std::cout << "You need to enter an IP address or 'Q' to exit." << std::endl;
                continue;
            } else if (ip_address == "Q") {
                break;
            }

            boost::system::error_code ec;
            auto endpoint = udp::endpoint(net::ip::make_address(ip_address, ec), port);

            auto rec_result = recorder.Record(max_buffer_size, 1.5s);

            size_t total_bytes = rec_result.frames * recorder.GetFrameSize();
            socket.send_to(net::buffer(rec_result.data, total_bytes), endpoint);
        }
    } catch (std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}
