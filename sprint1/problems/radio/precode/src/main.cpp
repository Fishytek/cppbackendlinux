#include "audio.h"
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <string_view>


namespace net = boost::asio;
using net::ip::udp;
using namespace std::literals;


void StartServer(uint16_t port){
    try {
        boost::asio::io_context io_context;

        udp::socket socket(io_context, udp::endpoint(udp::v4(), port));

        Player player(ma_format_u8, 1);

        std::cout << "Server listening on UDP port" << std::endl;
        while(true) {
            std::vector<char> buffer(65000 * player.GetFrameSize());
            udp::endpoint remote_endpoint;

            boost::system::error_code ec;
            auto received_bytes = socket.receive_from(boost::asio::buffer(buffer), remote_endpoint);

            if(ec){
                std::cout << "Cannot receive: " << ec.message() << std::endl;
                continue;
            }

            std::cout << "Received " << received_bytes <<  " bytes from " << remote_endpoint.address().to_string() << std::endl; 
            
            size_t frames_received = received_bytes / player.GetFrameSize();

            if(frames_received > 0){
                auto play_duration = std::chrono::duration<double>(frames_received / 44100.0);
                
                std::cout << "Playing " << frames_received << " frames (" 
                          << play_duration.count() << " seconds)..." << std::endl;
                
                player.PlayBuffer(buffer.data(), frames_received, play_duration);
                
                std::cout << "Playing done" << std::endl;
            }
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}


void StartClient(uint16_t port){
    try {
        net::io_context io_context;
        udp::socket socket(io_context, udp::v4());
        std::string server_ip;
        std::cout << "Enter IP: ";
        std::getline(std::cin, server_ip);
        Recorder recorder(ma_format_u8, 1);
        udp::endpoint server_endpoint(net::ip::make_address(server_ip), port);
        std::cout << "Press Enter to record message..." << std::endl;
 while (true) {
            std::string input;
            std::getline(std::cin, input);
            
            std::cout << "Recording..." << std::endl;
            
            auto record_duration = 65000.0 / 44100.0;  // ~1.47 секунд
            auto result = recorder.Record(65000, std::chrono::duration<double>(record_duration));
            
            std::cout << "Recorded " << result.frames << " frames" << std::endl;
            
            if (result.frames > 0) {
                // Вычисляем размер данных для отправки
                size_t data_size = result.frames * recorder.GetFrameSize();
                
                // Отправляем данные серверу
                boost::system::error_code ec;
                socket.send_to(net::buffer(result.data.data(), data_size), server_endpoint, 0, ec);
                
                if (ec) {
                    std::cout << "Send error: " << ec.message() << std::endl;
                } else {
                    std::cout << "Sent " << data_size << " bytes to server" << std::endl;
                }
            }
            
            std::cout << "Press Enter to record again..." << std::endl;
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
   if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <client|server> <port>" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    uint16_t port;
    
    try {
        port = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cout << "Invalid port: " << argv[2] << std::endl;
        return 1;
    }
    
    if (mode == "server") {
        StartServer(port);
    } else if (mode == "client") {
        StartClient(port);
    } else {
        std::cout << "Invalid mode: " << mode << ". Use 'client' or 'server'" << std::endl;
        return 1;
    }
    
    return 0;
}
