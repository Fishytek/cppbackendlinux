#include "json_loader.h"
#include <fstream>
#include <boost/json.hpp>
#include <iostream>
#include <optional> 

namespace json_loader {

using namespace boost;

namespace {

// Вспомогательные функции для загрузки отдельных объектов
model::Road LoadRoad(const json::object& road_obj) {
    int x0 = road_obj.at("x0").as_int64();
    int y0 = road_obj.at("y0").as_int64();
    
    if (road_obj.contains("x1")) {
        int x1 = road_obj.at("x1").as_int64();
        return model::Road(model::Road::HORIZONTAL, {x0, y0}, x1);
    } else {
        int y1 = road_obj.at("y1").as_int64();
        return model::Road(model::Road::VERTICAL, {x0, y0}, y1);
    }
}

model::Building LoadBuilding(const json::object& building_obj) {
    int x = building_obj.at("x").as_int64();
    int y = building_obj.at("y").as_int64();
    int w = building_obj.at("w").as_int64();
    int h = building_obj.at("h").as_int64();
    
    return model::Building({{x, y}, {w, h}});
}

model::Office LoadOffice(const json::object& office_obj) {
    auto id = model::Office::Id(std::string(office_obj.at("id").as_string()));
    int x = office_obj.at("x").as_int64();
    int y = office_obj.at("y").as_int64();
    int offsetX = office_obj.at("offsetX").as_int64();
    int offsetY = office_obj.at("offsetY").as_int64();
    
    return model::Office(std::move(id), {x, y}, {offsetX, offsetY});
}

// Функции для загрузки массивов объектов
void LoadRoads(model::Map& map, const json::array& roads_array) {
    for (auto& road_value : roads_array) {
        map.AddRoad(LoadRoad(road_value.as_object()));
    }
}

void LoadBuildings(model::Map& map, const json::array& buildings_array) {
    for (auto& building_value : buildings_array) {
        map.AddBuilding(LoadBuilding(building_value.as_object()));
    }
}

void LoadOffices(model::Map& map, const json::array& offices_array) {
    for (auto& office_value : offices_array) {
        map.AddOffice(LoadOffice(office_value.as_object()));
    }
}

// Основная функция загрузки карты
model::Map LoadMap(const json::object& map_obj) {
    auto id = model::Map::Id(std::string(map_obj.at("id").as_string()));
    auto name = std::string(map_obj.at("name").as_string());
    model::Map map(std::move(id), std::move(name));

    // Загружаем дороги (обязательные)
    LoadRoads(map, map_obj.at("roads").as_array());
    
    // Загружаем опциональные объекты
    if (map_obj.contains("buildings")) {
        LoadBuildings(map, map_obj.at("buildings").as_array());
    }
    
    if (map_obj.contains("offices")) {
        LoadOffices(map, map_obj.at("offices").as_array());
    }

    return map;
}

} // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    try {
        // 1. Открываем файл
        std::ifstream file(json_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open json file: " << json_path << std::endl;
            return std::nullopt;
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
            game.AddMap(LoadMap(map_value.as_object()));
        }

        return game;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading game from " << json_path << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

}  // namespace json_loader