#pragma once
#include <boost/json/serialize.hpp>
#include <boost/json/serializer.hpp>
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/json.hpp>
#include <boost/beast/http.hpp>

#include "http_server.h"
#include "model.h"
#include "tagged.h"
#include <string_view>
#include <utility>
#include <filesystem>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;

using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html";
    constexpr static std::string_view APP_JSON = "application/json";
    constexpr static std::string_view OCTET_STREAM = "application/octet-stream"; 
    constexpr static std::string_view TEXT_PLAIN = "text/plain";
};

// Базовый класс для обработчиков
class BaseHandler {
protected:
    StringResponse MakeStringResponse(http::status status, std::string_view body, 
                                    unsigned http_version, bool keep_alive, 
                                    std::string_view content_type = ContentType::TEXT_HTML);

    StringResponse createErrorResponse(http::status status, std::string_view code, 
                                     std::string_view message, 
                                     std::initializer_list<std::string_view> allowed_methods = {});
};

// Обработчик API запросов
class ApiHandler : public BaseHandler {
public:
    explicit ApiHandler(model::Game& game) 
        : game_(game){}

    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());
        
        if (target == "/api/v1/game/join") {
            if (req.method() == http::verb::post) {
                HandleJoinGame(std::move(req), std::forward<Send>(send));
            } else {
                send(createErrorResponse(http::status::method_not_allowed, 
                    "invalidMethod", "Only POST method is expected", {"POST"}));
            }
        } 
        else if (target == "/api/v1/game/players") {
            if (req.method() == http::verb::get || req.method() == http::verb::head) {
                HandleGetPlayers(std::move(req), std::forward<Send>(send));
            } else {
                send(createErrorResponse(http::status::method_not_allowed,
                    "invalidMethod", "Only GET, HEAD methods are expected", {"GET", "HEAD"}));
            }
        }
        else if (target == "/api/v1/maps") {
            if (req.method() == http::verb::get || req.method() == http::verb::head) {
                HandleGetMaps(std::move(req), std::forward<Send>(send));
            } else {
                send(createErrorResponse(http::status::method_not_allowed,
                    "invalidMethod", "Only GET, HEAD methods are expected", {"GET", "HEAD"}));
            }
        }
        else if (target.starts_with("/api/v1/maps/")) {
            if (req.method() == http::verb::get || req.method() == http::verb::head) {
                const auto mapId = model::Map::Id(std::string(target.substr(13)));
                HandlerGetMapsById(std::move(req), std::forward<Send>(send), mapId);
            } else {
                send(createErrorResponse(http::status::method_not_allowed,
                    "invalidMethod", "Only GET, HEAD methods are expected", {"GET", "HEAD"}));
            }
        }
        else {
            send(createErrorResponse(http::status::bad_request, "badRequest", "Bad request"));
        }
    }

