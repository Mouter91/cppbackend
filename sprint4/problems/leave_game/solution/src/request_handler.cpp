#include "request_handler.h"

namespace http_handler {

// BaseHandler implementation
StringResponse BaseHandler::MakeStringResponse(http::status status, std::string_view body,
                                               unsigned http_version, bool keep_alive,
                                               std::string_view content_type) {
  StringResponse response(status, http_version);
  response.set(http::field::content_type, content_type);
  response.body() = body;
  response.prepare_payload();
  response.keep_alive(keep_alive);
  return response;
}

StringResponse BaseHandler::MakeJsonResponse(http::status status, const json::value& body,
                                             unsigned http_version, bool keep_alive) {
  auto response = MakeStringResponse(status, json::serialize(body), http_version, keep_alive,
                                     ContentType::APP_JSON);
  response.set(http::field::cache_control, "no-cache");
  return response;
}

StringResponse BaseHandler::MakeErrorResponse(
    http::status status, std::string_view code, std::string_view message,
    const std::vector<std::string_view>& allowed_methods) {
  json::value error = {{"code", code}, {"message", message}};
  auto response = MakeJsonResponse(status, error, 11, false);

  if (status == http::status::method_not_allowed && !allowed_methods.empty()) {
    std::string allow_header;
    for (const auto& method : allowed_methods) {
      if (!allow_header.empty())
        allow_header += ", ";
      allow_header += method;
    }
    response.set(http::field::allow, allow_header);
  }

  return response;
}

StringResponse BaseHandler::MakeUnauthorizedError(std::string_view message) {
  return MakeErrorResponse(http::status::unauthorized, "invalidToken", message);
}

StringResponse BaseHandler::MakeBadRequestError(std::string_view message) {
  return MakeErrorResponse(http::status::bad_request, "badRequest", message);
}

StringResponse BaseHandler::MakeMethodNotAllowedError(
    const std::vector<std::string_view>& allowed_methods) {
  return MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method",
                           allowed_methods);
}

std::optional<model::Token> ApiHandler::TryExtractToken(const StringRequest& req) const {
  auto auth_header = req[http::field::authorization];
  if (auth_header.empty()) {
    return std::nullopt;
  }

  const std::string bearer_prefix = "Bearer ";
  if (auth_header.find(bearer_prefix) != 0) {
    return std::nullopt;
  }

  std::string token_str(auth_header.substr(bearer_prefix.size()));
  if (token_str.size() < 32) {
    return std::nullopt;
  }

  return model::Token(token_str);
}

bool ApiHandler::ValidateContentType(const StringRequest& req) const {
  return req[http::field::content_type] == ContentType::APP_JSON;
}

bool ApiHandler::ValidateMoveDirection(const std::string& direction) const {
  return direction == "L" || direction == "R" || direction == "U" || direction == "D" ||
         direction == "";
}

StringResponse ApiHandler::HandleGetMaps(const StringRequest& req) {
  json::array maps_json;
  for (const auto& map : app_.GetGame().GetMaps()) {
    maps_json.push_back({{"id", *map.GetId()}, {"name", map.GetName()}});
  }
  return MakeJsonResponse(http::status::ok, maps_json, req.version(), req.keep_alive());
}

StringResponse ApiHandler::HandleGetMapById(const StringRequest& req,
                                            const model::Map::Id& map_id) {
  const auto* map = app_.GetGame().FindMap(map_id);
  if (!map) {
    return MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found");
  }

  json::object map_json{{"id", *map->GetId()}, {"name", map->GetName()}};

  SerializeRoads(map, map_json);
  SerializeBuildings(map, map_json);
  SerializeOffices(map, map_json);

  map_json["lootTypes"] = app_.GetExtraData().GetLootTypes(map_id);

  return MakeJsonResponse(http::status::ok, map_json, req.version(), req.keep_alive());
}

