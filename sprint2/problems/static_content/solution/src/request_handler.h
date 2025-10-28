#pragma once
#include "model.h"
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;

// Уберите наследование от HttpHandler, если такого класса нет
class RequestHandler {
public:
    // Константы для эндпоинтов
    static constexpr std::string_view MAPS_LIST_ENDPOINT = "/api/v1/maps";
    static constexpr std::string_view MAP_BY_ID_ENDPOINT_PREFIX = "/api/v1/maps/";
    static constexpr std::string_view JOIN_GAME_ENDPOINT = "/api/v1/game/join";
    static constexpr std::string_view PLAYERS_LIST_ENDPOINT = "/api/v1/game/players";
    static constexpr std::string_view GAME_STATE_ENDPOINT = "/api/v1/game/state";

    explicit RequestHandler(model::Game& game, const std::string& static_files_path = "") 
        : game_(game), static_files_path_(static_files_path) {
    }

    template <typename Request, typename Send>
    void operator()(Request&& req, Send&& send) {
        auto target = req.target();
        // Преобразуем beast::string_view в std::string для сравнения
        std::string target_str(target.data(), target.size());
        
        if (target_str.starts_with("/api/")) {
            HandleApiReq(std::move(req), std::forward<Send>(send));
        } else {
            HandleStaticFile(std::move(req), std::forward<Send>(send));
        }
    }

private:
    model::Game& game_;
    std::string static_files_path_;

