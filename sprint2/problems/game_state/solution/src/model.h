#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <memory>
#include <random>
#include <unordered_map>
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

namespace model {

using Dimension = int;
using Coord = Dimension;

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

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
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

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
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
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
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

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

namespace detail {
struct TokenTag {};
}

using Token = util::Tagged<std::string, detail::TokenTag>;

class Dog {
public:
    using Id = std::size_t;

    explicit Dog(std::string_view name)
        : name_(name),
          id_(boost::hash<std::string>()(std::string(name))) {}

    Id GetId() const {
        return id_;
    }

    const std::string& GetName() const {
        return name_;
    }

    const MoveInfo::Point& GetPosition() const noexcept {
        return state_.position;
    }

    const MoveInfo::Speed& GetSpeed() const noexcept {
        return state_.speed;
    }

    const MoveInfo::Direction& GetDirection() const noexcept {
        return state_.direction;
    }

    const MoveInfo& GetState() const noexcept {
        return state_;
    }

private:
    std::string name_;
    Id id_;

    MoveInfo state_;
};

class GameSession {
public:
    using Id = size_t;
    using Dogs = std::unordered_map<Dog::Id, std::shared_ptr<Dog>>;

    GameSession(const Map& map) : map_(map), id_(GenerateId()){}

    Map::Id GetMapId() const {
        return map_.GetId();
    }

    void AddDog(std::shared_ptr<Dog> dog) {
        dogs_.insert({dog->GetId(), dog});
    }

    const Dogs& GetDogs() const {
        return dogs_;
    }

    Id GetSessionId() {
        return id_;
    }

    const std::vector<MoveInfo> GetPlayersUnitStates() const {
        std::vector<MoveInfo> dogs;
        for (const auto& [dog_id, dog]: dogs_) {
            dogs.push_back(dog->GetState());
        }

        return dogs;
    }

    const std::vector<std::string> GetPlayersNames() const;

private:
    static Id GenerateId() {
        static boost::uuids::random_generator gen;
        boost::uuids::uuid uuid = gen();
        return boost::hash<boost::uuids::uuid>()(uuid);
    }

    Dogs dogs_;
    const Map& map_;
    Id id_;
};

class Player {
	public:
        Player() = delete;
        Player(std::shared_ptr<Dog> dog, std::shared_ptr<GameSession> game_session):
            dog_(dog), 
            game_session_(game_session) {
            game_session->AddDog(dog_);
        }

        Dog::Id GetDogId() {
            return dog_->GetId();
        }

        const std::shared_ptr<GameSession> GetGameSession() const {
            return game_session_;
        }

	private:
        std::shared_ptr<Dog> dog_;
		std::shared_ptr<GameSession> game_session_;
		
};

class PlayerTokens {
public:
    PlayerTokens() = default;

    Token AddPlayer(std::shared_ptr<Player> player);
    std::shared_ptr<Player> FindPlayerByToken(const Token& token);

private:
    using TokenHasher = util::TaggedHasher<Token>;
    std::unordered_map<Token, std::shared_ptr<Player>, TokenHasher> token_to_player_;

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
    Token AddPlayer(std::shared_ptr<Dog> dog, std::shared_ptr<GameSession> game_session);

    std::shared_ptr<Player> GetPlayerByToken(Token token) {
        return player_tokens_.FindPlayerByToken(token);
    }

    std::shared_ptr<Player> FindByDogAndMapId(Dog::Id dog_id, Map::Id map_id) {
        return players_[{dog_id, *map_id}];
    }

private:
    PlayerTokens player_tokens_;
    std::unordered_map<std::pair<Dog::Id, std::string>, 
        std::shared_ptr<Player>, boost::hash<std::pair<Dog::Id, std::string>>> players_;

};



class Game {
public:
    using Maps = std::vector<Map>;
    using GameSessions = std::vector<std::shared_ptr<GameSession>>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    std::shared_ptr<GameSession> FindGameSession(Map::Id map_id) {
        if (!FindMap(map_id)) {
            return nullptr;
        }

        auto it = map_id_to_session_id_.find(map_id);
        if (it != map_id_to_session_id_.end()) {
            return FindGameSessionBySessionId(it->second);
        }

        return CreateGameSession(map_id);
    }

    std::shared_ptr<GameSession> CreateGameSession(Map::Id map_id) {
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

        return session;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

private:
    std::shared_ptr<GameSession> FindGameSessionBySessionId(GameSession::Id session_id) {
        if (auto it = game_sessions_id_to_index_.find(session_id); it != game_sessions_id_to_index_.end()) {
            return sessions_.at(it->second);
        }
        return nullptr;
    }

    using MapIdHasher = util::TaggedHasher<Map::Id>;

    std::vector<Map> maps_;
    std::unordered_map<Map::Id, size_t, MapIdHasher> map_id_to_index_;
    std::unordered_map<Map::Id, GameSession::Id, MapIdHasher> map_id_to_session_id_;

    GameSessions sessions_;
    std::unordered_map<GameSession::Id, size_t> game_sessions_id_to_index_;
};

class Application {
public:
    explicit Application(Game& game, Players& players);
    
    const std::vector<std::string> GetPlayersList(Token token) const;

private:
    Game& game_;
    Players& players_;

};


}  // namespace model
