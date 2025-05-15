#include "api_handler.h"

#include <boost/beast/http/field.hpp>
#include <sstream>
#include <fstream>
#include <iomanip>



StringResponse ApiHandler::MakeStringResponse(http::status status, std::string_view body, 
                                            unsigned http_version, bool keep_alive, 
                                            std::string_view content_type) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.set(http::field::cache_control, "no-cache");
    response.body() = body;
    response.prepare_payload();
    response.keep_alive(keep_alive);
    return response;
}

StringResponse ApiHandler::CreateErrorResponse(http::status status, std::string_view code, 
                                             std::string_view message) {
    json::value error = {{"code", code}, {"message", message}};
    return MakeStringResponse(status, json::serialize(error), 11, false, ContentType::APP_JSON);
}

StringResponse MapApiHandler::HandleMaps(const StringRequest& req) {
    json::array mapsJson;
    const auto& maps = GetGame().GetMaps();

    for (const auto& map : maps) {
        mapsJson.push_back({
            {"id", *map.GetId()},
            {"name", map.GetName()}
        });
    }

    return MakeStringResponse(http::status::ok, json::serialize(mapsJson),
                          req.version(), req.keep_alive(), ContentType::APP_JSON);
}

StringResponse MapApiHandler::HandleMapById(const StringRequest& req, const model::Map::Id& map_id) {
    const auto* map = GetGame().FindMap(map_id);
    if (!map) {
        return CreateErrorResponse(http::status::not_found, "mapNotFound", "Map not found");
    }

    json::object mapJson{
        {"id", *map->GetId()},
        {"name", map->GetName()}
    };

    SerializeRoads(map, mapJson);
    SerializeBuildings(map, mapJson);
    SerializeOffices(map, mapJson);

    return MakeStringResponse(http::status::ok, json::serialize(mapJson),
                          req.version(), req.keep_alive(), ContentType::APP_JSON);
}

void MapApiHandler::SerializeRoads(const model::Map* map, json::object& mapJson) {
    json::array roadsJson;
    for (const auto& road : map->GetRoads()) {
        const auto& start = road.GetStart();
        const auto& end = road.GetEnd();

        json::object roadJson{
            {"x0", start.x},
            {"y0", start.y}
        };

        if (road.IsHorizontal()) {
            roadJson["x1"] = end.x;
        } else {
            roadJson["y1"] = end.y;
        }

        roadsJson.push_back(std::move(roadJson));
    }
    mapJson["roads"] = std::move(roadsJson);
}

void MapApiHandler::SerializeBuildings(const model::Map* map, json::object& mapJson) {
    json::array buildingsJson;
    for (const auto& building : map->GetBuildings()) {
        const auto& bounds = building.GetBounds();
        buildingsJson.push_back({
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        });
    }
    mapJson["buildings"] = std::move(buildingsJson);
}

void MapApiHandler::SerializeOffices(const model::Map* map, json::object& mapJson) {
    json::array officesJson;
    for (const auto& office : map->GetOffices()) {
        officesJson.push_back({
            {"id", *office.GetId()},
            {"x", office.GetPosition().x},
            {"y", office.GetPosition().y},
            {"offsetX", office.GetOffset().dx},
            {"offsetY", office.GetOffset().dy}
        });
    }
    mapJson["offices"] = std::move(officesJson);
}


StringResponse StaticFileHandler::HandleRequest(const StringRequest& req) {
    std::string decoded_path = UrlDecode(req.target());
    
    // Обработка корневого пути
    if (decoded_path.empty() || decoded_path == "/") {
        decoded_path = "index.html";
    }
    
    // Удаляем ведущий слэш
    if (!decoded_path.empty() && decoded_path[0] == '/') {
        decoded_path.erase(0, 1);
    }
    
    // Формируем полный путь к файлу
    fs::path file_path = static_root_ / decoded_path;
    file_path = fs::weakly_canonical(file_path);
    
    // Проверка безопасности пути
    if (!IsPathWithinRoot(file_path)) {
        return CreateErrorResponse(
            http::status::bad_request, 
            "badRequest", 
            "Path is outside root directory");
    }
    
    // Если путь - директория, ищем index.html
    if (fs::is_directory(file_path)) {
        file_path /= "index.html";
    }
    
    // Пытаемся открыть файл
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return CreateErrorResponse(
            http::status::not_found, 
            "notFound", 
            "File not found");
    }
    
    // Читаем содержимое файла в строку
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    return MakeStringResponse(
        http::status::ok, 
        content,
        req.version(), 
        req.keep_alive(),
        GetMimeType(file_path.extension().string()));
}

std::string StaticFileHandler::GetMimeType(const std::string& ext) const {
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"}, {".htm", "text/html"}, {".css", "text/css"},
        {".js", "application/javascript"}, {".json", "application/json"},
        {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".gif", "image/gif"}, {".svg", "image/svg+xml"}, {".ico", "image/x-icon"},
        {".txt", "text/plain"}
    };
    
    auto it = mime_types.find(ext);
    return it != mime_types.end() ? it->second : "text/plain";
}

bool StaticFileHandler::IsPathWithinRoot(const fs::path& path) const {
    auto rel_path = fs::relative(path, static_root_);
    return !rel_path.empty() && rel_path.native()[0] != '.';
}

std::string StaticFileHandler::UrlDecode(std::string_view str) const {
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

StringResponse GameApiHandler::HandleJoinGame(const StringRequest& req);
StringResponse GameApiHandler::HandleGetPlayers(const StringRequest& req);
