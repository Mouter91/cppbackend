#pragma once

#include <chrono>
#include <pqxx/pqxx>
#include <memory>
#include <string>
#include "connection_pool.h"

namespace db {

class Database {
 public:
  explicit Database(const std::string& db_url);
  void Initialize();  // Проверяет и создаёт таблицу при необходимости

  // Методы для работы с таблицей retired_players
  void AddRetiredPlayer(const std::string& name, int score, double playTime);
  pqxx::result GetRetiredPlayers(int start = 0, int maxItems = 100);

 private:
  ConnectionPool pool_;
};

}  // namespace db
