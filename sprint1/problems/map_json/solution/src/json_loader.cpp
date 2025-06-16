#include "json_loader.h"
#include <fstream>
#include <boost/json.hpp>

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }

    std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    auto json_value = boost::json::parse(json_content);
    auto json_obj = json_value.as_object();

    model::Game game;

    if (json_obj.contains("maps")) {
        for (const auto& map_json : json_obj["maps"].as_array()) {
            auto map_obj = map_json.as_object();
            std::string id = map_obj["id"].as_string().c_str();
            std::string name = map_obj["name"].as_string().c_str();

            model::Map game_map{model::Map::Id(id), name};

            if (map_obj.contains("roads")) {
                for (const auto& road_json : map_obj["roads"].as_array()) {
                    auto road_obj = road_json.as_object();
                    int x0 = road_obj["x0"].as_int64();
                    int y0 = road_obj["y0"].as_int64();
                    if (road_obj.contains("x1")) {
                        int x1 = road_obj["x1"].as_int64();
                        game_map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, x1));
                    } else {
                        int y1 = road_obj["y1"].as_int64();
                        game_map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, y1));
                    }
                }
            }

            if (map_obj.contains("buildings")) {
                for (const auto& building_json : map_obj["buildings"].as_array()) {
                    auto building_obj = building_json.as_object();
                    int x = building_obj["x"].as_int64();
                    int y = building_obj["y"].as_int64();
                    int w = building_obj["w"].as_int64();
                    int h = building_obj["h"].as_int64();

                    game_map.AddBuilding(model::Building({{x, y}, {w, h}}));
                }
            }

            if (map_obj.contains("offices")) {
                for (const auto& office_json : map_obj["offices"].as_array()) {
                    auto office_obj = office_json.as_object();
                    std::string office_id = office_obj["id"].as_string().c_str();
                    int x = office_obj["x"].as_int64();
                    int y = office_obj["y"].as_int64();
                    int offsetX = office_obj["offsetX"].as_int64();
                    int offsetY = office_obj["offsetY"].as_int64();

                    game_map.AddOffice(model::Office(model::Office::Id(office_id), {x, y}, {offsetX, offsetY}));
                }
            }

            game.AddMap(std::move(game_map));
        }
    }

    return game;
}

} // namespace json_loader