StringResponse ApiHandler::HandleJoinGame(const StringRequest& req) {
  if (!ValidateContentType(req)) {
    return MakeBadRequestError("Invalid content type");
  }

  try {
    json::value request_body = json::parse(req.body());
    json::object request_obj = request_body.as_object();

    if (!request_obj.contains("userName") || !request_obj.contains("mapId")) {
      return MakeBadRequestError("Missing required fields");
    }

    std::string user_name = json::value_to<std::string>(request_obj.at("userName"));
    std::string map_id = json::value_to<std::string>(request_obj.at("mapId"));

    if (user_name.empty()) {
      return MakeBadRequestError("Invalid name");
    }

    const auto* map = app_.GetGame().FindMap(model::Map::Id(map_id));
    if (!map) {
      return MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found");
    }

    auto session = app_.GetGame().FindGameSession(model::Map::Id(map_id));
    if (!session) {
      return MakeErrorResponse(http::status::internal_server_error, "internalError",
                               "Failed to create game session");
    }

    auto dog = std::make_shared<model::Dog>(user_name);
    dog->SetDefaultDogSpeed(session->GetMapDefaultSpeed());
    auto token = app_.GetPlayers().AddPlayer(dog, session);

    json::object response{{"authToken", *token}, {"playerId", dog->GetId()}};

    return MakeJsonResponse(http::status::ok, response, req.version(), req.keep_alive());

  } catch (const boost::system::system_error&) {
    return MakeBadRequestError("Join game request parse error");
  } catch (const std::exception& ex) {
    return MakeErrorResponse(http::status::internal_server_error, "internalError", ex.what());
  }
}

StringResponse ApiHandler::HandleGetPlayers(const StringRequest& req) {
  return ExecuteAuthorized(req, [this, &req](const model::Player& player) {
    auto game_session = player.GetGameSession();
    json::object players_json;

    for (const auto& [dog_id, dog] : game_session->GetDogs()) {
      players_json[std::to_string(dog_id)] = json::object{{"name", dog->GetName()}};
    }

    return MakeJsonResponse(http::status::ok, players_json, req.version(), req.keep_alive());
  });
}

StringResponse ApiHandler::HandleGetGameState(const StringRequest& req) {
  return ExecuteAuthorized(req, [this, &req](const model::Player& player) {
    auto game_session = player.GetGameSession();
    json::object players_json;
    json::object lost_objects_json;

    // Сериализация игроков
    for (const auto& [dog_id, dog] : game_session->GetDogs()) {
      const auto& state = dog->GetState();

      std::string direction;
      switch (state.direction) {
        case MoveInfo::Direction::NORTH:
          direction = "U";
          break;
        case MoveInfo::Direction::SOUTH:
          direction = "D";
          break;
        case MoveInfo::Direction::WEST:
          direction = "L";
          break;
        case MoveInfo::Direction::EAST:
          direction = "R";
          break;
        default:
          assert(false && "Unexpected direction in MoveInfo::Direction");
      }

      json::array bag_json;
      const auto& bag_items = dog->GetBag().GetItems();
      for (size_t i = 0; i < bag_items.size(); ++i) {
        bag_json.push_back(
            json::object{{"id", static_cast<int>(i)}, {"type", static_cast<int>(bag_items[i])}});
      }

      players_json[std::to_string(dog_id)] = {
          {"pos", json::array{state.position.x, state.position.y}},
          {"speed", json::array{state.speed.x, state.speed.y}},
          {"dir", direction},
          {"bag", bag_json},
          {"score", dog->GetScore()}  // Добавляем счет игрока
      };
    }

    // Сериализация потерянных объектов
    size_t loot_id = 0;
    for (const auto& loot : game_session->GetLoots()) {
      lost_objects_json[std::to_string(loot_id++)] = {
          {"type", loot.type}, {"pos", json::array{loot.position.x, loot.position.y}}};
    }

    json::object response{{"players", players_json}, {"lostObjects", lost_objects_json}};

    return MakeJsonResponse(http::status::ok, response, req.version(), req.keep_alive());
  });
}

