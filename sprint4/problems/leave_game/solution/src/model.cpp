#include "model.h"
#include "move_info.h"

#include <chrono>
#include <stdexcept>

namespace model {
using namespace std::literals;

//---------------------------Map---------------------------

Map::Map(Id id, std::string name) noexcept : id_(std::move(id)), name_(std::move(name)) {
}

const Map::Id& Map::GetId() const noexcept {
  return id_;
}

const std::string& Map::GetName() const noexcept {
  return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
  return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
  return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
  return offices_;
}

void Map::AddRoad(const Road& road) {
  roads_.emplace_back(road);
}

void Map::AddBuilding(const Building& building) {
  buildings_.emplace_back(building);
}

void Map::AddOffice(Office office) {
  if (warehouse_id_to_index_.contains(office.GetId())) {
    throw std::invalid_argument("Duplicate warehouse");
  }

  const size_t index = offices_.size();
  Office& o = offices_.emplace_back(std::move(office));
  try {
    warehouse_id_to_index_.emplace(o.GetId(), index);
  } catch (...) {
    offices_.pop_back();
    throw;
  }
}

void Map::SetDefaultDogSpeed(double default_speed) {
  default_dog_speed_ = default_speed;
}

double Map::GetDefaultDogSpeed() const {
  return default_dog_speed_;
}

bool Map::IsDefaultDogSpeedValueConfigured() const {
  return default_dog_speed_ != 1;
}

//****************************************************************
//------------------------------Dog--------------------------------

Dog::Dog(std::string_view name, size_t bag_capacity)
    : name_(name), id_(boost::hash<std::string>()(std::string(name))), bag_(bag_capacity) {
}

Dog::Dog(std::string_view name, MoveInfo state, size_t bag_capacity)
    : name_(name),
      id_(boost::hash<std::string>()(std::string(name))),
      state_(std::move(state)),
      bag_(bag_capacity) {
}

Dog::Dog(std::string_view name, size_t dog_id, MoveInfo state, size_t bag_capacity)
    : name_(name), id_(dog_id), state_(std::move(state)), bag_(bag_capacity) {
}

const std::string& Dog::GetName() const {
  return name_;
}

const MoveInfo::Position& Dog::GetPosition() const noexcept {
  return state_.position;
}

const MoveInfo::Speed& Dog::GetSpeed() const noexcept {
  return state_.speed;
}

const MoveInfo::Direction& Dog::GetDirection() const noexcept {
  return state_.direction;
}

const MoveInfo& Dog::GetState() const noexcept {
  return state_;
}

void Dog::SetDefaultDogSpeed(double speed) {
  default_dog_speed_ = speed;
}

void Dog::SetDogSpeed(double x, double y) {
  state_.speed.x = x;
  state_.speed.y = y;
}

void Dog::SetDogDirSpeed(std::string dir) {
  if (dir == "") {
    SetDogSpeed(0, 0);
  } else if (dir == "L") {
    SetDogSpeed(-default_dog_speed_, 0);
    state_.direction = MoveInfo::Direction::WEST;
  } else if (dir == "R") {
    SetDogSpeed(default_dog_speed_, 0);
    state_.direction = MoveInfo::Direction::EAST;
  } else if (dir == "U") {
    SetDogSpeed(0, -default_dog_speed_);
    state_.direction = MoveInfo::Direction::NORTH;
  } else if (dir == "D") {
    SetDogSpeed(0, default_dog_speed_);
    state_.direction = MoveInfo::Direction::SOUTH;
  }
}

const double Dog::GetDefaultDogSpeed() const {
  return default_dog_speed_;
}

void Dog::StopDog() {
  SetDogSpeed(0, 0);
}

MoveInfo::Position Dog::MoveDog(MoveInfo::Position other_pos) {
  state_.position = other_pos;
  return state_.position;
}

Bag& Dog::GetBag() {
  return bag_;
}

const Bag& Dog::GetBag() const {
  return bag_;
}

void Dog::SetBagCapacity(size_t capacity) {
  bag_.SetCapacity(capacity);
}

int Dog::GetScore() const {
  return score_;
}
void Dog::AddScore(int value) {
  score_ += value;
}

//************************************************************
//---------------------------GameSession----------------------

GameSession::GameSession(const Map& map) : map_(map), id_(GenerateId()) {
  InitializeRegions();
}

GameSession::GameSession(Dogs dogs, const Map& map, Id id, std::vector<Loot> loots)
    : dogs_(std::move(dogs)), map_(map), id_(std::move(id)), loots_(std::move(loots)) {
  InitializeRegions();
}

Map::Id GameSession::GetMapId() const {
  return map_.GetId();
}

void GameSession::AddDog(std::shared_ptr<Dog> dog) {
  auto start = FindStartingPosition();
  dog->MoveDog(start);

  dogs_.insert({dog->GetId(), dog});
}

const GameSession::Dogs& GameSession::GetDogs() const {
  return dogs_;
}

bool GameSession::HasDog(Dog::Id id) {
  return dogs_.count(id) > 0;
}

const GameSession::Id GameSession::GetSessionId() const {
  return id_;
}

const std::vector<std::string> GameSession::GetPlayersNames() const {
  std::vector<std::string> names;
  for (const auto& [dog_id, dog] : dogs_) {
    names.push_back(dog->GetName());
  }

  return names;
}

const std::vector<MoveInfo> GameSession::GetPlayersUnitStates() const {
  std::vector<MoveInfo> dogs;
  for (const auto& [dog_id, dog] : dogs_) {
    dogs.push_back(dog->GetState());
  }

  return dogs;
}

double GameSession::GetMapDefaultSpeed() const {
  return map_.GetDefaultDogSpeed();
}

void GameSession::MovePlayer(Dog::Id id, double delta_time) {
  auto& dog = dogs_[id];
  auto new_position = CalculateNewPosition(dog->GetPosition(), dog->GetSpeed(), delta_time);

  if (IsWithinAnyRegion(new_position, regions_)) {
    dog->MoveDog(new_position);
  } else {
    MoveInfo::Position max_pos = AdjustPositionToMaxRegion(dog);
    dog->MoveDog(max_pos);
    dog->StopDog();
  }
}

void GameSession::StopPlayer(Dog::Id id) {
  if (HasDog(id)) {
    dogs_.at(id)->StopDog();
  }
}

void GameSession::ProcessCollisions(double delta_time) {
  GameItemGathererProvider provider(*this, delta_time);
  auto events = collision_detector::FindGatherEvents(provider);

  std::sort(events.begin(), events.end(),
            [](const auto& e1, const auto& e2) { return e1.time < e2.time; });

  for (const auto& event : events) {
    auto dog_it = dogs_.find(static_cast<Dog::Id>(event.gatherer_id));
    if (dog_it == dogs_.end())
      continue;

    auto& dog = dog_it->second;
    auto& bag = dog->GetBag();

    if (event.item_id < loots_.size()) {
      // Сбор предмета
      if (!bag.IsFull()) {
        bag.AddLoot(loots_[event.item_id].type);
        loots_.erase(loots_.begin() + event.item_id);
      }
    } else {
      // Сдача предметов на базу
      const auto& loot_values = map_.GetLootValues();
      for (size_t loot_type : bag.GetItems()) {
        if (loot_type < loot_values.size()) {
          dog->AddScore(loot_values[loot_type]);  // Начисляем очки собаке
        }
      }
      bag.Clear();
    }
  }
}

void GameSession::Tick(double delta_time) {
  // Сначала перемещаем всех игроков
  for (auto& [id, dog] : dogs_) {
    MovePlayer(id, delta_time);
  }

  // Затем обрабатываем коллизии
  ProcessCollisions(delta_time);
}

void GameSession::InitializeRegions() {
  for (const auto& road : map_.GetRoads()) {
    if (road.IsHorizontal()) {
      double left_x = road.GetStart().x;
      double right_x = road.GetEnd().x;
      if (left_x > right_x) {
        std::swap(left_x, right_x);
      }
      regions_.emplace(regions_.size(), Region{left_x - 0.4, right_x + 0.4, road.GetStart().y - 0.4,
                                               road.GetStart().y + 0.4});
    } else if (road.IsVertical()) {
      double lower_y = road.GetStart().y;
      double upper_y = road.GetEnd().y;
      if (lower_y > upper_y) {
        std::swap(lower_y, upper_y);
      }
      regions_.emplace(regions_.size(), Region{road.GetStart().x - 0.4, road.GetStart().x + 0.4,
                                               lower_y - 0.4, upper_y + 0.4});
    }
  }
}

MoveInfo::Position GameSession::CalculateNewPosition(const MoveInfo::Position& position,
                                                     const MoveInfo::Speed& speed,
                                                     double delta_time) {
  MoveInfo::Position result{};

  result.x = position.x + speed.x * delta_time;
  result.y = position.y + speed.y * delta_time;

  return result;
}

bool GameSession::IsWithinAnyRegion(const MoveInfo::Position& pos,
                                    const std::unordered_map<int, Region>& regions) {
  for (const auto& [id, region] : regions) {
    if (region.Contains(pos)) {
      return true;
    }
  }
  return false;
}

MoveInfo::Position GameSession::FindStartingPosition() const {
  const auto& roads = map_.GetRoads();
  if (roads.empty()) {
    return {0.0, 0.0};
  }

  const auto& first_road = roads.front();
  const auto start = first_road.GetStart();

  return MoveInfo::Position{static_cast<double>(start.x), static_cast<double>(start.y)};
}

MoveInfo::Position GameSession::AdjustPositionToMaxRegion(const std::shared_ptr<Dog>& dog) {
  MoveInfo::Position max_pos = dog->GetPosition();
  double max_diff = 0;

  for (const auto& [region_id, region] : regions_) {
    if (region.Contains(dog->GetPosition())) {
      MoveInfo::Position possible_max_pos =
          MaxValueOfRegion(region, dog->GetDirection(), dog->GetPosition());
      MoveInfo::Position max_distance_pos =
          MoveOnMaxDistance(dog->GetPosition(), possible_max_pos, max_diff);

      double dx = max_distance_pos.x - dog->GetPosition().x;
      double dy = max_distance_pos.y - dog->GetPosition().y;
      double distance = std::sqrt(dx * dx + dy * dy);

      if (distance > max_diff) {
        max_diff = distance;
        max_pos = max_distance_pos;
      }
    }
  }

  return max_pos;
}

MoveInfo::Position GameSession::MaxValueOfRegion(Region reg, MoveInfo::Direction dir,
                                                 MoveInfo::Position current_pos) {
  MoveInfo::Position result = current_pos;
  if (dir == MoveInfo::Direction::EAST) {
    result.x = reg.max_x;
  } else if (dir == MoveInfo::Direction::WEST) {
    result.x = reg.min_x;
  } else if (dir == MoveInfo::Direction::SOUTH) {
    result.y = reg.max_y;
  } else if (dir == MoveInfo::Direction::NORTH) {
    result.y = reg.min_y;
  }

  return result;
}

MoveInfo::Position GameSession::MoveOnMaxDistance(const MoveInfo::Position& current,
                                                  const MoveInfo::Position& possible,
                                                  double current_max) {
  double dx = current.x - possible.x;
  double dy = current.y - possible.y;
  double dist = std::sqrt(dx * dx + dy * dy);

  if (dist > current_max) {
    return possible;
  }

  return current;
}

//***********************************************************
//--------------------------Game----------------------------

void Game::AddMap(Map map) {
  const size_t index = maps_.size();
  if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
    throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
  } else {
    try {
      maps_.emplace_back(std::move(map));
    } catch (...) {
      map_id_to_index_.erase(it);
      throw;
    }
  }
}

