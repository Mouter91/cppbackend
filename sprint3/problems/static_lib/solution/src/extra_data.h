#pragma once
#include <boost/json.hpp>
#include <unordered_map>

#include "model.h"
#include "tagged.h"

namespace extra_data {

class MapsExtra {
 public:
  void AddMapData(const model::Map::Id& map_id, boost::json::value loot_types) {
    loot_types_[map_id] = std::move(loot_types);
  }

  const boost::json::value& GetLootTypes(const model::Map::Id& map_id) const {
    static const boost::json::value empty = boost::json::array();
    auto it = loot_types_.find(map_id);
    return it != loot_types_.end() ? it->second : empty;
  }

 private:
  std::unordered_map<model::Map::Id, boost::json::value, util::TaggedHasher<model::Map::Id>>
      loot_types_;
};

}  // namespace extra_data
