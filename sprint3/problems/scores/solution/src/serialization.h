#pragma once

#include "model.h"
#include "move_info.h"
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/utility.hpp>  // for std::pair
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <fstream>

namespace model {

template <class Archive>
void serialize(Archive& ar, Point& point, const unsigned int version) {
  ar & point.x & point.y;
}

// Serialization for Size
template <class Archive>
void serialize(Archive& ar, Size& size, const unsigned int version) {
  ar & size.width & size.height;
}

// Serialization for Rectangle
template <class Archive>
void serialize(Archive& ar, Rectangle& rect, const unsigned int version) {
  ar & rect.position & rect.size;
}

// Serialization for Offset
template <class Archive>
void serialize(Archive& ar, Offset& offset, const unsigned int version) {
  ar & offset.dx & offset.dy;
}

// Update the Tagged serialization to use Value() instead of GetValue()
template <class Archive, typename Value, typename Tag>
void serialize(Archive& ar, util::Tagged<Value, Tag>& tagged, const unsigned int version) {
  ar & tagged.Value();
}

template <class Archive>
void serialize(Archive& ar, model::Game::Settings& settings, const unsigned int version) {
  ar & settings.default_dog_speed_;
  ar & settings.default_bag_capacity_;
  ar & settings.random_spawn;
  ar & settings.period;
  ar & settings.probability;
  // Skip ticker as it's runtime state
}

struct SerDog {
  std::string name;
  size_t id;
  int score;
  MoveInfo state;
  double default_dog_speed;
  std::vector<size_t> bag_items;
  size_t bag_capacity;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & name & id & score & state & default_dog_speed & bag_items & bag_capacity;
  }
};

struct SerLoot {
  size_t type;
  int value;
  MoveInfo::Position position;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & type & value & position;
  }
};

struct SerGameSession {
  std::string map_id;
  GameSession::Id id;
  std::unordered_map<Dog::Id, SerDog> dogs;
  std::vector<SerLoot> loots;
  bool random_spawn_mode;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & id & dogs & loots & random_spawn_mode;
  }
};

struct SerPlayer {
  size_t dog_id;
  std::string session_id;
  std::string map_id;
  std::string token;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int) {
    ar & dog_id & session_id & token;
  }
};

struct SerPlayers {
  std::vector<SerPlayer> players;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int) {
    ar & players;
  }
};

/// Конвертирует Dog → SerDog
SerDog SerializeDog(const Dog& dog);
Dog DeserializeDog(const SerDog& ser_dog);

/// Конвертирует GameSession → SerGameSession
SerGameSession SerializeGameSession(const GameSession& session);
void DeserializeGameSessionInto(GameSession& session, const SerGameSession& ser_session);

SerPlayer SerializePlayer(const model::Player& player, const model::Players& players);
SerPlayers SerializePlayers(model::Players& players);

std::shared_ptr<model::Player> DeserializePlayer(const SerPlayer& ser_player, model::Game& game);
void DeserializePlayers(const SerPlayers& ser_players, model::Game& game, model::Players& players);

inline void SaveGame(const model::Game& game, model::Players& players, std::ostream& out) {
  boost::archive::text_oarchive oa(out);
  std::vector<SerGameSession> serialized_sessions;
  for (const auto& session : game.GetGameSessions()) {
    serialized_sessions.push_back(SerializeGameSession(*session));
  }
  SerPlayers serialized_players = SerializePlayers(players);
  oa << serialized_sessions << serialized_players;
}

inline void LoadGame(model::Game& game, model::Players& players, std::istream& in) {
  boost::archive::text_iarchive ia(in);
  std::vector<SerGameSession> serialized_sessions;
  SerPlayers serialized_players;

  ia >> serialized_sessions >> serialized_players;

  for (const auto& ser_session : serialized_sessions) {
    auto map = game.FindMap(model::Map::Id(ser_session.map_id));
    if (!map) {
      throw std::runtime_error("Map not found: " + ser_session.map_id);
    }
    auto session = game.CreateGameSession(map->GetId());
    DeserializeGameSessionInto(*session, ser_session);
  }

  DeserializePlayers(serialized_players, game, players);
}

}  // namespace model
   //
