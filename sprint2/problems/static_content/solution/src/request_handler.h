#pragma once
#include "model.h"
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <string>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler : public HttpHandler {
public:
    // Константы для эндпоинтов
    static constexpr std::string_view MAPS_LIST_ENDPOINT = "/api/v1/maps";
    static constexpr std::string_view MAP_BY_ID_ENDPOINT_PREFIX = "/api/v1/maps/";
    static constexpr std::string_view JOIN_GAME_ENDPOINT = "/api/v1/game/join";
    static constexpr std::string_view PLAYERS_LIST_ENDPOINT = "/api/v1/game/players";
    static constexpr std::string_view GAME_STATE_ENDPOINT = "/api/v1/game/state";

    explicit RequestHandler(model::Game& game) : game_(game) {
    }

    template <typename Request, typename Send>
    void operator()(Request&& req, Send&& send) {
        auto target = req.target();

        if (target == MAPS_LIST_ENDPOINT) {
            HandleGetMapsList(std::move(req), std::forward<Send>(send));
            return;
        } else if (target.starts_with(MAP_BY_ID_ENDPOINT_PREFIX)) {
            HandleGetMapById(std::move(req), std::forward<Send>(send));
            return;
        } else if (target == JOIN_GAME_ENDPOINT) {
            HandleJoinGame(std::move(req), std::forward<Send>(send));
            return;
        }

    }

private:
    model::Game& game_;

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

        // Преобразуем beast::string_view в std::string
        std::string target_str(target.data(), target.size());
        std::string map_id_str = target_str.substr(13); // "/api/v1/maps/".length() = 13

        model::Map::Id map_id(map_id_str);
        const auto* map = game_.FindMap(map_id);

        if (!map) {
            std::string error_json = R"({"code":"mapNotFound","message":"Map not found"})";
            auto response = MakeResponse(std::move(req), error_json, http::status::not_found);
            send(std::move(response));
            return;
        }

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
    void HandleBadRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string error_json = R"({"code":"badRequest","message":"Bad request"})";
        auto response = MakeResponse(std::move(req), error_json, http::status::bad_request);
        send(std::move(response));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleNotFound(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string error_json = R"({"code":"notFound","message":"Not found"})";
        auto response = MakeResponse(std::move(req), error_json, http::status::not_found);
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
