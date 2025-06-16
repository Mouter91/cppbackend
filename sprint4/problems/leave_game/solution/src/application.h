#pragma once

#include <boost/signals2.hpp>
#include <boost/algorithm/string/split.hpp>
#include <chrono>
#include <string>
#include <filesystem>
#include <fstream>

#include "model.h"
#include "extra_data.h"
#include "command_line.h"
#include "serialization.h"
#include "database.h"

namespace net = boost::asio;
namespace sig = boost::signals2;

using Strand = net::strand<net::io_context::executor_type>;

namespace app {

class Application {
 public:
  using milliseconds = std::chrono::milliseconds;
  using TickSignal = boost::signals2::signal<void(milliseconds)>;

  explicit Application(model::Game&& game, extra_data::MapsExtra&& extra,
                       const std::string& db_url);
  ~Application();

  model::Game& GetGame();
  extra_data::MapsExtra& GetExtraData();
  model::Players& GetPlayers();

  void SetGameSettings(const std::optional<Args>& config);
  void SetGameTicker(const std::optional<Args>& config, Strand& strand);
  void SetSaveSettings(const std::optional<Args>& config);

  // Метод для ручного вызова при обработке /api/v1/game/tick
  void ManualTick(milliseconds delta);

  void SaveStateBeforeExit();

  db::Database& GetDatabase() {
    return *database_;
  }

 private:
  void TrySaveState(milliseconds delta);
  void AtomicSave();

  model::Game game_;
  extra_data::MapsExtra maps_extra_;
  model::Players players_;
  std::unique_ptr<db::Database> database_;
  TickSignal tick_signal_;

  sig::connection players_connection_;

  // Настройки сохранения
  std::filesystem::path save_filepath_;
  std::filesystem::path temp_save_filepath_;
  milliseconds save_period_{0};
  milliseconds time_since_last_save_{0};
  std::ofstream save_stream_;
  sig::connection save_connection_;
};

}  // namespace app