const Game::Maps& Game::GetMaps() const noexcept {
  return maps_;
}

std::shared_ptr<GameSession> Game::FindGameSession(Map::Id map_id) {
  if (!FindMap(map_id)) {
    return nullptr;
  }

  auto it = map_id_to_session_id_.find(map_id);
  if (it != map_id_to_session_id_.end()) {
    return FindGameSessionBySessionId(it->second);
  }

  return CreateGameSession(map_id);
}

std::shared_ptr<GameSession> Game::CreateGameSession(Map::Id map_id) {
  auto map_it = map_id_to_index_.find(map_id);
  if (map_it == map_id_to_index_.end()) {
    return nullptr;
  }

  const Map& map = maps_[map_it->second];
  auto session = std::make_shared<GameSession>(map);
  GameSession::Id session_id = session->GetSessionId();

  game_sessions_id_to_index_[session_id] = sessions_.size();
  sessions_.push_back(session);
  map_id_to_session_id_[map_id] = session_id;

  session->SetRandomSpawnMode(settings_.random_spawn);

  return session;
}

const Game::GameSessions& Game::GetGameSessions() const {
  return sessions_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
  if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
    return &maps_.at(it->second);
  }
  return nullptr;
}

void Game::SetDefaultDogSpeed(double default_speed) {
  settings_.default_dog_speed_ = default_speed;
}

