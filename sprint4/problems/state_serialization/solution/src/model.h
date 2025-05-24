#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <memory>
#include <random>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/functional/hash.hpp>

#include "tagged.h"
#include "move_info.h"
#include "ticker.h"
#include "loot_generator.h"
#include "collision_detector.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

struct Point {
  Coord x, y;
};

struct Size {
  Dimension width, height;
};

struct Rectangle {
  Point position;
  Size size;
};

struct Offset {
  Dimension dx, dy;
};

struct Region {
  double min_x, max_x;
  double min_y, max_y;

  bool Contains(const MoveInfo::Position& pos) const {
    return pos.x >= min_x && pos.x <= max_x && pos.y >= min_y && pos.y <= max_y;
  }
};

class Road {
  struct HorizontalTag {
    HorizontalTag() = default;
  };

  struct VerticalTag {
    VerticalTag() = default;
  };

 public:
  constexpr static HorizontalTag HORIZONTAL{};
  constexpr static VerticalTag VERTICAL{};

  Road(HorizontalTag, Point start, Coord end_x) noexcept : start_{start}, end_{end_x, start.y} {
  }

  Road(VerticalTag, Point start, Coord end_y) noexcept : start_{start}, end_{start.x, end_y} {
  }

  bool IsHorizontal() const noexcept {
    return start_.y == end_.y;
  }

  bool IsVertical() const noexcept {
    return start_.x == end_.x;
  }

  Point GetStart() const noexcept {
    return start_;
  }

  Point GetEnd() const noexcept {
    return end_;
  }

  std::pair<Coord, Coord> GetXBounds() const {
    return std::minmax(start_.x, end_.x);
  }

  std::pair<Coord, Coord> GetYBounds() const {
    return std::minmax(start_.y, end_.y);
  }

 private:
  Point start_;
  Point end_;
};

class Building {
 public:
  explicit Building(Rectangle bounds) noexcept : bounds_{bounds} {
  }

  const Rectangle& GetBounds() const noexcept {
    return bounds_;
  }

 private:
  Rectangle bounds_;
};

class Office {
 public:
  using Id = util::Tagged<std::string, Office>;

  Office(Id id, Point position, Offset offset) noexcept
      : id_{std::move(id)}, position_{position}, offset_{offset} {
  }

  const Id& GetId() const noexcept {
    return id_;
  }

  Point GetPosition() const noexcept {
    return position_;
  }

  Offset GetOffset() const noexcept {
    return offset_;
  }

 private:
  Id id_;
  Point position_;
  Offset offset_;
};

class RoadIndex {
 public:
  void AddRoad(const Road& road) {
    if (road.IsHorizontal()) {
      horizontal_roads_[road.GetStart().y].push_back(road);
    } else if (road.IsVertical()) {
      vertical_roads_[road.GetStart().x].push_back(road);
    }
  }

  const std::vector<Road>& GetHorizontalRoads(double y) const {
    static std::vector<Road> empty;
    auto it = horizontal_roads_.find(y);
    return it != horizontal_roads_.end() ? it->second : empty;
  }

  const std::vector<Road>& GetVerticalRoads(double x) const {
    static std::vector<Road> empty;
    auto it = vertical_roads_.find(x);
    return it != vertical_roads_.end() ? it->second : empty;
  }

 private:
  std::unordered_map<double, std::vector<Road>> horizontal_roads_;
  std::unordered_map<double, std::vector<Road>> vertical_roads_;
};

class Map {
 public:
  using Id = util::Tagged<std::string, Map>;
  using Roads = std::vector<Road>;
  using Buildings = std::vector<Building>;
  using Offices = std::vector<Office>;

  Map(Id id, std::string name) noexcept;

  const Id& GetId() const noexcept;
  const std::string& GetName() const noexcept;

  const Buildings& GetBuildings() const noexcept;
  const Roads& GetRoads() const noexcept;
  const Offices& GetOffices() const noexcept;

  void AddRoad(const Road& road);
  void AddBuilding(const Building& building);
  void AddOffice(Office office);

  void SetDefaultDogSpeed(double default_speed);
  double GetDefaultDogSpeed() const;
  bool IsDefaultDogSpeedValueConfigured() const;

  size_t GetLootTypesCount() const {
    return loot_values_.size();
  }

