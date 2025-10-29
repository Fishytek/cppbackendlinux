#pragma once
#include "model.h"
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <algorithm> 

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;
using string_view = std::string_view;

class RequestHandler {
public:
    // Константы для эндпоинтов
    static constexpr string_view MAPS_LIST_ENDPOINT = "/api/v1/maps";
    static constexpr string_view MAP_BY_ID_ENDPOINT_PREFIX = "/api/v1/maps/";
    static constexpr string_view JOIN_GAME_ENDPOINT = "/api/v1/game/join";
    static constexpr string_view PLAYERS_LIST_ENDPOINT = "/api/v1/game/players";
    static constexpr string_view GAME_STATE_ENDPOINT = "/api/v1/game/state";
    static constexpr string_view API_PREFIX = "/api/";

    explicit RequestHandler(model::Game& game, const std::string& static_path = "") 
        : game_(game), static_path_(static_path) {
        InitializeMimeTypes();
    }

    template <typename Request, typename Send>
    void operator()(Request&& req, Send&& send) {
        // Преобразуем boost::string_view в std::string для сравнения
        std::string target_str(req.target().data(), req.target().size());
        string_view target = target_str;

        // Проверяем, является ли запрос API-запросом
        if (target.find(API_PREFIX) == 0) {
            HandleApiRequest(std::move(req), std::forward<Send>(send));
        } else {
            // Иначе обрабатываем как статический контент
            HandleStaticContent(std::move(req), std::forward<Send>(send));
        }
    }

private:
    model::Game& game_;
    std::string static_path_;
    std::unordered_map<std::string, std::string> mime_types_;

    void InitializeMimeTypes() {
        mime_types_[".html"] = "text/html";
        mime_types_[".htm"] = "text/html";
        mime_types_[".css"] = "text/css";
        mime_types_[".txt"] = "text/plain";
        mime_types_[".js"] = "text/javascript";
        mime_types_[".json"] = "application/json";
        mime_types_[".xml"] = "application/xml";
        mime_types_[".png"] = "image/png";
        mime_types_[".jpg"] = "image/jpeg";
        mime_types_[".jpe"] = "image/jpeg";
        mime_types_[".jpeg"] = "image/jpeg";
        mime_types_[".gif"] = "image/gif";
        mime_types_[".bmp"] = "image/bmp";
        mime_types_[".ico"] = "image/vnd.microsoft.icon";
        mime_types_[".tiff"] = "image/tiff";
        mime_types_[".tif"] = "image/tiff";
        mime_types_[".svg"] = "image/svg+xml";
        mime_types_[".svgz"] = "image/svg+xml";
        mime_types_[".mp3"] = "audio/mpeg";
    }

template <typename Body, typename Allocator, typename Send>
void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    std::string target_str(req.target().data(), req.target().size());
    string_view target = target_str;

    if (target == MAPS_LIST_ENDPOINT && req.method() == http::verb::get) {
        HandleGetMapsList(std::move(req), std::forward<Send>(send));
    } else if (target.find(MAP_BY_ID_ENDPOINT_PREFIX) == 0 && req.method() == http::verb::get) {
        HandleGetMap(std::move(req), std::forward<Send>(send));
    } else if (target == JOIN_GAME_ENDPOINT && req.method() == http::verb::post) {
        HandleJoinGame(std::move(req), std::forward<Send>(send));
    } else {
        // Для неизвестных API endpoint возвращаем 400 (а не 404)
        HandleBadRequest(std::move(req), std::forward<Send>(send), "Bad API request");
    }
}