StringResponse ApiHandler::HandlePlayerAction(const StringRequest& req) {
  if (!ValidateContentType(req)) {
    return MakeBadRequestError("Invalid content type");
  }

  try {
    json::value request_body = json::parse(req.body());
    json::object request_obj = request_body.as_object();

    std::string move_direction;

    if (request_obj.contains("move")) {
      move_direction = json::value_to<std::string>(request_obj.at("move"));
      if (!ValidateMoveDirection(move_direction)) {
        return MakeBadRequestError("Invalid move direction");
      }
    } else {
      // Если поле "move" отсутствует — интерпретируем как пустое движение (остановка)
      move_direction = "";
    }

    return ExecuteAuthorized(req, [this, req, move_direction](model::Player& player) {
      auto game_session = player.GetGameSession();
      auto dog_id = player.GetDogId();
      auto dogs = game_session->GetDogs();
      auto dog_it = dogs.find(dog_id);

      if (dog_it == dogs.end()) {
        return MakeErrorResponse(http::status::internal_server_error, "internalError",
                                 "Dog not found in game session");
      }

      auto dog = dog_it->second;

      if (!move_direction.empty()) {
        player.SetTryingToMove(true);
        player.SetStopTime(0);  // активность — сброс
      } else {
        player.SetTryingToMove(false);
        if (player.GetStopTime() == 0) {
          player.CheckActivityDog(0);  // запуск отсчёта бездействия
        }
      }

      dog->SetDogDirSpeed(move_direction);

      return MakeJsonResponse(http::status::ok, json::object{}, req.version(), req.keep_alive());
    });

  } catch (const boost::system::system_error&) {
    return MakeBadRequestError("Failed to parse action");
  } catch (const std::exception& ex) {
    return MakeErrorResponse(http::status::internal_server_error, "internalError", ex.what());
  }
}

StringResponse ApiHandler::HandleGameTick(const StringRequest& req) {
  if (app_.GetGame().GetSettings().IsAutoTickEnabled()) {
    return MakeErrorResponse(http::status::bad_request, "badRequest",
                             "Manual ticks are disabled in auto-tick mode");
  }

  if (!ValidateContentType(req)) {
    return MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type");
  }

  try {
    json::value request_body = json::parse(req.body());
    json::object request_obj = request_body.as_object();

    if (!request_obj.contains("timeDelta")) {
      return MakeErrorResponse(http::status::bad_request, "invalidArgument",
                               "timeDelta is required");
    }

    const auto& delta_value = request_obj.at("timeDelta");

    if (!delta_value.is_int64()) {
      return MakeErrorResponse(http::status::bad_request, "invalidArgument",
                               "timeDelta must be an integer");
    }

    int64_t time_delta_ms = delta_value.as_int64();

    if (time_delta_ms <= 0) {
      return MakeErrorResponse(http::status::bad_request, "invalidArgument",
                               "timeDelta must be positive");
    }

    // Вызываем ManualTick вместо прямого вызова game_.Tick()
    app_.ManualTick(std::chrono::milliseconds(time_delta_ms));

    return MakeJsonResponse(http::status::ok, json::object{}, req.version(), req.keep_alive());

  } catch (const boost::system::system_error&) {
    return MakeErrorResponse(http::status::bad_request, "invalidArgument",
                             "Failed to parse tick request JSON");
  } catch (const std::exception& ex) {
    return MakeErrorResponse(http::status::internal_server_error, "internalError", ex.what());
  }
}