    template <typename Request, typename Send>
    void HandleApiReq(Request&& req, Send&& send) {
        auto target = req.target();
        std::string target_str(target.data(), target.size());
        
        if (target_str == MAPS_LIST_ENDPOINT) {
            HandleGetMapsList(std::move(req), std::forward<Send>(send));
            return;
        } else if (target_str.starts_with(MAP_BY_ID_ENDPOINT_PREFIX)) {
            HandleGetMap(std::move(req), std::forward<Send>(send));
            return;
        } else if (target_str == JOIN_GAME_ENDPOINT) {
            HandleJoinGame(std::move(req), std::forward<Send>(send));
            return;
        } else {
            HandleNotFound(std::move(req), std::forward<Send>(send));
        }
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleGetMapsList(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        json::array maps_array;
        
        const auto& maps = game_.GetMaps();
        for (const auto& map : maps) {
            json::object map_obj;
            map_obj["id"] = *map.GetId();
            map_obj["name"] = map.GetName();
            
            maps_array.push_back(std::move(map_obj));
        }
        
        std::string json_str = json::serialize(maps_array);
        
        auto response = MakeResponse(std::move(req), json_str, http::status::ok);
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleGetMap(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto target = req.target();
        
        // Преобразуем beast::string_view в std::string
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
        
        // Используем boost::json для построения JSON
        json::object map_json;
        map_json["id"] = *map->GetId();
        map_json["name"] = map->GetName();
        
        // Roads
        json::array roads_array;
        for (const auto& road : map->GetRoads()) {
            json::object road_obj;
            road_obj["x0"] = road.GetStart().x;
            road_obj["y0"] = road.GetStart().y;
            if (road.IsHorizontal()) {
                road_obj["x1"] = road.GetEnd().x;
            } else {
                road_obj["y1"] = road.GetEnd().y;
            }
            roads_array.push_back(std::move(road_obj));
        }
        map_json["roads"] = std::move(roads_array);
        
        // Buildings
        json::array buildings_array;
        for (const auto& building : map->GetBuildings()) {
            const auto& bounds = building.GetBounds();
            json::object building_obj;
            building_obj["x"] = bounds.position.x;
            building_obj["y"] = bounds.position.y;
            building_obj["w"] = bounds.size.width;
            building_obj["h"] = bounds.size.height;
            buildings_array.push_back(std::move(building_obj));
        }
        map_json["buildings"] = std::move(buildings_array);
        
        // Offices
        json::array offices_array;
        for (const auto& office : map->GetOffices()) {
            json::object office_obj;
            office_obj["id"] = *office.GetId();
            office_obj["x"] = office.GetPosition().x;
            office_obj["y"] = office.GetPosition().y;
            office_obj["offsetX"] = office.GetOffset().dx;
            office_obj["offsetY"] = office.GetOffset().dy;
            offices_array.push_back(std::move(office_obj));
        }
        map_json["offices"] = std::move(offices_array);
        
        std::string json_str = json::serialize(map_json);
        auto response = MakeResponse(std::move(req), json_str, http::status::ok);
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoinGame(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string error_json = R"({"code":"notImplemented","message":"Join game not implemented"})";
        auto response = MakeResponse(std::move(req), error_json, http::status::not_implemented);
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleNotFound(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string error_json = R"({"code":"notFound","message":"Not found"})";
        auto response = MakeResponse(std::move(req), error_json, http::status::not_found);
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleStaticFile(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Поддерживаем только GET и HEAD запросы для статических файлов
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto response = MakeErrorResponse("Method not allowed", http::status::method_not_allowed);
            send(std::move(response));
            return;
        }

        // URL декодирование
        std::string decoded_path = UrlDecode(std::string(req.target()));
        
        // Проверка безопасности пути
        if (!IsPathSafe(decoded_path)) {
            auto response = MakeErrorResponse("Bad request", http::status::bad_request);
            send(std::move(response));
            return;
        }
        
        // Построение полного пути к файлу
        fs::path file_path = BuildFilePath(decoded_path);
        
        // Если путь ведет к директории, ищем index.html
        if (fs::is_directory(file_path)) {
            file_path /= "index.html";
        }
        
        // Проверка существования файла
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            auto response = MakeErrorResponse("File not found", http::status::not_found);
            send(std::move(response));
            return;
        }
        
        // Отправка файла
        auto response = MakeFileResponse(file_path, req.version(), req.keep_alive());
        
        // Для HEAD запроса убираем тело
        if (req.method() == http::verb::head) {
            response.body() = {};
        }
        
        send(std::move(response));
    }

    // URL декодирование
    std::string UrlDecode(std::string url) {
        std::string result;
        result.reserve(url.size());
        
        for (size_t i = 0; i < url.size(); ++i) {
            if (url[i] == '%' && i + 2 < url.size()) {
                int hex_value;
                std::string hex_str = url.substr(i + 1, 2);
                try {
                    hex_value = std::stoi(hex_str, nullptr, 16);
                    result += static_cast<char>(hex_value);
                    i += 2;
                } catch (...) {
                    result += url[i];
                }
            } else if (url[i] == '+') {
                result += ' ';
            } else {
                result += url[i];
            }
        }
        
        return result;
    }
    
    // Проверка безопасности пути
    bool IsPathSafe(const std::string& path) {
        try {
            fs::path requested_path(path);
            fs::path full_path = fs::weakly_canonical(fs::path(static_files_path_) / requested_path);
            fs::path root_path = fs::weakly_canonical(fs::path(static_files_path_));
            
            // Проверяем, что полный путь начинается с корневого пути
            auto root_it = root_path.begin();
            auto full_it = full_path.begin();
            
            while (root_it != root_path.end() && full_it != full_path.end()) {
                if (*root_it != *full_it) {
                    return false;
                }
                ++root_it;
                ++full_it;
            }
            
            return root_it == root_path.end();
        } catch (...) {
            return false;
        }
    }
    
    // Построение полного пути к файлу
    fs::path BuildFilePath(const std::string& decoded_path) {
        fs::path relative_path(decoded_path);
        
        // Убираем начальный слеш если есть
        if (relative_path.has_root_directory()) {
            relative_path = relative_path.relative_path();
        }
        
        return fs::path(static_files_path_) / relative_path;
    }
    
    // Определение MIME типа
    std::string GetMimeType(const std::string& file_path) {
        fs::path path(file_path);
        std::string extension = path.extension().string();
        
        // Приводим к нижнему регистру
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        
        static const std::unordered_map<std::string, std::string> mime_types = {
            {".html", "text/html"},
            {".htm", "text/html"},
            {".css", "text/css"},
            {".txt", "text/plain"},
            {".js", "text/javascript"},
            {".json", "application/json"},
            {".xml", "application/xml"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".jpe", "image/jpeg"},
            {".gif", "image/gif"},
            {".bmp", "image/bmp"},
            {".ico", "image/vnd.microsoft.icon"},
            {".tiff", "image/tiff"},
            {".tif", "image/tiff"},
            {".svg", "image/svg+xml"},
            {".svgz", "image/svg+xml"},
            {".mp3", "audio/mpeg"}
        };
        
        auto it = mime_types.find(extension);
        if (it != mime_types.end()) {
            return it->second;
        }
        
        return "application/octet-stream";
    }

    // Метод для создания ошибок
    http::response<http::string_body> MakeErrorResponse(
        const std::string& message,
        http::status status) {
        
        http::response<http::string_body> response{status, 11};
        response.set(http::field::content_type, "text/plain");
        response.body() = message;
        response.prepare_payload();
        
        return response;
    }

    // Метод для файловых ответов
    http::response<http::file_body> MakeFileResponse(
        const fs::path& file_path,
        unsigned version,
        bool keep_alive) {
        
        http::response<http::file_body> response;
        response.version(version);
        response.keep_alive(keep_alive);
        
        // Открываем файл
        boost::system::error_code ec;
        http::file_body::value_type file;
        file.open(file_path.string().c_str(), beast::file_mode::read, ec);
        
        if (ec) {
            // Если не удалось открыть файл, возвращаем ошибку
            response.result(http::status::internal_server_error);
            return response;
        }
        
        // Устанавливаем содержимое файла
        response.body() = std::move(file);
        
        // Устанавливаем заголовки
        response.set(http::field::content_type, GetMimeType(file_path.string()));
        response.content_length(response.body().size());
        response.result(http::status::ok);
        
        return response;
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