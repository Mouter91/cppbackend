#include "serialization.h"
#include <memory>
#include "model.h"

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
  Dog dog(ser_dog.name, ser_dog.id, ser_dog.state, ser_dog.bag_capacity);
  dog.SetDefaultDogSpeed(ser_dog.default_dog_speed);
  dog.AddScore(ser_dog.score);
  for (size_t loot_type : ser_dog.bag_items) {
    dog.GetBag().AddLoot(loot_type);
  }
  return dog;
}

SerGameSession SerializeGameSession(const GameSession& session) {
  SerGameSession ser_session;

  ser_session.map_id = *session.GetMapId();
  ser_session.id = session.GetSessionId();

  for (const auto& [dog_id, dog] : session.GetDogs()) {
    ser_session.dogs[dog_id] = SerializeDog(*dog);
  }

  for (const auto& loot : session.GetLoots()) {
    ser_session.loots.push_back({loot.type, loot.value, loot.position});
  }

  return ser_session;
}

std::shared_ptr<GameSession> DeserializeGameSessionInto(const SerGameSession& ser_session,
                                                        Game& game) {
  // 1. Найти карту
  const Map* map = game.FindMap(Map::Id{ser_session.map_id});
  if (!map) {
    throw std::runtime_error("Map not found: " + ser_session.map_id);
  }

  // 2. Восстановить Dogs из SerDog
  model::GameSession::Dogs dogs;
  for (const auto& [dog_id, ser_dog] : ser_session.dogs) {
    auto dog = std::make_shared<Dog>(DeserializeDog(ser_dog));
    dogs[dog_id] = dog;
  }

  // 3. Восстановить Loots
  std::vector<GameSession::Loot> loots;
  for (const auto& ser_loot : ser_session.loots) {
    loots.push_back({ser_loot.type, ser_loot.value, ser_loot.position});
  }

  // 4. Создать GameSession
  auto session =
      std::make_shared<GameSession>(std::move(dogs), *map, ser_session.id, std::move(loots));

  session->SetRandomSpawnMode(game.GetSettings().random_spawn);

  return session;
}

SerPlayers SerializePlayers(Players& players) {
  SerPlayers ser;

  for (const auto& [token, player_ptr] : players.GetPlayerTokens().GetTokenToPlayer()) {
    SerPlayer ser_player;
    ser_player.dog_id = player_ptr->GetDogId();
    ser_player.gamesession_id = player_ptr->GetGameSession()->GetSessionId();
    ser_player.map_id = *player_ptr->GetGameSession()->GetMap().GetId();

    ser.players_with_tokens.push_back(SerPlayerWithToken{std::string(*token), ser_player});
  }

  return ser;
}

void DeserializePlayers(const SerPlayers& ser_players, Game& game, Players& players) {
  for (const auto& ser_player_with_token : ser_players.players_with_tokens) {
    const auto& ser_player = ser_player_with_token.player;
    const auto& token_str = ser_player_with_token.token;

    // Находим игровую сессию
    auto game_session = game.FindGameSession(Map::Id{ser_player.map_id});
    if (!game_session) {
      continue;
    }

    // Находим собаку
    auto dog = game_session->FindDog(ser_player.dog_id);
    if (!dog) {
      continue;
    }

    // Восстанавливаем игрока с токеном
    auto player = std::make_shared<Player>(dog, game_session);
    players.GetPlayerTokens().AddToken(player, Token{token_str});  // Создаем Token из строки
    players.GetAllPlayers().emplace(std::make_pair(dog->GetId(), ser_player.map_id), player);
  }
}

}  // namespace model
