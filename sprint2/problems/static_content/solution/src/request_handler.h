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

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, const std::filesystem::path& static_root)
        : game_{game}, static_root_{fs::weakly_canonical(static_root)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() == http::verb::get || req.method() == http::verb::head) {
            std::string target(req.target());
            if (target.starts_with("/api/")) {
                if (target == "/api/v1/maps") {
                    handlerGetMaps(std::move(req), std::forward<Send>(send));
                } else if (target.starts_with("/api/v1/maps/")) {
                    const auto mapId = model::Map::Id(std::string(target.substr(13)));
                    handlerGetMapsById(std::move(req), std::forward<Send>(send), mapId);
                } else {
                    send(createErrorResponse(http::status::bad_request, "badRequest", "Bad request"));
                }
            } else {
                handleStaticFile(std::move(req), std::forward<Send>(send));
            }
        } else {
            send(createErrorResponse(http::status::method_not_allowed, "notAllowed", "Method Not Allowed"));
        }
    }

private:
    model::Game& game_;
    fs::path static_root_;
 
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

    template <typename Body, typename Allocator, typename Send>
    void handleStaticFile(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string decoded_path = urlDecode(req.target());

        if (decoded_path.empty() || decoded_path == "/") {
        decoded_path = "index.html";
        }    

        // Удаляем начальный слэш если есть
        if (!decoded_path.empty() && decoded_path[0] == '/') {
            decoded_path.erase(0, 1);
        }
        
        fs::path file_path = static_root_ / decoded_path;
        file_path = fs::weakly_canonical(file_path);
        
        // Проверка что путь находится внутри static_root
        if (!isPathWithinRoot(file_path)) {
            return send(createErrorResponse(http::status::bad_request, "badRequest", "Path is outside root directory"));
        }
        
        // Если это директория, добавляем index.html
        if (fs::is_directory(file_path)) {
            file_path /= "index.html";
        }
        
        // Открываем файл
        http::file_body::value_type file;
        beast::error_code ec;
        file.open(file_path.c_str(), beast::file_mode::read, ec);
        
        if (ec) {
            return send(createErrorResponse(http::status::not_found, "notFound", "File not found"));
        }
        
        // Создаем ответ с файлом
        FileResponse res{http::status::ok, req.version()};
        res.set(http::field::content_type, getMimeType(file_path.extension().string()));
        res.body() = std::move(file);
        res.prepare_payload();
        res.keep_alive(req.keep_alive());
        
        send(std::move(res));
    }

    std::string getMimeType(const std::string& ext) {
        static const std::unordered_map<std::string, std::string> mime_types = {
            {".htm", "text/html"}, {".html", "text/html"}, {".css", "text/css"}, {".txt", "text/plain"},
            {".js", "text/javascript"}, {".json", "application/json"}, {".xml", "application/xml"},
            {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".gif", "image/gif"},
            {".bmp", "image/bmp"}, {".ico", "image/vnd.microsoft.icon"}, {".tiff", "image/tiff"},
            {".svg", "image/svg+xml"}, {".mp3", "audio/mpeg"}};
        std::string ext_lower = ext;
        std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
        auto it = mime_types.find(ext_lower);
        return it != mime_types.end() ? it->second : std::string(ContentType::OCTET_STREAM);
    } 
    bool isPathWithinRoot(const fs::path& path) {
    // Получаем относительный путь от static_root
        auto rel_path = fs::relative(path, static_root_);
    
    // Если путь не начинается с .. и не пустой, значит он внутри
        return !rel_path.empty() && rel_path.native()[0] != '.';
    }
    bool IsSubPath(fs::path path) {
        path = fs::weakly_canonical(path);
        auto base = fs::weakly_canonical(static_root_);

        for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
            if (p == path.end() || *p != *b) {
                return false;
            }
        }
        return true;
    }

    std::string urlDecode(std::string_view str) {
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

    void serializeRoads(const model::Map* map, json::object& mapJson);
    void serializeBuildings(const model::Map* map, json::object& mapJson);
    void serializeOffices(const model::Map* map, json::object& mapJson);
 
    StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                      bool keep_alive, std::string_view content_type = ContentType::TEXT_HTML); 

    StringResponse createErrorResponse(http::status status, std::string_view code, std::string_view message); 

};

}  // namespace http_handler

