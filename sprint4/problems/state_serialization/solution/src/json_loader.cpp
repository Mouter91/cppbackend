#include "json_loader.h"
#include <fstream>
#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;
using namespace json_keys;

boost::json::value ParseJsonFromFile(const std::filesystem::path& json_path) {
  std::ifstream file(json_path);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + json_path.string());
  }
  std::string json_content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
  return json::parse(json_content);
}

model::Map ParseMap(const json::object& map_obj, double default_speed, int default_bag_capacity) {
  std::string id = map_obj.at(ID).as_string().c_str();
  std::string name = map_obj.at(NAME).as_string().c_str();
  model::Map game_map{model::Map::Id(id), name};

  // Устанавливаем скорость для карты
  if (map_obj.contains(MAP_DOG_SPEED)) {
    game_map.SetDefaultDogSpeed(map_obj.at(MAP_DOG_SPEED).as_double());
  } else {
    game_map.SetDefaultDogSpeed(default_speed);
  }

  if (map_obj.contains(ROADS)) {
    ParseRoads(game_map, map_obj.at(ROADS).as_array());
  }
  if (map_obj.contains(BUILDINGS)) {
    ParseBuildings(game_map, map_obj.at(BUILDINGS).as_array());
  }
  if (map_obj.contains(OFFICES)) {
    ParseOffices(game_map, map_obj.at(OFFICES).as_array());
  }

  if (map_obj.contains(BAG_CAPACITY)) {
    game_map.SetBagCapacity(map_obj.at(BAG_CAPACITY).as_int64());
  } else {
    game_map.SetBagCapacity(default_bag_capacity);
  }
  return game_map;
}

void ParseRoads(model::Map& game_map, const json::array& roads) {
  for (const auto& road_json : roads) {
    auto road_obj = road_json.as_object();
    int x0 = road_obj.at(X0).as_int64();
    int y0 = road_obj.at(Y0).as_int64();
    if (road_obj.contains(X1)) {
      int x1 = road_obj.at(X1).as_int64();
      game_map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, x1));
    } else {
      int y1 = road_obj.at(Y1).as_int64();
      game_map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, y1));
    }
  }
}

void ParseBuildings(model::Map& game_map, const json::array& buildings) {
  for (const auto& building_json : buildings) {
    auto building_obj = building_json.as_object();
    int x = building_obj.at(X).as_int64();
    int y = building_obj.at(Y).as_int64();
    int w = building_obj.at(WIDTH).as_int64();
    int h = building_obj.at(HEIGHT).as_int64();
    game_map.AddBuilding(model::Building({{x, y}, {w, h}}));
  }
}

void ParseOffices(model::Map& game_map, const json::array& offices) {
  for (const auto& office_json : offices) {
    auto office_obj = office_json.as_object();
    std::string office_id = office_obj.at(ID).as_string().c_str();
    int x = office_obj.at(X).as_int64();
    int y = office_obj.at(Y).as_int64();
    int offsetX = office_obj.at(OFFSET_X).as_int64();
    int offsetY = office_obj.at(OFFSET_Y).as_int64();
    game_map.AddOffice(model::Office(model::Office::Id(office_id), {x, y}, {offsetX, offsetY}));
  }
}

model::Game LoadGame(const std::filesystem::path& json_path) {
  // Загружаем JSON из файла
  auto json_value = ParseJsonFromFile(json_path);
  auto json_obj = json_value.as_object();
  model::Game game;

  // Устанавливаем глобальную скорость по умолчанию (1.0 если не задано)
  if (json_obj.contains(DEFAULT_DOG_SPEED)) {
    game.SetDefaultDogSpeed(json_obj.at(DEFAULT_DOG_SPEED).as_double());
  }

  if (json_obj.contains(DEFAULT_BAG_CAPACITY)) {
    game.SetDefaultBagCapacity(json_obj.at(DEFAULT_BAG_CAPACITY).as_int64());
  }
  // Парсим карты
  if (json_obj.contains(MAPS)) {
    for (const auto& map_json : json_obj.at(MAPS).as_array()) {
      auto map =
          ParseMap(map_json.as_object(), game.GetDefaultDogSpeed(), game.GetDefaultBagCapacity());

      // Если у карты не установлена своя скорость, используем глобальную
      if (!map.IsDefaultDogSpeedValueConfigured()) {
        map.SetDefaultDogSpeed(game.GetDefaultDogSpeed());
      }

      game.AddMap(std::move(map));
    }
  }

  return game;
}

GamePackage LoadGamePackage(const std::filesystem::path& json_path) {
  auto json_value = ParseJsonFromFile(json_path);
  auto json_obj = json_value.as_object();

  GamePackage result;
  model::Game& game = result.game;

  // Устанавливаем глобальную скорость по умолчанию (1.0 если не задано)
  if (json_obj.contains(DEFAULT_DOG_SPEED)) {
    game.SetDefaultDogSpeed(json_obj.at(DEFAULT_DOG_SPEED).as_double());
  }

  if (json_obj.contains(DEFAULT_BAG_CAPACITY)) {
    game.SetDefaultBagCapacity(json_obj.at(DEFAULT_BAG_CAPACITY).as_int64());
  }
  // Загрузка конфигурации генератора трофеев
  if (json_obj.contains(LOOT_GENERATOR_CONFIG)) {
    auto config = json_obj.at(LOOT_GENERATOR_CONFIG).as_object();

    double period = config.at(PERIOD).as_double();
    game.GetSettings().period = period;

    double probability = config.at(PROBABILITY).as_double();
    game.GetSettings().probability = period;

    game.SetLootGeneratorConfig(period, probability);
  }

  // Загрузка карт
  if (json_obj.contains(MAPS)) {
    for (const auto& map_json : json_obj.at(MAPS).as_array()) {
      auto map_obj = map_json.as_object();
      auto map = ParseMap(map_obj, game.GetDefaultDogSpeed(), game.GetDefaultBagCapacity());

      if (!map.IsDefaultDogSpeedValueConfigured()) {
        map.SetDefaultDogSpeed(game.GetDefaultDogSpeed());
      }

      extra_data::MapsExtra& extra = result.extra_data;

      if (map_obj.contains(LOOT_TYPES)) {
        auto loot_types = map_obj.at(LOOT_TYPES);
        std::vector<int> loot_values;
        loot_values.reserve(loot_types.as_array().size());

        for (const auto& loot_type : loot_types.as_array()) {
          loot_values.push_back(loot_type.as_object().at("value").as_int64());
        }

        extra.AddMapData(map.GetId(), loot_types);
        map.SetLootValues(std::move(loot_values));
      }

      game.AddMap(std::move(map));
    }
  }

  return result;
}
}  // namespace json_loader
