#pragma once

#include "model.h"
#include <filesystem>
#include <boost/json.hpp>

namespace json_loader {

// Загружает JSON из файла и возвращает разобранный объект
boost::json::value ParseJsonFromFile(const std::filesystem::path& json_path);

// Разбирает карту из JSON-объекта
model::Map ParseMap(const boost::json::object& map_obj);

// Разбирает дороги и добавляет их в карту
void ParseRoads(model::Map& game_map, const boost::json::array& roads);

// Разбирает здания и добавляет их в карту
void ParseBuildings(model::Map& game_map, const boost::json::array& buildings);

// Разбирает офисы и добавляет их в карту
void ParseOffices(model::Map& game_map, const boost::json::array& offices);

// Загружает игру из JSON-файла
model::Game LoadGame(const std::filesystem::path& json_path);

} // namespace json_loader

