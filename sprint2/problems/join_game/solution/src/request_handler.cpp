#include "request_handler.h"

namespace http_handler {

void ApiHandler::SerializeRoads(const model::Map* map, json::object& mapJson) {
    json::array roadsJson;
    for (const auto& road : map->GetRoads()) {
        const auto& start = road.GetStart();
        const auto& end = road.GetEnd();

        if (road.IsHorizontal()) {
            roadsJson.push_back(json::object{
                {"x0", start.x},
                {"y0", start.y},
                {"x1", end.x}
            });
        } else if (road.IsVertical()) {
            roadsJson.push_back(json::object{
                {"x0", start.x},
                {"y0", start.y},
                {"y1", end.y}
            });
        }
    }
    mapJson["roads"] = std::move(roadsJson);
}

void ApiHandler::SerializeBuildings(const model::Map* map, json::object& mapJson) {
    json::array buildingsJson;
    for (const auto& building : map->GetBuildings()) {
        const auto& bounds = building.GetBounds();
        buildingsJson.push_back(json::object{
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        });
    }
    mapJson["buildings"] = std::move(buildingsJson);
}

void ApiHandler::SerializeOffices(const model::Map* map, json::object& mapJson) {
    json::array officesJson;
    for (const auto& office : map->GetOffices()) {
        const auto& position = office.GetPosition();
        const auto& offset = office.GetOffset();

        officesJson.push_back(json::object{
            {"id", *office.GetId()},
            {"x", position.x},
            {"y", position.y},
            {"offsetX", offset.dx},
            {"offsetY", offset.dy}
        });
    }
    mapJson["offices"] = std::move(officesJson);
}


StringResponse BaseHandler::MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive, std::string_view content_type) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.prepare_payload();
    response.keep_alive(keep_alive);
    return response;
}

StringResponse BaseHandler::createErrorResponse(
    http::status status,
    std::string_view code,
    std::string_view message,
    std::initializer_list<std::string_view> allowed_methods) 
{
    // Всегда используем JSON для ошибок API
    std::string_view content_type = ContentType::APP_JSON;

    StringResponse response;
    json::value error = {{"code", code}, {"message", message}};
    response = MakeStringResponse(status, json::serialize(error), 11, false, content_type);

    response.set(http::field::cache_control, "no-cache");
    
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

std::string_view StaticHandler::GetMimeType(std::string_view path) {
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

bool StaticHandler::IsPathWithinRoot(const fs::path& path) {
    auto rel_path = fs::relative(path, static_root_);
    return !rel_path.empty() && rel_path.native()[0] != '.';
}

std::string StaticHandler::UrlDecode(std::string_view str) {
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
