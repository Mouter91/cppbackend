#pragma once

#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <filesystem>
#include <string_view>

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
using namespace std::literals;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html";
    constexpr static std::string_view APP_JSON = "application/json";
    constexpr static std::string_view OCTET_STREAM = "application/octet-stream"; 
    constexpr static std::string_view TEXT_PLAIN = "text/plain";
};

class ApiHandler {
public:
    explicit ApiHandler(model::Game& game) : game_(game) {}
    static StringResponse MakeStringResponse(http::status status, 
                                           std::string_view body, 
                                           unsigned http_version,
                                           bool keep_alive, 
                                           std::string_view content_type = ContentType::TEXT_HTML);

    static StringResponse CreateErrorResponse(http::status status, 
                                            std::string_view code, 
                                            std::string_view message);
protected:
    model::Game& GetGame() const { return game_; }

private:
    model::Game& game_;
};

class MapApiHandler : public ApiHandler {
public:
    using ApiHandler::ApiHandler;
    
    StringResponse HandleMaps(const StringRequest& req);
    StringResponse HandleMapById(const StringRequest& req, const model::Map::Id& map_id);

private:
    void SerializeRoads(const model::Map* map, json::object& mapJson);
    void SerializeBuildings(const model::Map* map, json::object& mapJson);
    void SerializeOffices(const model::Map* map, json::object& mapJson);
};

class GameApiHandler : public ApiHandler {
public:
    using ApiHandler::ApiHandler;
    
    StringResponse HandleJoinGame(const StringRequest& req);
    StringResponse HandleGetPlayers(const StringRequest& req);

private:
    bool ValidateAuthHeader(std::string_view auth_header, std::string& token) const;
    static std::string GenerateAuthToken();
};

class StaticFileHandler : public ApiHandler {
public:
    explicit StaticFileHandler(model::Game& game, const fs::path& static_root) : ApiHandler(game), static_root_(fs::weakly_canonical(static_root)) {}
    
    StringResponse HandleRequest(const StringRequest& req);

private:
    fs::path static_root_;

    std::string GetMimeType(const std::string& ext) const;
    bool IsPathWithinRoot(const fs::path& path) const;
    std::string UrlDecode(std::string_view str) const;
};