template <typename Body, typename Allocator, typename Send>
void HandleStaticContent(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    // Проверяем метод - только GET и HEAD разрешены для статики
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        std::cout << "Method not allowed: " << req.method() << std::endl;
        HandleMethodNotAllowed(std::move(req), std::forward<Send>(send));
        return;
    }

    // Декодируем URL
    std::string target_str(req.target().data(), req.target().size());
    std::string decoded_path = UrlDecode(target_str);

    std::cout << "Original target: " << target_str << std::endl;
    std::cout << "Decoded path: " << decoded_path << std::endl;

    // Убираем начальный слэш если есть
    if (!decoded_path.empty() && decoded_path[0] == '/') {
        decoded_path = decoded_path.substr(1);
    }

    // Если путь пустой, используем index.html
    if (decoded_path.empty()) {
        decoded_path = "index.html";
    }

    std::cout << "Final path: " << decoded_path << std::endl;

    // Строим полный путь к файлу
    fs::path file_path = fs::path(static_path_) / decoded_path;

    std::cout << "Static path: " << static_path_ << std::endl;
    std::cout << "Full file path: " << file_path.string() << std::endl;

    // Проверяем, что путь не выходит за пределы корневой директории
    if (!IsPathWithinRoot(file_path, static_path_)) {
        std::cout << "SECURITY CHECK FAILED - returning 400" << std::endl;
        HandleBadRequest(std::move(req), std::forward<Send>(send), "Invalid path");
        return;
    } else {
        std::cout << "Security check passed" << std::endl;
    }

    // Если путь ведет к директории, добавляем index.html
    if (fs::exists(file_path) && fs::is_directory(file_path)) {
        file_path /= "index.html";
        std::cout << "Path is directory, using: " << file_path.string() << std::endl;
    }

    // Проверяем существование файла
    if (!fs::exists(file_path)) {
        std::cout << "FILE NOT EXISTS - returning 404" << std::endl;
        HandleFileNotFound(std::move(req), std::forward<Send>(send));
        return;
    }

    if (!fs::is_regular_file(file_path)) {
        std::cout << "NOT A REGULAR FILE - returning 404" << std::endl;
        HandleFileNotFound(std::move(req), std::forward<Send>(send));
        return;
    }

    std::cout << "File exists and is regular file" << std::endl;

    // Читаем файл
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cout << "CANNOT OPEN FILE - returning 404" << std::endl;
        HandleFileNotFound(std::move(req), std::forward<Send>(send));
        return;
    }
}
    template <typename Body, typename Allocator, typename Send>
void HandleApiNotFound(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    // Для неизвестных API endpoint - 404
    std::string error_json = R"({"code":"badRequest","message":"Bad request"})";
    auto response = MakeResponse(std::move(req), error_json, http::status::not_found);
    send(std::move(response));
}

template <typename Body, typename Allocator, typename Send>
void HandleFileNotFound(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    // Для несуществующих файлов - 404
    http::response<http::string_body> response{http::status::not_found, req.version()};
    response.set(http::field::content_type, "text/plain");
    response.body() = "File Not Found";
    response.prepare_payload();
    response.keep_alive(req.keep_alive());
    send(std::move(response));
}

    std::string UrlDecode(const std::string& encoded) {
        std::string decoded;
        decoded.reserve(encoded.size());
        
        for (size_t i = 0; i < encoded.size(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.size()) {
                int hex_value;
                std::istringstream hex_stream(encoded.substr(i + 1, 2));
                if (hex_stream >> std::hex >> hex_value) {
                    decoded += static_cast<char>(hex_value);
                    i += 2;
                } else {
                    decoded += encoded[i];
                }
            } else if (encoded[i] == '+') {
                decoded += ' ';
            } else {
                decoded += encoded[i];
            }
        }
        
        return decoded;
    }

