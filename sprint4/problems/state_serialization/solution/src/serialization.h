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
#include <boost/serialization/access.hpp>

#include <fstream>

namespace model {
template <class Archive>
void serialize(Archive& ar, Point& point, const unsigned int version) {
  ar & point.x & point.y;
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

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & map_id & id & dogs & loots;
  }
};

struct SerPlayer {
  size_t dog_id;
  size_t gamesession_id;
  std::string map_id;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & dog_id & gamesession_id & map_id;
    ;
  }
};

struct SerPlayerWithToken {
  std::string token;
  SerPlayer player;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & token & player;
  }
};

struct SerPlayers {
  std::vector<SerPlayerWithToken> players_with_tokens;

  template <typename Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & players_with_tokens;
  }
};

SerPlayers SerializePlayers(Players& players);
void DeserializePlayers(const SerPlayers& ser_players, Game& game, Players& players);

/// Конвертирует Dog → SerDog
SerDog SerializeDog(const Dog& dog);
Dog DeserializeDog(const SerDog& ser_dog);

/// Конвертирует GameSession → SerGameSession
SerGameSession SerializeGameSession(const GameSession& session);
std::shared_ptr<GameSession> DeserializeGameSessionInto(const SerGameSession& ser_session,
                                                        Game& game);

inline void SaveGame(model::Game& game, model::Players& players, std::ostream& out) {
  boost::archive::text_oarchive oa(out);
  std::vector<SerGameSession> serialized_sessions;
  for (const auto& session : game.GetGameSessions()) {
    serialized_sessions.push_back(SerializeGameSession(*session));
  }
  SerPlayers ser_players = SerializePlayers(players);
  oa << serialized_sessions;
  oa << ser_players;
}

inline void LoadGame(model::Game& game, model::Players& players, std::istream& in) {
  boost::archive::text_iarchive ia(in);
  std::vector<SerGameSession> serialized_sessions;
  SerPlayers ser_players;

  ia >> serialized_sessions;
  ia >> ser_players;

  for (const auto& ser_session : serialized_sessions) {
    game.LoadGameSession(DeserializeGameSessionInto(ser_session, game));
  }

  DeserializePlayers(ser_players, game, players);
}

}  // namespace model
   //
