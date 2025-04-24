#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

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

const std::vector<std::string> GameSession::GetPlayersNames() const {
    std::vector<std::string> names;
    for (const auto& [dog_id, dog]: dogs_) {
        names.push_back(dog->GetName());
    }

    return names;
}

Token Players::AddPlayer(std::shared_ptr<Dog> dog, std::shared_ptr<GameSession> game_session) {
    std::shared_ptr<Player> player = std::make_shared<Player>(dog, game_session);
    Token token = player_tokens_.AddPlayer(player);
    players_[{dog->GetId(), *(game_session->GetMapId())}] = 
        std::make_shared<Player>(dog, game_session);

    return token;
}

Token PlayerTokens::AddPlayer(std::shared_ptr<Player> player) {
    Token token = GenerateToken();
    token_to_player_[token] = player;

    return token;
}

std::shared_ptr<Player> PlayerTokens::FindPlayerByToken(const Token& token) {
    auto player = token_to_player_.find(token);
    if (player != token_to_player_.end())
        return player->second;

    return nullptr;
}

Token PlayerTokens::GenerateToken() {
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << generator1_()
       << std::setw(16) << std::setfill('0') << generator2_();
    return Token{ss.str()};
}

Application::Application(Game& game, Players& players) 
    : game_(game), players_(players) {}

const std::vector<std::string> Application::GetPlayersList(Token token) const {
    auto game_session = players_.GetPlayerByToken(token)->GetGameSession();            
    return game_session->GetPlayersNames();
}

}  // namespace model
