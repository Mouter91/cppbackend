#pragma once
#include <boost/json/serialize.hpp>
#include <boost/json/serializer.hpp>
#include <boost/algorithm/string.hpp>
#include "application.h"
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/json.hpp>
#include <boost/beast/http.hpp>

#include "http_server.h"
#include "model.h"
#include "tagged.h"
#include "extra_data.h"

#include <string_view>
#include <utility>
#include <filesystem>
#include <optional>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace net = boost::asio;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using Strand = net::strand<net::io_context::executor_type>;

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

  StringResponse MakeJsonResponse(http::status status, const json::value& body,
                                  unsigned http_version, bool keep_alive);

  StringResponse MakeErrorResponse(http::status status, std::string_view code,
                                   std::string_view message,
                                   const std::vector<std::string_view>& allowed_methods = {});

  StringResponse MakeUnauthorizedError(
      std::string_view message = "Authorization header is missing");
  StringResponse MakeBadRequestError(std::string_view message = "Bad request");
  StringResponse MakeMethodNotAllowedError(const std::vector<std::string_view>& allowed_methods);
};

// Обработчик API запросов
class ApiHandler : public BaseHandler {
 public:
  explicit ApiHandler(app::Application& app, Strand& strand) : app_(app), strand_(strand) {
  }

  template <typename Body, typename Allocator, typename Send>
  void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    auto handle = [this, req = std::move(req), send = std::forward<Send>(send)]() mutable {
      static const std::unordered_map<
          std::string_view,
          std::vector<std::pair<http::verb, StringResponse (ApiHandler::*)(const StringRequest&)>>>
          handlers = {
              {"/api/v1/game/join", {{http::verb::post, &ApiHandler::HandleJoinGame}}},
              {"/api/v1/game/players",
               {{http::verb::get, &ApiHandler::HandleGetPlayers},
                {http::verb::head, &ApiHandler::HandleGetPlayers}}},
              {"/api/v1/maps",
               {{http::verb::get, &ApiHandler::HandleGetMaps},
                {http::verb::head, &ApiHandler::HandleGetMaps}}},
              {"/api/v1/game/state",
               {{http::verb::get, &ApiHandler::HandleGetGameState},
                {http::verb::head, &ApiHandler::HandleGetGameState}}},
              {"/api/v1/game/records",
               {{http::verb::get, &ApiHandler::HandleGetRecords},
                {http::verb::head, &ApiHandler::HandleGetRecords}}},
              {"/api/v1/game/player/action", {{http::verb::post, &ApiHandler::HandlePlayerAction}}},
              {"/api/v1/game/tick", {{http::verb::post, &ApiHandler::HandleGameTick}}}};

      auto it = handlers.find(req.target());
      if (it != handlers.end()) {
        for (const auto& [verb, handler] : it->second) {
          if (verb == req.method()) {
            return send((this->*handler)(req));
          }
        }
        std::vector<std::string_view> allowed_methods;
        for (const auto& [m, _] : it->second) {
          allowed_methods.push_back(boost::beast::http::to_string(m));
        }
        return send(MakeMethodNotAllowedError(allowed_methods));
      }

      // Handle map ID requests
      if (req.target().starts_with("/api/v1/maps/") &&
          req.target().size() > strlen("/api/v1/maps/")) {
        auto map_id = model::Map::Id{std::string(req.target().substr(strlen("/api/v1/maps/")))};
        if (req.method() == http::verb::get || req.method() == http::verb::head) {
          return send(HandleGetMapById(req, map_id));
        } else {
          return send(MakeMethodNotAllowedError({"GET", "HEAD"}));
        }
      }

      return send(MakeBadRequestError());
    };

    net::dispatch(strand_, std::move(handle));
  }

 private:
  app::Application& app_;
  Strand& strand_;

  // Вспомогательные методы
  std::optional<model::Token> TryExtractToken(const StringRequest& req) const;
  bool ValidateContentType(const StringRequest& req) const;
  bool ValidateMoveDirection(const std::string& direction) const;

  // Шаблонный метод для авторизованных запросов
  template <typename Fn>
  StringResponse ExecuteAuthorized(const StringRequest& req, Fn&& action) {
    auto token = TryExtractToken(req);
    if (!token) {
      return MakeUnauthorizedError();
    }

    auto player = app_.GetPlayers().GetPlayerByToken(*token);
    if (!player) {
      return MakeUnauthorizedError("Player token has not been found");
    }

    return action(*player);
  }

  // Обработчики эндпоинтов
  StringResponse HandleGetMaps(const StringRequest& req);
  StringResponse HandleGetMapById(const StringRequest& req, const model::Map::Id& map_id);
  StringResponse HandleJoinGame(const StringRequest& req);
  StringResponse HandleGetPlayers(const StringRequest& req);
  StringResponse HandleGetGameState(const StringRequest& req);
  StringResponse HandlePlayerAction(const StringRequest& req);
  StringResponse HandleGameTick(const StringRequest& req);
  StringResponse HandleGetRecords(const StringRequest& req);

  // Сериализаторы
  void SerializeRoads(const model::Map* map, json::object& map_json) const;
  void SerializeBuildings(const model::Map* map, json::object& map_json) const;
  void SerializeOffices(const model::Map* map, json::object& map_json) const;
};

// Обработчик статических файлов
class StaticHandler : public BaseHandler {
 public:
  explicit StaticHandler(const std::filesystem::path& static_root)
      : static_root_(fs::weakly_canonical(static_root)) {
  }

  template <typename Body, typename Allocator, typename Send>
  void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
      send(MakeMethodNotAllowedError({"GET", "HEAD"}));
      return;
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
      send(MakeStringResponse(http::status::bad_request, "Path is outside root directory",
                              req.version(), req.keep_alive(), ContentType::TEXT_PLAIN));
      return;
    }

    if (fs::is_directory(file_path)) {
      file_path /= "index.html";
    }

    http::file_body::value_type file;
    beast::error_code ec;
    file.open(file_path.c_str(), beast::file_mode::read, ec);

    if (ec) {
      send(MakeStringResponse(http::status::not_found, "File not found", req.version(),
                              req.keep_alive(), ContentType::TEXT_PLAIN));
      return;
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

  std::string_view GetMimeType(std::string_view path) const;
  bool IsPathWithinRoot(const fs::path& path) const;
  std::string UrlDecode(std::string_view str) const;
};

// Основной обработчик запросов
class RequestHandler {
 public:
  explicit RequestHandler(app::Application& app, Strand& strand,
                          const std::filesystem::path& static_root)
      : api_handler_(app, strand), static_handler_(static_root) {
  }

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
