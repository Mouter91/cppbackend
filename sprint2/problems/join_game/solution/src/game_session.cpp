#include "game_session.h"

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

Application::Application(model::Game& game, Players& players) 
    : game_(game), players_(players) {}

const std::vector<std::string> Application::GetPlayersList(Token token) const {
    auto game_session = players_.GetPlayerByToken(token)->GetGameSession();            
    return game_session->GetPlayersNames();
}


