#pragma once
#include <boost/json/serialize.hpp>
#include <boost/json/serializer.hpp>
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/json.hpp>

#include "http_server.h"
#include "model.h"
#include "tagged.h"
#include <string_view>
#include <utility>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using StringResponse = http::response<http::string_body>;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html";
    constexpr static std::string_view APP_JSON = "application/json";
};

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

template <typename Body, typename Allocator, typename Send>
void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
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

private:
    model::Game& game_;

template <typename Body, typename Allocator, typename Send>
void handlerGetMaps(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
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
void handlerGetMapsById(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, const model::Map::Id& mapId) {
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


    void serializeRoads(const model::Map* map, json::object& mapJson);
    void serializeBuildings(const model::Map* map, json::object& mapJson);
    void serializeOffices(const model::Map* map, json::object& mapJson);
 
    StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                      bool keep_alive, std::string_view content_type = ContentType::TEXT_HTML); 

    StringResponse createErrorResponse(http::status status, std::string_view code, std::string_view message); 

};

}  // namespace http_handler