  const std::vector<int>& GetLootValues() const {
    return loot_values_;
  }

  void SetLootValues(std::vector<int> values) {
    loot_values_ = std::move(values);
  }

  void SetBagCapacity(int capacity) {
    bag_capacity_ = capacity;
  }

  size_t GetBagCapacity() const {
    return bag_capacity_;
  }

  bool IsBagCapacityConfigured() const {
    return bag_capacity_;
  }

 private:
  using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

  Id id_;
  std::string name_;
  Roads roads_;
  Buildings buildings_;

  OfficeIdToIndex warehouse_id_to_index_;
  Offices offices_;

  double default_dog_speed_ = 1.0;
  std::vector<int> loot_values_;

  size_t bag_capacity_;
};

class Bag {
 public:
  explicit Bag(size_t capacity) : capacity_(capacity) {
  }

  bool AddLoot(size_t loot_type) {
    if (items_.size() >= capacity_) {
      return false;  // Рюкзак полон
    }
    items_.push_back(loot_type);
    return true;
  }

  bool IsFull() const {
    return items_.size() >= capacity_;
  }

  void Clear() {
    items_.clear();
  }

  size_t GetSize() const {
    return items_.size();
  }

  size_t GetCapacity() const {
    return capacity_;
  }

  const std::vector<size_t>& GetItems() const {
    return items_;
  }

  void SetCapacity(size_t new_capacity) {
    capacity_ = new_capacity;
    if (items_.size() > capacity_) {
      items_.resize(capacity_);
    }
  }

 private:
  std::vector<size_t> items_;
  size_t capacity_;
};

class Dog {
 public:
  using Id = std::size_t;

  explicit Dog(std::string_view name, size_t bag_capacity = 3);
  explicit Dog(std::string_view name, MoveInfo state, size_t bag_capacity = 3);

  const Id GetId() const {
    return id_;
  }

  const std::string& GetName() const;

  const MoveInfo::Position& GetPosition() const noexcept;
  const MoveInfo::Speed& GetSpeed() const noexcept;
  const MoveInfo::Direction& GetDirection() const noexcept;
  const MoveInfo& GetState() const noexcept;

  void SetDefaultDogSpeed(double speed);
  void SetDogSpeed(double x, double y);
  void SetDogDirSpeed(std::string dir);

  const double GetDefaultDogSpeed() const;

  void StopDog();
  MoveInfo::Position MoveDog(MoveInfo::Position other_pos);

  Bag& GetBag();
  const Bag& GetBag() const;
  void SetBagCapacity(size_t capacity);

  int GetScore() const;
  void AddScore(int value);

 private:
  std::string name_;
  Id id_;
  int score_ = 0;

  MoveInfo state_;
  double default_dog_speed_ = 0.0;

  Bag bag_;
};

class GameSession {
 public:
  using Id = size_t;
  using Dogs = std::unordered_map<Dog::Id, std::shared_ptr<Dog>>;

  struct Loot {
    size_t type;  // индекс типа трофея
    int value;
    MoveInfo::Position position;
  };

  const std::vector<Loot>& GetLoots() const {
    return loots_;
  }

  void GenerateLoot(unsigned count, size_t loot_types_count) {
    const auto& roads = map_.GetRoads();
    if (roads.empty())
      return;

    const auto& loot_values = map_.GetLootValues();
    if (loot_values.empty())
      return;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> type_dist(0, loot_types_count - 1);
    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);

