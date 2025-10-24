#include "json_loader.h"
#include <fstream>
#include <boost/json.hpp>

namespace json_loader {

// Объявляем пространство имен для Boost.JSON
using namespace boost;

model::Game LoadGame(const std::filesystem::path& json_path) {
    // 1. Открываем файл
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open json_file: " + json_path.string());
    }
    
    // 2. Читаем содержимое файла
    std::string content((std::istreambuf_iterator<char>(file)), 
                       std::istreambuf_iterator<char>());
    file.close();

    // 3. Парсим JSON
    auto value = json::parse(content);
    auto& root = value.as_object();
    auto& maps_array = root.at("maps").as_array();
    
    model::Game game;

    // 4. Обрабатываем каждую карту
    for (auto& map_value : maps_array) {
        auto& map_obj = map_value.as_object();
        
        // 4.1 Создаем карту
        auto id = model::Map::Id(std::string(map_obj.at("id").as_string()));
        auto name = std::string(map_obj.at("name").as_string());
        model::Map map(std::move(id), std::move(name));

        // 4.2 Добавляем дороги
        auto& roads_array = map_obj.at("roads").as_array();
        for (auto& road_value : roads_array) {
            auto& road_obj = road_value.as_object();
            
            int x0 = road_obj.at("x0").as_int64();
            int y0 = road_obj.at("y0").as_int64();
            
            if (road_obj.contains("x1")) {
                // Горизонтальная дорога
                int x1 = road_obj.at("x1").as_int64();
                map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, x1));
            } else if (road_obj.contains("y1")) {
                // Вертикальная дорога
                int y1 = road_obj.at("y1").as_int64();
                map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, y1));
            }
        }

        // 4.3 Добавляем здания
        if (map_obj.contains("buildings")) {
            auto& buildings_array = map_obj.at("buildings").as_array();
            for (auto& building_value : buildings_array) {
                auto& building_obj = building_value.as_object();
                
                int x = building_obj.at("x").as_int64();
                int y = building_obj.at("y").as_int64();
                int w = building_obj.at("w").as_int64();
                int h = building_obj.at("h").as_int64();
                
                model::Building building({{x, y}, {w, h}});
                map.AddBuilding(building);
            }
        }

        // 4.4 Добавляем офисы
        if (map_obj.contains("offices")) {
            auto& offices_array = map_obj.at("offices").as_array();
            for (auto& office_value : offices_array) {
                auto& office_obj = office_value.as_object();
                
                auto office_id = model::Office::Id(std::string(office_obj.at("id").as_string()));
                int x = office_obj.at("x").as_int64();
                int y = office_obj.at("y").as_int64();
                int offsetX = office_obj.at("offsetX").as_int64();
                int offsetY = office_obj.at("offsetY").as_int64();
                
                model::Office office(std::move(office_id), {x, y}, {offsetX, offsetY});
                map.AddOffice(std::move(office));
            }
        }

        // 4.5 Добавляем карту в игру
        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader