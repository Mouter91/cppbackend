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

namespace detail {
struct TokenTag {};
}

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
        return pos.x >= min_x && pos.x <= max_x &&
               pos.y >= min_y && pos.y <= max_y;
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

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;

    double default_dog_speed_ = 1.0;
};



class Dog {
public:
    using Id = std::size_t;

    explicit Dog(std::string_view name);

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

    void StopDog();
    MoveInfo::Position MoveDog(MoveInfo::Position other_pos);

private:
    std::string name_;
    Id id_;

    MoveInfo state_;
    double default_dog_speed_ = 0.0;
};

class GameSession {
public:
    using Id = size_t;
    using Dogs = std::unordered_map<Dog::Id, std::shared_ptr<Dog>>;

    GameSession(const Map& map);
    Map::Id GetMapId() const;
    Id GetSessionId();

    void AddDog(std::shared_ptr<Dog> dog);
    const Dogs& GetDogs() const;
    bool HasDog(Dog::Id id);

    const std::vector<std::string> GetPlayersNames() const;
    const std::vector<MoveInfo> GetPlayersUnitStates() const; 

    double GetMapDefaultSpeed() const;

    void MovePlayer(Dog::Id id, double delta_time);
    void StopPlayer(Dog::Id id);
    void Tick(double delta_time);

    void InitializeRegions();    

private:
    static Id GenerateId() {
        static boost::uuids::random_generator gen;
        boost::uuids::uuid uuid = gen();
        return boost::hash<boost::uuids::uuid>()(uuid);
    }

    MoveInfo::Position CalculateNewPosition(const MoveInfo::Position& position, const MoveInfo::Speed& speed, double delta_time);
   bool IsWithinAnyRegion(const MoveInfo::Position& pos, const std::unordered_map<int, Region>& regions);
   MoveInfo::Position FindStartingPosition() const;
   MoveInfo::Position AdjustPositionToMaxRegion(const std::shared_ptr<Dog>& dog);
   MoveInfo::Position MaxValueOfRegion(Region reg, MoveInfo::Direction dir, MoveInfo::Position current_pos);
   MoveInfo::Position  MoveOnMaxDistance(const MoveInfo::Position& current, const MoveInfo::Position& possible, double current_max);    
    Dogs dogs_;
    const Map& map_;
    Id id_;

    RoadIndex road_index_;
    std::unordered_map<int, Region> regions_;
    
};

class Player {
	public:
        Player() = delete;
        Player(std::shared_ptr<Dog> dog, std::shared_ptr<GameSession> game_session);

        const Dog::Id GetDogId() const;
        const std::shared_ptr<GameSession> GetGameSession() const;
        void MovePlayer(std::string direction = "");

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
    const Maps& GetMaps() const noexcept;
    const Map* FindMap(const Map::Id& id) const noexcept;


    std::shared_ptr<GameSession> CreateGameSession(Map::Id map_id);
    std::shared_ptr<GameSession> FindGameSession(Map::Id map_id);

    void SetDefaultDogSpeed(double default_speed);
    double GetDefaultDogSpeed() const;

    void Tick(double delta_time);

private:
    std::shared_ptr<GameSession> FindGameSessionBySessionId(GameSession::Id session_id); 

    using MapIdHasher = util::TaggedHasher<Map::Id>;

    std::vector<Map> maps_;
    std::unordered_map<Map::Id, size_t, MapIdHasher> map_id_to_index_;
    std::unordered_map<Map::Id, GameSession::Id, MapIdHasher> map_id_to_session_id_;

    GameSessions sessions_;
    std::unordered_map<GameSession::Id, size_t> game_sessions_id_to_index_;

    double default_dog_speed_ = 1.0;
};




}  // namespace model