bool IsPathWithinRoot(const fs::path& path, const std::string& root) {
    try {
        // Простая проверка: если путь содержит "..", считаем небезопасным
        std::string path_str = path.string();
        if (path_str.find("..") != std::string::npos) {
            return false;
        }
        
        // Более надежная проверка через canonical paths
        fs::path abs_root = fs::absolute(root);
        fs::path abs_path = fs::absolute(path);
        
        // Нормализуем пути
        fs::path norm_root = abs_root.lexically_normal();
        fs::path norm_path = abs_path.lexically_normal();
        
        // Преобразуем в строки для сравнения
        std::string root_str = norm_root.string();
        std::string path_str_final = norm_path.string();
        
        // Путь должен начинаться с корневой директории
        bool result = (path_str_final.find(root_str) == 0);
        
        // Отладочный вывод (можно убрать после отладки)
        // std::cout << "Root: " << root_str << std::endl;
        // std::cout << "Path: " << path_str_final << std::endl;
        // std::cout << "Result: " << result << std::endl;
        
        return result;
        
    } catch (const fs::filesystem_error& e) {
        // В случае ошибки файловой системы считаем путь небезопасным
        return false;
    }
}

    std::string GetMimeType(const std::string& extension) {
        std::string ext_lower = extension;
        std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
        
        auto it = mime_types_.find(ext_lower);
        if (it != mime_types_.end()) {
            return it->second;
        }
        
        return "application/octet-stream";
    }

    // API методы (остаются как были)
    template <typename Body, typename Allocator, typename Send>
    void HandleGetMapsList(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        boost::json::array maps_array;

        const auto& maps = game_.GetMaps();
        for (const auto& map : maps) {
            boost::json::object map_obj;
            map_obj["id"] = *map.GetId();
            map_obj["name"] = map.GetName();

            maps_array.push_back(std::move(map_obj));
        }

        std::string json = boost::json::serialize(maps_array);

        auto response = MakeResponse(std::move(req), json, http::status::ok);
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleGetMap(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto target = req.target();
        std::string target_str(target.data(), target.size());
        std::string map_id_str = target_str.substr(MAP_BY_ID_ENDPOINT_PREFIX.length());

        model::Map::Id map_id(map_id_str);
        const auto* map = game_.FindMap(map_id);

        if (!map) {
            std::string error_json = R"({"code":"mapNotFound","message":"Map not found"})";
            auto response = MakeResponse(std::move(req), error_json, http::status::not_found);
            send(std::move(response));
            return;
        }

        // ... остальная логика HandleGetMap (как у вас было)
        std::string json = "{";
        json += "\"id\":\"" + std::string(*map->GetId()) + "\",";
        json += "\"name\":\"" + map->GetName() + "\",";

        // Roads
        json += "\"roads\":[";
        const auto& roads = map->GetRoads();
        for (size_t i = 0; i < roads.size(); ++i) {
            const auto& road = roads[i];
            if (road.IsHorizontal()) {
                json += "{\"x0\":" + std::to_string(road.GetStart().x) + 
                        ",\"y0\":" + std::to_string(road.GetStart().y) + 
                        ",\"x1\":" + std::to_string(road.GetEnd().x) + "}";
            } else {
                json += "{\"x0\":" + std::to_string(road.GetStart().x) + 
                        ",\"y0\":" + std::to_string(road.GetStart().y) + 
                        ",\"y1\":" + std::to_string(road.GetEnd().y) + "}";
            }
            if (i != roads.size() - 1) {
                json += ",";
            }
        }
        json += "],";

        // Buildings
        json += "\"buildings\":[";
        const auto& buildings = map->GetBuildings();
        for (size_t i = 0; i < buildings.size(); ++i) {
            const auto& bounds = buildings[i].GetBounds();
            json += "{\"x\":" + std::to_string(bounds.position.x) + 
                    ",\"y\":" + std::to_string(bounds.position.y) + 
                    ",\"w\":" + std::to_string(bounds.size.width) + 
                    ",\"h\":" + std::to_string(bounds.size.height) + "}";
            if (i != buildings.size() - 1) {
                json += ",";
            }
        }
        json += "],";

        // Offices
        json += "\"offices\":[";
        const auto& offices = map->GetOffices();
        for (size_t i = 0; i < offices.size(); ++i) {
            json += "{\"id\":\"" + std::string(*offices[i].GetId()) + 
                    "\",\"x\":" + std::to_string(offices[i].GetPosition().x) + 
                    ",\"y\":" + std::to_string(offices[i].GetPosition().y) + 
                    ",\"offsetX\":" + std::to_string(offices[i].GetOffset().dx) + 
                    ",\"offsetY\":" + std::to_string(offices[i].GetOffset().dy) + "}";
            if (i != offices.size() - 1) {
                json += ",";
            }
        }
        json += "]";

        json += "}";

        auto response = MakeResponse(std::move(req), json, http::status::ok);
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoinGame(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string error_json = R"({"code":"notImplemented","message":"Join game not implemented"})";
        auto response = MakeResponse(std::move(req), error_json, http::status::not_implemented);
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleMethodNotAllowed(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        http::response<http::string_body> response{http::status::method_not_allowed, req.version()};
        response.set(http::field::content_type, "text/plain");
        response.body() = "Method Not Allowed";
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
void HandleBadRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, 
                     const std::string& message = "Bad request") {
    // Для API bad request возвращаем JSON
    std::string target_str(req.target().data(), req.target().size());
    string_view target = target_str;
    
    if (target.find(API_PREFIX) == 0) {
        // API запросы - возвращаем JSON
        std::string error_json = R"({"code":"badRequest","message":")" + message + "\"}";
        auto response = MakeResponse(std::move(req), error_json, http::status::bad_request);
        send(std::move(response));
    } else {
        // Статические запросы - возвращаем plain text
        http::response<http::string_body> response{http::status::bad_request, req.version()};
        response.set(http::field::content_type, "text/plain");
        response.body() = message;
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }
}

    template <typename Body, typename Allocator, typename Send>
    void HandleNotFound(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        http::response<http::string_body> response{http::status::not_found, req.version()};
        response.set(http::field::content_type, "text/plain");
        response.body() = "File Not Found";
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> MakeResponse(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        const std::string& data,
        http::status status) {

        http::response<http::string_body> response{status, req.version()};
        response.set(http::field::content_type, "application/json");
        response.body() = data;
        response.prepare_payload();
        response.keep_alive(req.keep_alive());

        return response;
    }
};

}  // namespace http_handler