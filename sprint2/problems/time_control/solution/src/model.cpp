#include "model.h"
#include "move_info.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

//---------------------------Map---------------------------

Map::Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {}

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

Dog::Dog(std::string_view name)
    : name_(name),
    id_(boost::hash<std::string>()(std::string(name))) {}


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

void Dog::StopDog() {
   SetDogSpeed(0, 0);
}

MoveInfo::Position Dog::MoveDog(MoveInfo::Position other_pos) {
    state_.position = other_pos;
    return state_.position;
}

//************************************************************
//---------------------------GameSession----------------------

GameSession::GameSession(const Map& map)
    : map_(map), id_(GenerateId()) {
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

GameSession::Id GameSession::GetSessionId() {
    return id_;
}

const std::vector<std::string> GameSession::GetPlayersNames() const {
    std::vector<std::string> names;
    for (const auto& [dog_id, dog]: dogs_) {
        names.push_back(dog->GetName());
    }

    return names;
}

const std::vector<MoveInfo> GameSession::GetPlayersUnitStates() const {
    std::vector<MoveInfo> dogs;
    for (const auto& [dog_id, dog]: dogs_) {
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

void GameSession::Tick(double delta_time) {
    for (auto& [id, dog] : dogs_) {
        MovePlayer(id, delta_time);
    }
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

    return session;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

void Game::SetDefaultDogSpeed(double default_speed) {
    default_dog_speed_ = default_speed; 
}


double Game::GetDefaultDogSpeed() const {
    return default_dog_speed_;
}

std::shared_ptr<GameSession> Game::FindGameSessionBySessionId(GameSession::Id session_id) {
    if (auto it = game_sessions_id_to_index_.find(session_id); it != game_sessions_id_to_index_.end()) {
        return sessions_.at(it->second);
    }
    return nullptr;
}

void Game::Tick(double delta_time) {
    for (const auto& session: sessions_) {
        session->Tick(delta_time);
    }
}

//------------------------Player-------------------------------------
Player::Player(std::shared_ptr<model::Dog> dog, std::shared_ptr<model::GameSession> game_session):
    dog_(dog), 
    game_session_(game_session) {
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

//******************************************************************
//--------------------------PlayerTokens----------------------------

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

//*******************************************************************
//----------------------------------Application----------------------



}  // namespace model