double Game::GetDefaultDogSpeed() const {
  return settings_.default_dog_speed_;
}

const Game::Settings& Game::GetSettings() const {
  return settings_;
}
Game::Settings& Game::GetSettings() {
  return settings_;
}

void Game::SetLootGeneratorConfig(double period, double probability) {
  loot_generator_ = std::make_unique<loot_gen::LootGenerator>(
      std::chrono::milliseconds(static_cast<int>(period * 1000)), probability);
}

void Game::SetDefaultBagCapacity(int capacity) {
  settings_.default_bag_capacity_ = capacity;
}

int Game::GetDefaultBagCapacity() const {
  return settings_.default_bag_capacity_;
}

std::shared_ptr<GameSession> Game::FindGameSessionBySessionId(GameSession::Id session_id) {
  if (auto it = game_sessions_id_to_index_.find(session_id);
      it != game_sessions_id_to_index_.end()) {
    return sessions_.at(it->second);
  }
  return nullptr;
}

void Game::Tick(double delta_time) {
  for (const auto& session : sessions_) {
    session->Tick(delta_time);

    if (loot_generator_) {
      auto loot_count = session->GetLoots().size();
      auto looter_count = session->GetDogs().size();

      // Конвертируем delta_time (в секундах) в миллисекунды
      auto time_delta_ms = static_cast<int>(delta_time * 1000);
      auto new_loot_count = loot_generator_->Generate(
          loot_gen::LootGenerator::TimeInterval(time_delta_ms), loot_count, looter_count);

      if (new_loot_count > 0) {
        const auto* map = FindMap(session->GetMapId());
        if (map) {
          session->GenerateLoot(new_loot_count, map->GetLootTypesCount());
        }
      }
    }
  }
}