    for (unsigned i = 0; i < count; ++i) {
      const auto& road = roads[road_dist(gen)];
      Loot loot;
      loot.type = type_dist(gen);
      loot.value = loot_values[loot.type];

      if (road.IsHorizontal()) {
        auto [x0, x1] = road.GetXBounds();
        std::uniform_int_distribution<Coord> x_dist(x0, x1);
        loot.position = {static_cast<double>(x_dist(gen)), static_cast<double>(road.GetStart().y)};
      } else if (road.IsVertical()) {
        auto [y0, y1] = road.GetYBounds();
        std::uniform_int_distribution<Coord> y_dist(y0, y1);
        loot.position = {static_cast<double>(road.GetStart().x), static_cast<double>(y_dist(gen))};
      }

      loots_.push_back(loot);
    }
  }

  GameSession(const Map& map);
  Map::Id GetMapId() const;
  const Id GetSessionId() const;

  void AddDog(std::shared_ptr<Dog> dog);
  const Dogs& GetDogs() const;
  bool HasDog(Dog::Id id);

  const std::vector<std::string> GetPlayersNames() const;
  const std::vector<MoveInfo> GetPlayersUnitStates() const;

  double GetMapDefaultSpeed() const;

  void MovePlayer(Dog::Id id, double delta_time);
  void StopPlayer(Dog::Id id);
  void Tick(double delta_time);

  void ProcessCollisions(double delta_time);

  const Map& GetMap() const {
    return map_;
  }

  void InitializeRegions();

  void SetRandomSpawnMode(bool enable) {
    random_spawn_mode_ = enable;
  }

  bool IsRandomSpawnEnabled() const {
    return random_spawn_mode_;
  }

  void AddLoot(const Loot& loot) {
    loots_.push_back(loot);
  }

 private:
  static Id GenerateId() {
    static boost::uuids::random_generator gen;
    boost::uuids::uuid uuid = gen();
    return boost::hash<boost::uuids::uuid>()(uuid);
  }

  MoveInfo::Position CalculateNewPosition(const MoveInfo::Position& position,
                                          const MoveInfo::Speed& speed, double delta_time);

  bool IsWithinAnyRegion(const MoveInfo::Position& pos,
                         const std::unordered_map<int, Region>& regions);

  MoveInfo::Position FindStartingPosition() const;
  MoveInfo::Position AdjustPositionToMaxRegion(const std::shared_ptr<Dog>& dog);
  MoveInfo::Position MaxValueOfRegion(Region reg, MoveInfo::Direction dir,
                                      MoveInfo::Position current_pos);
  MoveInfo::Position MoveOnMaxDistance(const MoveInfo::Position& current,
                                       const MoveInfo::Position& possible, double current_max);
  Dogs dogs_;
  const Map& map_;
  Id id_;

  RoadIndex road_index_;
  std::unordered_map<int, Region> regions_;

  bool random_spawn_mode_ = false;
  std::vector<Loot> loots_;
};

class Player {
 public:
  Player() = delete;
  Player(std::shared_ptr<Dog> dog, std::shared_ptr<GameSession> game_session);

  const Dog::Id GetDogId() const;
  const std::shared_ptr<GameSession> GetGameSession() const;
  const std::shared_ptr<Dog> GetDogPlayer() const;

  void MovePlayer(std::string direction = "");

 private:
  std::shared_ptr<Dog> dog_;
  std::shared_ptr<GameSession> game_session_;
};

class PlayerTokens {
 public:
  using TokenHasher = util::TaggedHasher<Token>;
  PlayerTokens() = default;

  Token AddPlayer(std::shared_ptr<Player> player);

  std::shared_ptr<Player> FindPlayerByToken(const Token& token);
  Token FindTokenByPlayer(std::shared_ptr<Player> player);

  void AddToken(std::shared_ptr<Player> player, Token token);

  std::unordered_map<Token, std::shared_ptr<Player>, TokenHasher>& GetTokenToPlayer() {
    return token_to_player_;
  }

 private:
  std::unordered_map<Token, std::shared_ptr<Player>, TokenHasher> token_to_player_;
  std::unordered_map<std::shared_ptr<Player>, Token> player_to_token_;

  std::random_device random_device_;
  std::mt19937_64 generator1_{[this] {
    std::uniform_int_distribution<std::mt19937_64::result_type> dist;
    return dist(random_device_);
  }()};
  std::mt19937_64 generator2_{[this] {
    std::uniform_int_distribution<std::mt19937_64::result_type> dist;
    return dist(random_device_);
  }()};

  Token GenerateToken();
};

class Players {
 public:
  using AllPlayers = std::unordered_map<std::pair<Dog::Id, std::string>, std::shared_ptr<Player>,
                                        boost::hash<std::pair<Dog::Id, std::string>>>;

  Token AddPlayer(std::shared_ptr<Dog> dog, std::shared_ptr<GameSession> game_session);

  std::shared_ptr<Player> GetPlayerByToken(Token token) {
    return player_tokens_.FindPlayerByToken(token);
  }

  std::shared_ptr<Player> FindByDogAndMapId(Dog::Id dog_id, Map::Id map_id) {
    return players_[{dog_id, *map_id}];
  }

