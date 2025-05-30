#include "database.h"
#include <stdexcept>

namespace db {

Database::Database(const std::string& db_url)
    : pool_(4, [&db_url] { return std::make_shared<pqxx::connection>(db_url); }) {
}

void Database::Initialize() {
  auto conn_wrapper = pool_.GetConnection();
  pqxx::work txn(*conn_wrapper);

  try {
    // Проверяем существование таблицы
    txn.exec("SELECT 1 FROM retired_players LIMIT 1");
  } catch (const pqxx::undefined_table&) {
    // Таблицы нет — создаём
    txn.exec(
        "CREATE TABLE retired_players ("
        "id SERIAL PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "score INTEGER NOT NULL,"
        "play_time DOUBLE PRECISION NOT NULL"
        ")");

    // Создаём индексы для сортировки
    txn.exec("CREATE INDEX idx_score ON retired_players (score DESC)");
    txn.exec("CREATE INDEX idx_play_time ON retired_players (play_time ASC)");
    txn.exec("CREATE INDEX idx_name ON retired_players (name ASC)");

    txn.commit();
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
  pqxx::work txn(*conn_wrapper);
  return txn.exec_params(
      "SELECT name, score, play_time FROM retired_players "
      "ORDER BY score DESC, play_time ASC, name ASC "
      "LIMIT $1 OFFSET $2",
      maxItems, start);
}

}  // namespace db
