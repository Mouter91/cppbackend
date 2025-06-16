#include "request_handler.h"

namespace http_handler {

void RequestHandler::serializeRoads(const model::Map* map, json::object& mapJson) {
  json::array roadsJson;
  for (const auto& road : map->GetRoads()) {
    const auto& start = road.GetStart();
    const auto& end = road.GetEnd();

    if (road.IsHorizontal()) {
      roadsJson.push_back(json::object{{"x0", start.x}, {"y0", start.y}, {"x1", end.x}});
    } else if (road.IsVertical()) {
      roadsJson.push_back(json::object{{"x0", start.x}, {"y0", start.y}, {"y1", end.y}});
    }
  }
  mapJson["roads"] = std::move(roadsJson);
}

void RequestHandler::serializeBuildings(const model::Map* map, json::object& mapJson) {
  json::array buildingsJson;
  for (const auto& building : map->GetBuildings()) {
    const auto& bounds = building.GetBounds();
    buildingsJson.push_back(json::object{{"x", bounds.position.x},
                                         {"y", bounds.position.y},
                                         {"w", bounds.size.width},
                                         {"h", bounds.size.height}});
  }
  mapJson["buildings"] = std::move(buildingsJson);
}

void RequestHandler::serializeOffices(const model::Map* map, json::object& mapJson) {
  json::array officesJson;
  for (const auto& office : map->GetOffices()) {
    const auto& position = office.GetPosition();
    const auto& offset = office.GetOffset();

    officesJson.push_back(json::object{{"id", *office.GetId()},
                                       {"x", position.x},
                                       {"y", position.y},
                                       {"offsetX", offset.dx},
                                       {"offsetY", offset.dy}});
  }
  mapJson["offices"] = std::move(officesJson);
}

StringResponse RequestHandler::MakeStringResponse(http::status status, std::string_view body,
                                                  unsigned http_version, bool keep_alive,
                                                  std::string_view content_type) {
  StringResponse response(status, http_version);
  response.set(http::field::content_type, content_type);
  response.body() = body;
  response.prepare_payload();
  response.keep_alive(keep_alive);
  return response;
}

StringResponse RequestHandler::createErrorResponse(http::status status, std::string_view code,
                                                   std::string_view message) {
  json::value error = {{"code", code}, {"message", message}};
  return MakeStringResponse(status, json::serialize(error), 11, false, ContentType::APP_JSON);
}
}  // namespace http_handler
