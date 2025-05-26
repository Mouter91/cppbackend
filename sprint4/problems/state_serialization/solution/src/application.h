#pragma once

#include <boost/signals2.hpp>
#include <chrono>
#include <string>
#include <filesystem>

#include "model.h"
#include "extra_data.h"
#include "command_line.h"
#include "serialization.h"

namespace net = boost::asio;
using Strand = net::strand<net::io_context::executor_type>;

namespace app {

class Application {
 public:
  using milliseconds = std::chrono::milliseconds;
  using TickSignal = boost::signals2::signal<void(milliseconds)>;

  explicit Application(model::Game&& game, extra_data::MapsExtra&& extra);

  model::Game& GetGame();
  extra_data::MapsExtra& GetExtraData();
  model::Players& GetPlayers();

  void SetGameSettings(const std::optional<Args>& config);
  void SetGameTicker(const std::optional<Args>& config, Strand& strand);

  // Метод для подключения обработчиков тика
  [[nodiscard]] boost::signals2::connection DoOnTick(const TickSignal::slot_type& handler) {
    return tick_signal_.connect(handler);
  }

  void EnableStateSerialization(const std::filesystem::path& file_path,
                                milliseconds period);  // <-- NEW

  void Tick(double seconds);
  void Tick(milliseconds delta);

 private:
  model::Game game_;
  extra_data::MapsExtra maps_extra_;
  model::Players players_;
  TickSignal tick_signal_;

  std::unique_ptr<class SerializingListener> serializer_;  // <-- NEW
};

class SerializingListener {
 public:
  SerializingListener(app::Application& app, std::filesystem::path file_path,
                      std::chrono::milliseconds period)
      : app_(app), file_path_(std::move(file_path)), save_period_(period) {
    last_save_ = std::chrono::steady_clock::now();

    // Подключаемся к тикам приложения
    conn_ = app_.DoOnTick([this](std::chrono::milliseconds delta) { this->OnTick(delta); });
  }

 private:
  void OnTick(std::chrono::milliseconds) {
    auto now = std::chrono::steady_clock::now();
    if (now - last_save_ >= save_period_) {
      last_save_ = now;
      try {
        tmp_path_ = file_path_;
        tmp_path_ += ".tmp";

        std::ofstream out(tmp_path_, std::ios::trunc);
        if (!out)
          throw std::runtime_error("Failed to open tmp file for writing");

        model::SaveGame(app_.GetGame(), app_.GetPlayers(), out);
        out.close();

        if (!std::filesystem::exists(tmp_path_))
          throw std::runtime_error("Temp file was not created");

        std::filesystem::copy_file(tmp_path_, file_path_,
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(tmp_path_);

      } catch (const std::exception& ex) {
        std::cerr << "Failed to save game state periodically: " << ex.what() << std::endl;
      }
    }
  }

  app::Application& app_;
  std::filesystem::path file_path_;
  std::filesystem::path tmp_path_;
  std::chrono::milliseconds save_period_;
  std::chrono::steady_clock::time_point last_save_;
  boost::signals2::scoped_connection conn_;
};

}  // namespace app