//------------------------Player-------------------------------------
Player::Player(std::shared_ptr<model::Dog> dog, std::shared_ptr<model::GameSession> game_session,
               double time)
    : dog_(dog), game_session_(game_session), join_game_(time) {
  size_t bag_capacity = game_session->GetMap().GetBagCapacity();
  dog_->SetBagCapacity(bag_capacity);
  game_session->AddDog(dog_);
}

const model::Dog::Id Player::GetDogId() const {
  return dog_->GetId();
}

void Player::MovePlayer(std::string direction) {
  dog_->SetDogDirSpeed(direction);
}

const std::shared_ptr<model::GameSession> Player::GetGameSession() const {
  return game_session_;
}

const std::shared_ptr<Dog> Player::GetDogPlayer() const {
  return dog_;
}

//******************************************************************
//--------------------------PlayerTokens----------------------------

Token Players::AddPlayer(std::shared_ptr<Dog> dog, std::shared_ptr<GameSession> game_session) {
  std::shared_ptr<Player> player = std::make_shared<Player>(dog, game_session, server_uptime_);
  Token token = player_tokens_.AddPlayer(player);
  players_[{dog->GetId(), *(game_session->GetMapId())}] =
      std::make_shared<Player>(dog, game_session);

  return token;
}

Token PlayerTokens::AddPlayer(std::shared_ptr<Player> player) {
  Token token = GenerateToken();
  AddToken(player, token);

  return token;
}

void PlayerTokens::AddToken(std::shared_ptr<Player> player, Token token) {
  token_to_player_[token] = player;
}

std::shared_ptr<Player> PlayerTokens::FindPlayerByToken(const Token& token) {
  auto player = token_to_player_.find(token);
  if (player != token_to_player_.end())
    return player->second;

  return nullptr;
}

Token PlayerTokens::GenerateToken() {
  std::ostringstream ss;
  ss << std::hex << std::setw(16) << std::setfill('0') << generator1_() << std::setw(16)
     << std::setfill('0') << generator2_();
  return Token{ss.str()};
}

//*******************************************************************
//----------------------------------Application----------------------

}  // namespace model
