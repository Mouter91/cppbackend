#include "request_handler.h"

namespace http_handler {
template <typename Body, typename Allocator, typename Send>
void RequestHandler::operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    if (req.method() == http::verb::get) {
        std::string target(req.target());
        if (target == "/api/v1/maps") {
            handlerGetMaps(std::move(req), std::forward<Send>(send));
        } else if(target.starts_with("/api/v1/maps/")){
            const auto mapId = model::Map::Id(std::string(target.substr(13)));
            handlerGetMapsById(std::move(req), std::forward<Send>(send), mapId);
        } else if (target.starts_with("/api/")) {
            send(createErrorResponse(http::status::bad_request, "badRequest", "Bad request"));
        } else {
            send(createErrorResponse(http::status::not_found,"mapNotFound", "Not Found"));
        }
    } else {
        send(createErrorResponse(http::status::method_not_allowed, "notAllowed", "Method Not Allowed"));
    }
}

template <typename Body, typename Allocator, typename Send>
void RequestHandler::handlerGetMaps(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    json::array mapsJson;

    for (const auto& map : game_.GetMaps()) {
        mapsJson.push_back({
            {"id", *map.GetId()},
            {"name", map.GetName()}
        });
    }
    std::string json_str = json::serialize(mapsJson);

    send(std::move(MakeStringResponse(http::status::ok, json_str,
                                      req.version(), req.keep_alive(), ContentType::APP_JSON)));
} 

template <typename Body, typename Allocator, typename Send>
void RequestHandler::handlerGetMapsById(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, const model::Map::Id& mapId) {
    const auto* map = game_.FindMap(mapId);
    if (!map) {
        send(createErrorResponse(http::status::not_found, "mapNotFound", "Map not found"));
        return;
    }

    json::object mapJson{
        {"id", *map->GetId()},
        {"name", map->GetName()}
    };

    // Сериализация данных карты
    serializeRoads(map, mapJson);
    serializeBuildings(map, mapJson);
    serializeOffices(map, mapJson);

    // Отправка ответа
    send(MakeStringResponse(http::status::ok, json::serialize(mapJson), req.version(), req.keep_alive(), ContentType::APP_JSON));
}

void RequestHandler::serializeRoads(const model::Map* map, json::object& mapJson) {
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

void RequestHandler::serializeBuildings(const model::Map* map, json::object& mapJson) {
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

void RequestHandler::serializeOffices(const model::Map* map, json::object& mapJson) {
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


StringResponse RequestHandler::MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                  bool keep_alive, std::string_view content_type) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.prepare_payload();
    response.keep_alive(keep_alive);
    return response;
}

StringResponse RequestHandler::createErrorResponse(http::status status, std::string_view code, std::string_view message) {
    json::value error = {
        {"code", code},
        {"message", message}
    };
    return MakeStringResponse(status, json::serialize(error), 11, false, ContentType::APP_JSON);
}
}  // namespace http_handler
