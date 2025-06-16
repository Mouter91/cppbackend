#include <boost/asio.hpp>
#include <array>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <string>
#include <string_view>

namespace net = boost::asio;
using net::ip::udp;

using namespace std::literals;

int main() {
    static const int port = 3333;
    static const size_t max_buffer_size = 1024;

    try {
        net::io_context io_context;
        udp::socket socket(io_context, udp::endpoint(udp::v4(), port));

        for(;;) {
            std::array<char, max_buffer_size> recv_buf;
            udp::endpoint remote_endpoint;
            
            auto size = socket.receive_from(net::buffer(recv_buf), remote_endpoint);

            std::cout << "Client said"sv << std::string_view(recv_buf.data(), size) << std::endl;

            boost::system::error_code ignored_error;
            socket.send_to(net::buffer("Hello from UDP-server"sv), remote_endpoint, 0, ignored_error);   
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
