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
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);     

private:
    model::Game& game_;

    template <typename Body, typename Allocator, typename Send>
    void handlerGetMaps(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send); 

    template <typename Body, typename Allocator, typename Send>
    void handlerGetMapsById(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, const model::Map::Id& mapId); 

    void serializeRoads(const model::Map* map, json::object& mapJson);
    void serializeBuildings(const model::Map* map, json::object& mapJson);
    void serializeOffices(const model::Map* map, json::object& mapJson);
 
    StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                      bool keep_alive, std::string_view content_type = ContentType::TEXT_HTML); 

    StringResponse createErrorResponse(http::status status, std::string_view code, std::string_view message); 

};

}  // namespace http_handler

