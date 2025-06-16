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
    auto response = MakeStringResponse(status, json::serialize(body), 
                                  http_version, keep_alive, ContentType::APP_JSON);
    response.set(http::field::cache_control, "no-cache");
    return response;
}

StringResponse BaseHandler::MakeErrorResponse(http::status status, std::string_view code, 
                                           std::string_view message,
                                           std::initializer_list<std::string_view> allowed_methods) {
    json::value error = {{"code", code}, {"message", message}};
    auto response = MakeJsonResponse(status, error, 11, false);
    
    if (status == http::status::method_not_allowed && allowed_methods.size() > 0) {
        std::string allow_header;
        for (const auto& method : allowed_methods) {
            if (!allow_header.empty()) allow_header += ", ";
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

StringResponse BaseHandler::MakeMethodNotAllowedError(std::initializer_list<std::string_view> allowed_methods) {
    return MakeErrorResponse(http::status::method_not_allowed, 
                           "invalidMethod", 
                           "Invalid method",
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
    return direction == "L" || direction == "R" || 
           direction == "U" || direction == "D" || 
           direction == "";
}

StringResponse ApiHandler::HandleGetMaps(const StringRequest& req) {
    json::array maps_json;
    for (const auto& map : game_.GetMaps()) {
        maps_json.push_back({
            {"id", *map.GetId()},
            {"name", map.GetName()}
        });
    }
    return MakeJsonResponse(http::status::ok, maps_json, req.version(), req.keep_alive());
}

StringResponse ApiHandler::HandleGetMapById(const StringRequest& req, const model::Map::Id& map_id) {
    const auto* map = game_.FindMap(map_id);
    if (!map) {
        return MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found");
    }

    json::object map_json{
        {"id", *map->GetId()},
        {"name", map->GetName()}
    };

    SerializeRoads(map, map_json);
    SerializeBuildings(map, map_json);
    SerializeOffices(map, map_json);

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

        const auto* map = game_.FindMap(model::Map::Id(map_id));
        if (!map) {
            return MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found");
        }

        auto session = game_.FindGameSession(model::Map::Id(map_id));
        if (!session) {
            return MakeErrorResponse(http::status::internal_server_error, "internalError", "Failed to create game session");
        }

        auto dog = std::make_shared<model::Dog>(user_name);
        auto token = players_.AddPlayer(dog, session);

        json::object response{
            {"authToken", *token},
            {"playerId", dog->GetId()}
        };

        return MakeJsonResponse(http::status::ok, response, req.version(), req.keep_alive());

    } catch (const json::system_error&) {
        return MakeBadRequestError("Join game request parse error");
    } catch (...) {
        return MakeErrorResponse(http::status::internal_server_error, "internalError", "Internal server error");
    }
}

StringResponse ApiHandler::HandleGetPlayers(const StringRequest& req) {
    auto token = TryExtractToken(req);
    if (!token) {
        return MakeUnauthorizedError();
    }

    auto player = players_.GetPlayerByToken(*token);
    if (!player) {
        return MakeUnauthorizedError("Player token has not been found");
    }

    auto game_session = player->GetGameSession();
    json::object players_json;
    
    for (const auto& [dog_id, dog] : game_session->GetDogs()) {
        players_json[std::to_string(dog_id)] = json::object{{"name", dog->GetName()}};
    }

    return MakeJsonResponse(http::status::ok, players_json, req.version(), req.keep_alive());
}

StringResponse ApiHandler::HandleGetGameState(const StringRequest& req) {
    auto token = TryExtractToken(req);
    if (!token) {
        return MakeUnauthorizedError();
    }

    auto player = players_.GetPlayerByToken(*token);
    if (!player) {
        return MakeUnauthorizedError("Player token has not been found");
    }

    auto game_session = player->GetGameSession();
    json::object players_json;
    
    for (const auto& [dog_id, dog] : game_session->GetDogs()) {
        const auto& state = dog->GetState();
        
        std::string direction;
        switch (state.direction) {
            case MoveInfo::Direction::NORTH: direction = "U"; break;
            case MoveInfo::Direction::SOUTH: direction = "D"; break;
            case MoveInfo::Direction::WEST: direction = "L"; break;
            case MoveInfo::Direction::EAST: direction = "R"; break;
        }

        players_json[std::to_string(dog_id)] = json::object{
            {"pos", json::array{state.position.x, state.position.y}},
            {"speed", json::array{state.speed.x, state.speed.y}},
            {"dir", direction}
        };
    }

    json::object response{
        {"players", players_json}
    };

    return MakeJsonResponse(http::status::ok, response, req.version(), req.keep_alive());
}

StringResponse ApiHandler::HandlePlayerAction(const StringRequest& req) {
    if (!ValidateContentType(req)) {
        return MakeBadRequestError("Invalid content type");
    }

    auto token = TryExtractToken(req);
    if (!token) {
        return MakeUnauthorizedError();
    }

    try {
        json::value request_body = json::parse(req.body());
        json::object request_obj = request_body.as_object();

        if (!request_obj.contains("move")) {
            return MakeBadRequestError("Move direction is required");
        }

        std::string move_direction = json::value_to<std::string>(request_obj.at("move"));
        if (!ValidateMoveDirection(move_direction)) {
            return MakeBadRequestError("Invalid move direction");
        }

        auto player = players_.GetPlayerByToken(*token);
        if (!player) {
            return MakeUnauthorizedError("Player token has not been found");
        }

        auto game_session = player->GetGameSession();
        auto dog_id = player->GetDogId();
        auto dogs = game_session->GetDogs();
        auto dog_it = dogs.find(dog_id);
        if (dog_it == dogs.end()) {
            return MakeErrorResponse(http::status::internal_server_error, "internalError", "Dog not found in game session");
        }

        auto dog = dog_it->second;
        dog->SetDefaultDogSpeed(game_session->GetMapDefaultSpeed());
        dog->SetDogSpeed(move_direction);

        return MakeJsonResponse(http::status::ok, json::object{}, req.version(), req.keep_alive());

    } catch (const json::system_error&) {
        return MakeBadRequestError("Failed to parse action");
    } catch (...) {
        return MakeErrorResponse(http::status::internal_server_error, "internalError", "Internal server error");
    }
}

void ApiHandler::SerializeRoads(const model::Map* map, json::object& map_json) const {
    json::array roads_json;
    for (const auto& road : map->GetRoads()) {
        const auto& start = road.GetStart();
        const auto& end = road.GetEnd();

        if (road.IsHorizontal()) {
            roads_json.push_back(json::object{
                {"x0", start.x},
                {"y0", start.y},
                {"x1", end.x}
            });
        } else if (road.IsVertical()) {
            roads_json.push_back(json::object{
                {"x0", start.x},
                {"y0", start.y},
                {"y1", end.y}
            });
        }
    }
    map_json["roads"] = std::move(roads_json);
}

void ApiHandler::SerializeBuildings(const model::Map* map, json::object& map_json) const {
    json::array buildings_json;
    for (const auto& building : map->GetBuildings()) {
        const auto& bounds = building.GetBounds();
        buildings_json.push_back(json::object{
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        });
    }
    map_json["buildings"] = std::move(buildings_json);
}

void ApiHandler::SerializeOffices(const model::Map* map, json::object& map_json) const {
    json::array offices_json;
    for (const auto& office : map->GetOffices()) {
        const auto& position = office.GetPosition();
        const auto& offset = office.GetOffset();

        offices_json.push_back(json::object{
            {"id", *office.GetId()},
            {"x", position.x},
            {"y", position.y},
            {"offsetX", offset.dx},
            {"offsetY", offset.dy}
        });
    }
    map_json["offices"] = std::move(offices_json);
}

// StaticHandler implementation

std::string_view StaticHandler::GetMimeType(std::string_view path) const {
    static const std::unordered_map<std::string_view, std::string_view> mime_types = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".txt", "text/plain"},
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

// RequestHandler implementation


}  // namespace http_handler  