  PlayerTokens& GetPlayerTokens() {
    return player_tokens_;
  }

  AllPlayers& GetAllPlayers() {
    return players_;
  }

  void AddPlayerWithToken(std::shared_ptr<Player> player, Token token);

 private:
  PlayerTokens player_tokens_;
  AllPlayers players_;
};

class Game {
 public:
  using Maps = std::vector<Map>;
  using GameSessions = std::vector<std::shared_ptr<GameSession>>;
  using MapIdHasher = util::TaggedHasher<Map::Id>;

  struct Settings {
    bool random_spawn = false;
    std::shared_ptr<game_time::Ticker> ticker = nullptr;

    double default_dog_speed_ = 1.0;
    int default_bag_capacity_ = 3;

    double probability = 0;
    double period = 0;

    bool IsAutoTickEnabled() const {
      return ticker != nullptr;
    }
  };

  void AddMap(Map map);
  const Maps& GetMaps() const noexcept;
  const Map* FindMap(const Map::Id& id) const noexcept;

  std::shared_ptr<GameSession> CreateGameSession(Map::Id map_id);
  std::shared_ptr<GameSession> FindGameSession(Map::Id map_id);
  const GameSessions& GetGameSessions() const;

  const Settings& GetSettings() const;
  Settings& GetSettings();

  void SetDefaultDogSpeed(double default_speed);
  double GetDefaultDogSpeed() const;
  void SetDefaultBagCapacity(int capacity);
  int GetDefaultBagCapacity() const;

  void Tick(double delta_time);

  void SetLootGeneratorConfig(double period, double probability);

  const std::unordered_map<Map::Id, size_t, MapIdHasher>& GetMapIdToIndex() const {
    return map_id_to_index_;
  }

  const std::unordered_map<Map::Id, GameSession::Id, MapIdHasher>& GetMapIdToSessionId() const {
    return map_id_to_session_id_;
  }

  const std::unordered_map<GameSession::Id, size_t>& GetGameSessionsIdToIndex() const {
    return game_sessions_id_to_index_;
  }

 private:
  std::shared_ptr<GameSession> FindGameSessionBySessionId(GameSession::Id session_id);

  Maps maps_;
  GameSessions sessions_;
  Settings settings_;

  std::unordered_map<Map::Id, size_t, MapIdHasher> map_id_to_index_;
  std::unordered_map<Map::Id, GameSession::Id, MapIdHasher> map_id_to_session_id_;
  std::unordered_map<GameSession::Id, size_t> game_sessions_id_to_index_;

  std::unique_ptr<loot_gen::LootGenerator> loot_generator_;
};

class GameItemGathererProvider : public collision_detector::ItemGathererProvider {
 public:
  GameItemGathererProvider(const GameSession& session) : session_(session) {
  }

  size_t ItemsCount() const override {
    return session_.GetLoots().size() + session_.GetMap().GetOffices().size();
  }

  collision_detector::Item GetItem(size_t idx) const override {
    const auto& loots = session_.GetLoots();
    const auto& offices = session_.GetMap().GetOffices();

    if (idx < loots.size()) {
      // Это предмет (loot)
      return {
          {loots[idx].position.x, loots[idx].position.y},
          0.0  // Ширина предмета 0
      };
    } else {
      // Это офис (база)
      idx -= loots.size();
      const auto& office = offices[idx];
      return {
          {static_cast<double>(office.GetPosition().x),
           static_cast<double>(office.GetPosition().y)},
          0.5  // Ширина базы 0.5
      };
    }
  }

  size_t GatherersCount() const override {
    return session_.GetDogs().size();
  }

  collision_detector::Gatherer GetGatherer(size_t idx) const override {
    auto it = session_.GetDogs().begin();
    std::advance(it, idx);
    const auto& dog = *it->second;

    return {
        {dog.GetState().position.x, dog.GetState().position.y},  // start_pos
        {dog.GetState().position.x + dog.GetState().speed.x * delta_time_,
         dog.GetState().position.y + dog.GetState().speed.y * delta_time_},  // end_pos
        0.6                                                                  // Ширина игрока 0.6
    };
  }

 private:
  const GameSession& session_;
  double delta_time_;  // Добавляем поле для хранения времени тика
};
}  // namespace model
