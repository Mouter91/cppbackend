#include "application.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace app {

using namespace std::chrono_literals;

SerializingListener::SerializingListener(model::Game& game, model::Players& players,
                                         const std::string& file_path, milliseconds period)
    : game_(game), players_(players), file_path_(file_path), period_(period) {
}

void SerializingListener::OnTick(milliseconds delta) {
  elapsed_ += delta;
  if (elapsed_ >= period_) {
    elapsed_ = 0ms;
    try {
      SaveState();
    } catch (const std::exception& e) {
      std::cerr << "Failed to save state in listener: " << e.what() << std::endl;
    }
  }
}

void SerializingListener::SaveState() {
  std::string tmp_path = file_path_ + ".tmp";
  std::ofstream ofs(tmp_path);
  if (!ofs.is_open()) {
    throw std::ios_base::failure("Failed to open temporary file for saving");
  }

  model::SaveGame(game_, players_, ofs);

  std::filesystem::rename(tmp_path, file_path_);
  if (!std::filesystem::exists(file_path_)) {
    throw std::runtime_error("Failed to verify saved state file");
  }
}

Application::Application(model::Game&& game, extra_data::MapsExtra&& extra, const Args* args)
    : game_(std::move(game)), maps_extra_(std::move(extra)) {
  if (args && !args->state_file.empty()) {
    LoadState(args->state_file);
  }
}

void Application::SaveState(const std::string& file_path) {
  try {
    std::string tmp_path = file_path + ".tmp";
    std::ofstream ofs(tmp_path);

    if (!ofs.is_open()) {
      throw std::ios_base::failure("Failed to open temporary file for saving");
    }

    model::SaveGame(game_, players_, ofs);

    // Атомарная замена
    std::filesystem::rename(tmp_path, file_path);
  } catch (const std::exception& e) {
    std::cerr << "Failed to save state: " << e.what() << std::endl;
    throw;
  }
}

void Application::LoadState(const std::string& file_path) {
  try {
    if (!std::filesystem::exists(file_path)) {
      return;
    }

    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
      throw std::runtime_error("Failed to open file for reading");
    }

    model::LoadGame(game_, players_, in);
  } catch (const std::exception& e) {
    std::cerr << "Failed to load state: " << e.what() << std::endl;
    throw;
  }
}

void Application::SetGameSettings(const std::optional<Args>& config, Strand& strand) {
  if (!config)
    return;

  game_.GetSettings().random_spawn = config->randomize_spawn_points;

  if (config->tick_period > 0) {
    game_.GetSettings().ticker = std::make_shared<game_time::Ticker>(
        strand, std::chrono::milliseconds(config->tick_period), [this](milliseconds delta) {
          this->Tick(delta);  // ✅ вызывает тик приложения и сигнал
        });
    game_.GetSettings().ticker->Start();
  }
}

void Application::Tick(milliseconds delta) {
  game_.Tick(static_cast<double>(delta.count()) / 1000.0);
  tick_signal_(delta);

  // Уведомляем всех слушателей
  for (auto& listener : listeners_) {
    listener->OnTick(delta);
  }
}
}  // namespace app
