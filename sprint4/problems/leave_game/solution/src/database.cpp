#include "database.h"
#include <chrono>

namespace db {

Database::Database(const std::string& db_url)
    : pool_(10, [db_url] {
        auto conn = std::make_shared<pqxx::connection>(db_url);
        if (!conn->is_open()) {
          throw std::runtime_error("Не удалось подключиться к БД");
        }
        return conn;
      }) {
#ifndef NDEBUG
  // Тестируем соединение сразу в debug-режиме
  auto conn = pool_.GetConnection();
  pqxx::nontransaction ntx(*conn);
  ntx.exec("SELECT 1");  // Простой тестовый запрос
#endif
}

void Database::Initialize() {
  auto conn_wrapper = pool_.GetConnection();
  pqxx::work txn(*conn_wrapper);

  try {
    // Создаём таблицу, если не существует
    txn.exec(
        "CREATE TABLE IF NOT EXISTS retired_players ("
        "id SERIAL PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "score INTEGER NOT NULL,"
        "play_time DOUBLE PRECISION NOT NULL"
        ")");

    // Создаём один составной индекс для сортировки по трём полям
    txn.exec(
        "CREATE INDEX IF NOT EXISTS idx_score_playtime_name "
        "ON retired_players (score DESC, play_time ASC, name ASC)");

    txn.commit();
  } catch (const pqxx::sql_error& e) {
    txn.abort();
    throw std::runtime_error("Ошибка инициализации БД: " + std::string(e.what()));
  }
}

void Database::AddRetiredPlayer(const std::string& name, int score, double playTime) {
  auto conn_wrapper = pool_.GetConnection();
  pqxx::work txn(*conn_wrapper);
  txn.exec_params("INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3)", name,
                  score, playTime);
  txn.commit();
}

pqxx::result Database::GetRetiredPlayers(int start, int maxItems) {
  auto conn_wrapper = pool_.GetConnection();
  pqxx::read_transaction rtxn(*conn_wrapper);
  return rtxn.exec_params(
      "SELECT name, score, play_time FROM retired_players "
      "ORDER BY score DESC, play_time ASC, name ASC "
      "LIMIT $1 OFFSET $2",
      maxItems, start);
}

}  // namespace db
