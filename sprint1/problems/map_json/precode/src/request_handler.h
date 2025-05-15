#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <string_view>
#include <utility>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::get) {
            SendBadRequest(req, std::forward<Send>(send), "Invalid method");
            return;
        }

        if (req.target().starts_with("/api/v1/maps")) {
            if (req.target() == "/api/v1/maps") {
                SendMapsList(req, std::forward<Send>(send));
                return;
            }

            constexpr std::string_view prefix = "/api/v1/maps/";
            if (req.target().starts_with(prefix)) {
                std::string map_id(req.target.substr(prefix.size()));
                SendMapDescription(req, std::forward<Send>(send), map_id);
                return;
            }
        }

        SendBadRequest(req, std::forward<Send>(send), "Bad request");
    }

private:
    model::Game& game_;

       template <typename Body, typename Allocator, typename Send>
    void SendMapsList(const http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        json::array maps_json;
        for (const auto& map : game_.GetMaps()) {
            maps_json.push_back({
                {"id", map.GetId()},
                {"name", map.GetName()}
            });
        }

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = json::serialize(maps_json);
        res.prepare_payload();
        send(std::move(res));
    }

    // Отправляет описание карты в JSON
    template <typename Body, typename Allocator, typename Send>
    void SendMapDescription(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        Send&& send,
        const std::string& map_id
    ) {
        const auto* map = game_.FindMap(model::Map::Id(map_id));
        if (!map) {
            SendNotFound(req, std::forward<Send>(send), "Map not found");
            return;
        }

        // Формируем JSON (примерно как в задании)
        json::value map_json = {
            {"id", map->GetId()},
            {"name", map->GetName()},
            {"roads", json::array()},  // TODO: добавить дороги
            {"buildings", json::array()},  // TODO: добавить здания
            {"offices", json::array()}  // TODO: добавить офисы
        };

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = json::serialize(map_json);
        res.prepare_payload();
        send(std::move(res));
    }

    // Отправляет ошибку 404 Not Found
    template <typename Body, typename Allocator, typename Send>
    void SendNotFound(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        Send&& send,
        const std::string& message
    ) {
        json::value error = {
            {"code", "mapNotFound"},
            {"message", message}
        };

        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = json::serialize(error);
        res.prepare_payload();
        send(std::move(res));
    }

    // Отправляет ошибку 400 Bad Request
    template <typename Body, typename Allocator, typename Send>
    void SendBadRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req,
        Send&& send,
        const std::string& message
    ) {
        json::value error = {
            {"code", "badRequest"},
            {"message", message}
        };

        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = json::serialize(error);
        res.prepare_payload();
        send(std::move(res));
    }
};

}  // namespace http_handler
