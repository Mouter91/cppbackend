#include "json_loader.h"
#include <fstream>
#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;

boost::json::value ParseJsonFromFile(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }
    std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return json::parse(json_content);
}

model::Map ParseMap(const json::object& map_obj) {
    std::string id = map_obj.at("id").as_string().c_str();
    std::string name = map_obj.at("name").as_string().c_str();
    model::Map game_map{model::Map::Id(id), name};

    if (map_obj.contains("roads")) {
        ParseRoads(game_map, map_obj.at("roads").as_array());
    }
    if (map_obj.contains("buildings")) {
        ParseBuildings(game_map, map_obj.at("buildings").as_array());
    }
    if (map_obj.contains("offices")) {
        ParseOffices(game_map, map_obj.at("offices").as_array());
    }

    return game_map;
}

void ParseRoads(model::Map& game_map, const json::array& roads) {
    for (const auto& road_json : roads) {
        auto road_obj = road_json.as_object();
        int x0 = road_obj.at("x0").as_int64();
        int y0 = road_obj.at("y0").as_int64();
        if (road_obj.contains("x1")) {
            int x1 = road_obj.at("x1").as_int64();
            game_map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, x1));
        } else {
            int y1 = road_obj.at("y1").as_int64();
            game_map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, y1));
        }
    }
}

void ParseBuildings(model::Map& game_map, const json::array& buildings) {
    for (const auto& building_json : buildings) {
        auto building_obj = building_json.as_object();
        int x = building_obj.at("x").as_int64();
        int y = building_obj.at("y").as_int64();
        int w = building_obj.at("w").as_int64();
        int h = building_obj.at("h").as_int64();
        game_map.AddBuilding(model::Building({{x, y}, {w, h}}));
    }
}

void ParseOffices(model::Map& game_map, const json::array& offices) {
    for (const auto& office_json : offices) {
        auto office_obj = office_json.as_object();
        std::string office_id = office_obj.at("id").as_string().c_str();
        int x = office_obj.at("x").as_int64();
        int y = office_obj.at("y").as_int64();
        int offsetX = office_obj.at("offsetX").as_int64();
        int offsetY = office_obj.at("offsetY").as_int64();
        game_map.AddOffice(model::Office(model::Office::Id(office_id), {x, y}, {offsetX, offsetY}));
    }
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    auto json_value = ParseJsonFromFile(json_path);
    auto json_obj = json_value.as_object();
    model::Game game;
    
    if (json_obj.contains("maps")) {
        for (const auto& map_json : json_obj.at("maps").as_array()) {
            auto map_obj = map_json.as_object();
            game.AddMap(ParseMap(map_obj));
        }
    }

    return game;
}

} // namespace json_loader

