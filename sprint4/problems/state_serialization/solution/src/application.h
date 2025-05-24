#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include "model.h"
#include "extra_data.h"
#include "serialization.h"
#include "command_line.h"
#include "ticker.h"

namespace net = boost::asio;

using Strand = net::strand<net::io_context::executor_type>;

namespace app {

class ApplicationListener {
 public:
  virtual void OnTick(std::chrono::milliseconds delta) = 0;
  virtual ~ApplicationListener() = default;
};

class Application {
 public:
  explicit Application(model::Game&& game, extra_data::MapsExtra&& extra,
                       const Args* args = nullptr);

  model::Game& GetGame() {
    return game_;
  }
  extra_data::MapsExtra& GetExtraData() {
    return maps_extra_;
  }
  model::Players& GetPlayers() {
    return players_;
  }

  void SetGameSettings(const std::optional<Args>& config, Strand& strand) {
    if (!config)
      return;  // Обязательно проверяем на наличие значения

    game_.GetSettings().random_spawn = config->randomize_spawn_points;

    if (config->tick_period > 0) {
      game_.GetSettings().ticker = std::make_shared<game_time::Ticker>(
          strand, std::chrono::milliseconds(config->tick_period),
          [this](std::chrono::milliseconds delta) {
            game_.Tick(static_cast<double>(delta.count()) / 1000.0);
          });
      game_.GetSettings().ticker->Start();
    }
  }

  void AddListener(std::shared_ptr<ApplicationListener> listener);
  void Tick(std::chrono::milliseconds delta);

  void SaveState(const std::string& file_path);
  void LoadState(const std::string& file_path);

 private:
  model::Game game_;
  extra_data::MapsExtra maps_extra_;
  model::Players players_;
  std::vector<std::shared_ptr<ApplicationListener>> listeners_;
};

class SerializingListener : public ApplicationListener {
 public:
  SerializingListener(Application& app, std::string file_path,
                      std::chrono::milliseconds save_period);

  void OnTick(std::chrono::milliseconds delta) override;

 private:
  Application& app_;
  std::string file_path_;
  std::chrono::milliseconds save_period_;
  std::chrono::milliseconds time_since_last_save_;
};

}  // namespace app