private:
    model::Game& game_;
    model::Players players_;

    void SerializeRoads(const model::Map* map, json::object& mapJson);
    void SerializeBuildings(const model::Map* map, json::object& mapJson);
    void SerializeOffices(const model::Map* map, json::object& mapJson);

    // Методы обработки конкретных API endpoints
    template <typename Body, typename Allocator, typename Send>
    void HandleGetMaps(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        json::array mapsJson;
        for (const auto& map : game_.GetMaps()) {
            mapsJson.push_back({
                {"id", *map.GetId()},
                {"name", map.GetName()}
            });
        }
        send(MakeStringResponse(http::status::ok, json::serialize(mapsJson),
                            req.version(), req.keep_alive(), ContentType::APP_JSON));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandlerGetMapsById(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, const model::Map::Id& mapId) {
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
        SerializeRoads(map, mapJson);
        SerializeBuildings(map, mapJson);
        SerializeOffices(map, mapJson);

        // Отправка ответа
        send(MakeStringResponse(http::status::ok, json::serialize(mapJson), req.version(), req.keep_alive(), ContentType::APP_JSON));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoinGame(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req[http::field::content_type] != ContentType::APP_JSON) {
            return send(createErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type"));
        }

        try {
            json::value request_body = json::parse(req.body());
            json::object request_obj = request_body.as_object();

            if (!request_obj.contains("userName") || !request_obj.contains("mapId")) {
                return send(createErrorResponse(http::status::bad_request, "invalidArgument", "Missing required fields"));
            }

            std::string userName = json::value_to<std::string>(request_obj.at("userName"));
            std::string mapId = json::value_to<std::string>(request_obj.at("mapId"));

            if (userName.empty()) {
                return send(createErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name"));
            }

            const auto* map = game_.FindMap(model::Map::Id(mapId));
            if (!map) {
                return send(createErrorResponse(http::status::not_found, "mapNotFound", "Map not found"));
            }

            auto session = game_.FindGameSession(model::Map::Id(mapId));
            if (!session) {
                return send(createErrorResponse(http::status::internal_server_error, "internalError", "Failed to create game session"));
            }

            auto dog = std::make_shared<model::Dog>(userName);
            auto token = players_.AddPlayer(dog, session);

            json::object response{
                {"authToken", *token},
                {"playerId", dog->GetId()}
            };

            auto res = MakeStringResponse(http::status::ok, json::serialize(response),
                                        req.version(), req.keep_alive(), ContentType::APP_JSON);
            res.set(http::field::cache_control, "no-cache");
            return send(std::move(res));

        } catch (const json::system_error&) {
            return send(createErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error"));
        } catch (...) {
            return send(createErrorResponse(http::status::internal_server_error, "internalError", "Internal server error"));
        }
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleGetPlayers(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto auth_header = req[http::field::authorization];
        if (auth_header.empty()) {
            return send(createErrorResponse(http::status::unauthorized, "invalidToken", 
                      "Authorization header is missing"));
        }

        const std::string bearer_prefix = "Bearer ";
        if (auth_header.find(bearer_prefix) != 0) {
            return send(createErrorResponse(http::status::unauthorized, "invalidToken", 
                      "Invalid token format"));
        }

        std::string token_str(auth_header.substr(bearer_prefix.size()));
        model::Token token(token_str);

        auto player = players_.GetPlayerByToken(token);
        if (!player) {
            return send(createErrorResponse(http::status::unauthorized, "unknownToken", 
                      "Player token has not been found"));
        }

        auto game_session = player->GetGameSession();
        json::object players_json;
        
        for (const auto& [dog_id, dog] : game_session->GetDogs()) {
            players_json[std::to_string(dog_id)] = json::object{{"name", dog->GetName()}};
        }

        auto response = MakeStringResponse(http::status::ok, json::serialize(players_json), 
                                 req.version(), req.keep_alive(), ContentType::APP_JSON);
        response.set(http::field::cache_control, "no-cache");
        return send(std::move(response));
    }
};

// Обработчик статических файлов
class StaticHandler : public BaseHandler {
public:
    explicit StaticHandler(const std::filesystem::path& static_root)
        : static_root_(fs::weakly_canonical(static_root)) {}

    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(createErrorResponse(http::status::method_not_allowed,
                "invalidMethod", "Only GET, HEAD methods are expected", {"GET", "HEAD"}));
        }

        std::string decoded_path = UrlDecode(req.target());

        if (decoded_path.empty() || decoded_path == "/") {
            decoded_path = "index.html";
        }    

        if (!decoded_path.empty() && decoded_path[0] == '/') {
            decoded_path.erase(0, 1);
        }
        
        fs::path file_path = static_root_ / decoded_path;
        file_path = fs::weakly_canonical(file_path);
        
        if (!IsPathWithinRoot(file_path)) {
            return send(createErrorResponse(http::status::bad_request, "badRequest", "Path is outside root directory"));
        }
        
        if (fs::is_directory(file_path)) {
            file_path /= "index.html";
        }
        
        http::file_body::value_type file;
        beast::error_code ec;
        file.open(file_path.c_str(), beast::file_mode::read, ec);
        
        if (ec) {
            return send(createErrorResponse(http::status::not_found, "notFound", "File not found"));
        }
        
        FileResponse res{http::status::ok, req.version()};
        res.set(http::field::content_type, GetMimeType(file_path.extension().string()));
        res.body() = std::move(file);
        res.prepare_payload();
        res.keep_alive(req.keep_alive());
        
        send(std::move(res));
    }

private:
    fs::path static_root_;

    std::string_view GetMimeType(std::string_view path);
    bool IsPathWithinRoot(const fs::path& path);
    std::string UrlDecode(std::string_view str);
};

// Основной обработчик запросов
class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, const std::filesystem::path& static_root)
        : api_handler_(game), static_handler_(static_root) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());
        
        if (target.starts_with("/api/")) {
            api_handler_.HandleRequest(std::move(req), std::forward<Send>(send));
        } else {
            static_handler_.HandleRequest(std::move(req), std::forward<Send>(send));
        }
    }

private:
    ApiHandler api_handler_;
    StaticHandler static_handler_;
};

}  // namespace http_handler
