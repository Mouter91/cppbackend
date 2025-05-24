#include "serialization.h"

namespace model {

// --- Реализация функций преобразования ---

SerDog SerializeDog(const Dog& dog) {
  return {
      dog.GetName(),              // name
      dog.GetId(),                // id
      dog.GetScore(),             // score
      dog.GetState(),             // state
      dog.GetDefaultDogSpeed(),   // default_dog_speed
      dog.GetBag().GetItems(),    // bag_items
      dog.GetBag().GetCapacity()  // bag_capacity
  };
}

Dog DeserializeDog(const SerDog& ser_dog) {
  Dog dog(ser_dog.name, ser_dog.state, ser_dog.bag_capacity);
  dog.SetDefaultDogSpeed(ser_dog.default_dog_speed);
  for (size_t loot_type : ser_dog.bag_items) {
    dog.GetBag().AddLoot(loot_type);
  }
  return dog;
}

SerGameSession SerializeGameSession(const GameSession& session) {
  SerGameSession ser_session(*session.GetMapId());  // Используем конструктор
  ser_session.id = session.GetSessionId();
  ser_session.random_spawn_mode = session.IsRandomSpawnEnabled();

  for (const auto& [dog_id, dog] : session.GetDogs()) {
    ser_session.dogs[dog_id] = SerializeDog(*dog);
  }

  for (const auto& loot : session.GetLoots()) {
    ser_session.loots.push_back({loot.type, loot.value, loot.position});
  }

  return ser_session;
}

void DeserializeGameSessionInto(GameSession& session, const SerGameSession& ser_session) {
  session.SetRandomSpawnMode(ser_session.random_spawn_mode);

  for (const auto& [dog_id, ser_dog] : ser_session.dogs) {
    auto dog = std::make_shared<Dog>(DeserializeDog(ser_dog));
    session.AddDog(dog);
  }

  for (const auto& ser_loot : ser_session.loots) {
    session.AddLoot({ser_loot.type, ser_loot.value, ser_loot.position});
  }
}

SerPlayer SerializePlayer(std::shared_ptr<Player> player, model::Players& players) {
  SerPlayer ser;
  ser.dog_id = player->GetDogId();
  ser.map_id = *player->GetGameSession()->GetMapId();
  ser.session_id = player->GetGameSession()->GetSessionId();
  ser.token = *players.GetPlayerTokens().FindTokenByPlayer(player);
  return ser;
}

SerPlayers SerializePlayers(model::Players& players) {
  SerPlayers result;
  auto& player_tokens = players.GetPlayerTokens().GetTokenToPlayer();
  for (const auto& [token, player] : player_tokens) {
    SerPlayer ser_player;
    ser_player.dog_id = player->GetDogId();
    ser_player.map_id = *player->GetGameSession()->GetMapId();
    ser_player.session_id = player->GetGameSession()->GetSessionId();
    ser_player.token = *token;
    result.players.push_back(ser_player);
  }
  return result;
}

std::shared_ptr<model::Player> DeserializePlayer(const SerPlayer& ser, model::Game& game) {
  auto session = game.FindGameSession(model::Map::Id(ser.map_id));
  if (!session) {
    throw std::runtime_error("GameSession not found for map_id: ");
  }

  const auto& dogs = session->GetDogs();  // Ссылка, чтобы не копировать контейнер
  auto it = dogs.find(ser.dog_id);
  if (it != dogs.end()) {
    auto dog = it->second;
    return std::make_shared<model::Player>(dog, session);
  }

  throw std::runtime_error("Dog not found for dog_id: " + std::to_string(ser.dog_id));
}

void DeserializePlayers(const SerPlayers& ser_players, model::Game& game, model::Players& result) {
  for (const auto& ser_player : ser_players.players) {
    auto session = game.FindGameSession(model::Map::Id(ser_player.map_id));
    if (!session)
      continue;

    const auto& dogs = session->GetDogs();
    auto it = dogs.find(ser_player.dog_id);
    if (it == dogs.end())
      continue;

    auto dog = it->second;
    auto player = std::make_shared<model::Player>(dog, session);
    result.AddPlayerWithToken(player, model::Token(ser_player.token));
  }
}

}  // namespace model
