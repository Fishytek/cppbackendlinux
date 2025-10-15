#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "seabattle.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;

    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);

    if (ec) {
        return std::nullopt;
    }

    return {{buf.data(), sz}};
}

static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;

    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);

    return !ec;
}

class SeabattleAgent {
public:
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {
    }

    void StartGame(tcp::socket& socket, bool my_initiative) {
        bool my_turn = my_initiative;
        
        while(!IsGameEnded()){
            PrintFields();
            if(my_turn){
                std::cout << "Your turn: ";
                auto move = GetPlayerMove();
                if(!move){
                    std::cout << "Invalid move! Try again." << std::endl;
                    continue;
                }
                std::string move_str = MoveToString(*move);
                if(!WriteExact(socket, move_str)){
                    std::cout << "Connection error!" << std::endl;
                    break; 
                }
                auto result_opt = ReadExact<1>(socket);
                if(!result_opt){
                    std::cout << "Connection error!" << std::endl;
                    break; 
                }
                char result_char = (*result_opt)[0];
                auto result = static_cast<SeabattleField::ShotResult>(result_char);
                ProcessOurShot(*move, result);

                if (result == SeabattleField::ShotResult::MISS) {
                    my_turn = false;
                    std::cout << "Miss! Opponent's turn." << std::endl;
                } else {
                    std::cout << "Good shot! Shoot again!" << std::endl;
                }
                
            } else {
                // Ход соперника
                std::cout << "Opponent's turn..." << std::endl;
                
                auto move_opt = ReadExact<2>(socket);
                if (!move_opt) {
                    std::cout << "Connection error!" << std::endl;
                    break;
                }
                
                auto move = ParseMove(*move_opt);
                if (!move) {
                    std::cout << "Invalid move from opponent!" << std::endl;
                    break;
                }
                
                std::cout << "Opponent shoots at: " << MoveToString(*move) << std::endl;
                
                auto result = my_field_.Shoot(move->first, move->second);
                
                char result_char = static_cast<char>(result);
                if (!WriteExact(socket, std::string_view(&result_char, 1))) {
                    std::cout << "Connection error!" << std::endl;
                    break;
                }
                
                if (result == SeabattleField::ShotResult::MISS) {
                    my_turn = true;
                    std::cout << "Opponent missed! Your turn." << std::endl;
                } else {
                    std::cout << "Opponent hit your ship!" << std::endl;
                }
            }
        }
        
        // Конец игры
        PrintFields();
        if (my_field_.IsLoser()) {
            std::cout << "YOU LOSE!" << std::endl;
        } else {
            std::cout << "YOU WIN!" << std::endl;
        }
    }

private:
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;

        int p1 = sv[0] - 'A', p2 = sv[1] - '1';

        if (p1 < 0 || p1 >= SeabattleField::field_size) return std::nullopt;
        if (p2 < 0 || p2 >= SeabattleField::field_size) return std::nullopt;

        return {{p1, p2}};
    }

    static std::string MoveToString(std::pair<int, int> move) {
        char letter = static_cast<char>('A' + move.first);
        char number = static_cast<char>('1' + move.second);
        return std::string{letter, number};
    }

    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    std::optional<std::pair<int, int>> GetPlayerMove() {
        while (true) {
            std::cout << "Enter your move";
            std::string input;
            std::getline(std::cin, input);
            
            // Убираем пробелы
            input.erase(std::remove_if(input.begin(), input.end(), ::isspace), input.end());
            
            auto move = ParseMove(input);
            if (!move) {
                std::cout << "Invalid format! Use format like A1, B5, etc." << std::endl;
                continue;
            }
            
            // Проверяем, что по этой клетке еще не стреляли
            auto cell_state = other_field_(move->first, move->second);
            if (cell_state != SeabattleField::State::UNKNOWN) {
                std::cout << "You already shot there! Choose another cell." << std::endl;
                continue;
            }
            
            return move;
        }
    }
    
    void ProcessOurShot(std::pair<int, int> move, SeabattleField::ShotResult result) {
        switch (result) {
            case SeabattleField::ShotResult::MISS:
                other_field_.MarkMiss(move.first, move.second);
                std::cout << "Miss!" << std::endl;
                break;
            case SeabattleField::ShotResult::HIT:
                other_field_.MarkHit(move.first, move.second);
                std::cout << "Hit!" << std::endl;
                break;
            case SeabattleField::ShotResult::KILL:
                other_field_.MarkKill(move.first, move.second);
                std::cout << "Kill!" << std::endl;
                break;
        }
    }

private:
    SeabattleField my_field_;
    SeabattleField other_field_{SeabattleField::State::UNKNOWN};
};


void StartServer(const SeabattleField& field, unsigned short port) {
    try {
        net::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
        
        std::cout << "Server waiting for connection on port " << port << std::endl;
        
        tcp::socket socket(io_context);
        acceptor.accept(socket);
        
        std::cout << "Client connected!" << std::endl;
        
        SeabattleAgent agent(field);
        agent.StartGame(socket, false);  // Сервер ходит вторым
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
}

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    try {
        net::io_context io_context;
        
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(ip_str, std::to_string(port));
        
        tcp::socket socket(io_context);
        net::connect(socket, endpoints);
        
        std::cout << "Connected to server!" << std::endl;
        
        SeabattleAgent agent(field);
        agent.StartGame(socket, true);  // Клиент ходит первым
        
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

int main(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: program <seed> [<ip>] <port>" << std::endl;
        return 1;
    }

    std::mt19937 engine(std::stoi(argv[1]));
    SeabattleField fieldL = SeabattleField::GetRandomField(engine);

    if (argc == 3) {
        StartServer(fieldL, std::stoi(argv[2]));
    } else if (argc == 4) {
        StartClient(fieldL, argv[2], std::stoi(argv[3]));
    }
}
