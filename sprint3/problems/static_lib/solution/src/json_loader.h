// json_loader.h
#pragma once

#include "model.h"
#include "extra_data.h"
#include <filesystem>
#include <boost/json.hpp>

namespace json_loader {

namespace json_keys {
constexpr const char* DEFAULT_DOG_SPEED = "defaultDogSpeed";
constexpr const char* MAP_DOG_SPEED = "dogSpeed";
constexpr const char* MAPS = "maps";
constexpr const char* ID = "id";
constexpr const char* NAME = "name";
constexpr const char* ROADS = "roads";
constexpr const char* BUILDINGS = "buildings";
constexpr const char* OFFICES = "offices";
constexpr const char* X0 = "x0";
constexpr const char* Y0 = "y0";
constexpr const char* X1 = "x1";
constexpr const char* Y1 = "y1";
constexpr const char* X = "x";
constexpr const char* Y = "y";
constexpr const char* WIDTH = "w";
constexpr const char* HEIGHT = "h";
constexpr const char* OFFSET_X = "offsetX";
constexpr const char* OFFSET_Y = "offsetY";

constexpr const char* LOOT_GENERATOR_CONFIG = "lootGeneratorConfig";
constexpr const char* PERIOD = "period";
constexpr const char* PROBABILITY = "probability";
constexpr const char* LOOT_TYPES = "lootTypes";
}  // namespace json_keys

struct GamePackage {
  model::Game game;
  extra_data::MapsExtra extra_data;
};

// Загружает JSON из файла и возвращает разобранный объект
boost::json::value ParseJsonFromFile(const std::filesystem::path& json_path);

// Разбирает карту из JSON-объекта
model::Map ParseMap(const boost::json::object& map_obj, double default_speed);

// Разбирает дороги и добавляет их в карту
void ParseRoads(model::Map& game_map, const boost::json::array& roads);

// Разбирает здания и добавляет их в карту
void ParseBuildings(model::Map& game_map, const boost::json::array& buildings);

// Разбирает офисы и добавляет их в карту
void ParseOffices(model::Map& game_map, const boost::json::array& offices);

// Загружает игру из JSON-файла
model::Game LoadGame(const std::filesystem::path& json_path);
GamePackage LoadGamePackage(const std::filesystem::path& json_path);

}  // namespace json_loader