StringResponse ApiHandler::HandleGetRecords(const StringRequest& req) {
  using namespace boost::urls;
  using namespace boost::json;

  try {
    auto parsed_url = parse_origin_form(std::string(req.target()));
    if (!parsed_url.has_value()) {
      return MakeJsonResponse(http::status::ok, array{}, req.version(), req.keep_alive());
    }

    int start = 0;
    int maxItems = 100;

    for (auto const& param : parsed_url->params()) {
      if (param.key == "start") {
        try {
          start = boost::lexical_cast<int>(param.value);
        } catch (const boost::bad_lexical_cast&) {
          // оставить значение по умолчанию
        }
      } else if (param.key == "maxItems") {
        try {
          maxItems = boost::lexical_cast<int>(param.value);
          if (maxItems > 100) {
            return MakeBadRequestError("maxItems must be less than or equal to 100");
          }
        } catch (const boost::bad_lexical_cast&) {
          // оставить значение по умолчанию
        }
      }
    }

    auto result = app_.GetDatabase().GetRetiredPlayers(start, maxItems);

    array records;
    for (const auto& row : result) {
      records.emplace_back(object{{"name", row["name"].c_str()},
                                  {"score", row["score"].as<int>()},
                                  {"playTime", row["play_time"].as<double>()}});
    }

    return MakeJsonResponse(http::status::ok, records, req.version(), req.keep_alive());

  } catch (const std::exception& ex) {
    return MakeJsonResponse(http::status::ok, array{}, req.version(), req.keep_alive());
  }
}

void ApiHandler::SerializeRoads(const model::Map* map, json::object& map_json) const {
  json::array roads_json;
  for (const auto& road : map->GetRoads()) {
    const auto& start = road.GetStart();
    const auto& end = road.GetEnd();

    if (road.IsHorizontal()) {
      roads_json.push_back(json::object{{"x0", start.x}, {"y0", start.y}, {"x1", end.x}});
    } else if (road.IsVertical()) {
      roads_json.push_back(json::object{{"x0", start.x}, {"y0", start.y}, {"y1", end.y}});
    }
  }
  map_json["roads"] = std::move(roads_json);
}

void ApiHandler::SerializeBuildings(const model::Map* map, json::object& map_json) const {
  json::array buildings_json;
  for (const auto& building : map->GetBuildings()) {
    const auto& bounds = building.GetBounds();
    buildings_json.push_back(json::object{{"x", bounds.position.x},
                                          {"y", bounds.position.y},
                                          {"w", bounds.size.width},
                                          {"h", bounds.size.height}});
  }
  map_json["buildings"] = std::move(buildings_json);
}

void ApiHandler::SerializeOffices(const model::Map* map, json::object& map_json) const {
  json::array offices_json;
  for (const auto& office : map->GetOffices()) {
    const auto& position = office.GetPosition();
    const auto& offset = office.GetOffset();

    offices_json.push_back(json::object{{"id", *office.GetId()},
                                        {"x", position.x},
                                        {"y", position.y},
                                        {"offsetX", offset.dx},
                                        {"offsetY", offset.dy}});
  }
  map_json["offices"] = std::move(offices_json);
}

// StaticHandler implementation
std::string_view StaticHandler::GetMimeType(std::string_view path) const {
  static const std::unordered_map<std::string_view, std::string_view> mime_types = {
      {".html", "text/html"},        {".htm", "text/html"},
      {".css", "text/css"},          {".js", "application/javascript"},
      {".json", "application/json"}, {".png", "image/png"},
      {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},         {".svg", "image/svg+xml"},
      {".ico", "image/x-icon"},      {".txt", "text/plain"},
  };

  const size_t dot_pos = path.rfind('.');
  if (dot_pos == std::string_view::npos) {
    return "text/plain";
  }

  const std::string_view extension = path.substr(dot_pos);
  auto it = mime_types.find(extension);
  return (it != mime_types.end()) ? it->second : "text/plain";
}

bool StaticHandler::IsPathWithinRoot(const fs::path& path) const {
  auto rel_path = fs::relative(path, static_root_);
  return !rel_path.empty() && rel_path.native()[0] != '.';
}

std::string StaticHandler::UrlDecode(std::string_view str) const {
  std::string result;
  result.reserve(str.size());

  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '%' && i + 2 < str.size()) {
      int value;
      std::istringstream iss(std::string(str.substr(i + 1, 2)));
      if (iss >> std::hex >> value) {
        result += static_cast<char>(value);
        i += 2;
      } else {
        result += str[i];
      }
    } else if (str[i] == '+') {
      result += ' ';
    } else {
      result += str[i];
    }
  }
  return result;
}

}  // namespace http_handler
