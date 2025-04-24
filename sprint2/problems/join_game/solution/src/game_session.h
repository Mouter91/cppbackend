#pragma once

#include "sdk.h"
#include "model.h"

#include <cstdint>
#include <memory>
#include <random>
#include <unordered_map>
#include <iostream>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/functional/hash.hpp>

using namespace model;

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

private:
    std::string name_;
    Id id_;
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

class Application {
public:
    explicit Application(model::Game& game, Players& players);
    
    const std::vector<std::string> GetPlayersList(Token token) const;

private:
    model::Game& game_;
    Players& players_;

};
