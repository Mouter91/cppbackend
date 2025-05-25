#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <boost/signals2.hpp>

#include "model.h"
#include "extra_data.h"
#include "serialization.h"
#include "command_line.h"
#include "ticker.h"

namespace net = boost::asio;
namespace sig = boost::signals2;

using Strand = net::strand<net::io_context::executor_type>;

namespace app {

using milliseconds = std::chrono::milliseconds;

class ApplicationListener {
 public:
  virtual ~ApplicationListener() = default;
  virtual void OnTick(milliseconds delta) = 0;
};

class SerializingListener : public ApplicationListener {
 public:
  SerializingListener(model::Game& game, model::Players& players, const std::string& file_path,
                      milliseconds period);

  void OnTick(milliseconds delta) override;

 private:
  void SaveState();

  model::Game& game_;
  model::Players& players_;
  std::string file_path_;
  milliseconds period_;
  milliseconds elapsed_{0ms};
};

class Application {
 public:
  using TickSignal = sig::signal<void(milliseconds)>;

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

  void SetGameSettings(const std::optional<Args>& config, Strand& strand);

  void Tick(milliseconds delta);

  void SaveState(const std::string& file_path);
  void LoadState(const std::string& file_path);

  [[nodiscard]] sig::connection DoOnTick(const TickSignal::slot_type& handler) {
    return tick_signal_.connect(handler);
  }

  void AddListener(std::unique_ptr<ApplicationListener> listener) {
    listeners_.push_back(std::move(listener));
  }

 private:
  model::Game game_;
  extra_data::MapsExtra maps_extra_;
  model::Players players_;
  TickSignal tick_signal_;
  std::vector<std::unique_ptr<ApplicationListener>> listeners_;
};

}  // namespace app
