#include "application.h"
#include <filesystem>
#include <fstream>

namespace app {

Application::Application(model::Game&& game, extra_data::MapsExtra&& extra, const Args* args)
    : game_(std::move(game)), maps_extra_(std::move(extra)) {
  if (args && !args->state_file.empty()) {
    if (args->save_state_period > 0) {
      AddListener(std::make_shared<SerializingListener>(
          *this, args->state_file, std::chrono::milliseconds(args->save_state_period)));
    }
  }
}

void Application::SaveState(const std::string& file_path) {
  try {
    // Сначала сохраняем во временный файл
    std::string tmp_path = file_path + ".tmp";
    {
      std::ofstream out(tmp_path, std::ios::binary);
      if (!out)
        throw std::runtime_error("Failed to open file for writing");
      model::SaveGame(game_, players_, out);
    }
    // Атомарно заменяем старый файл новым
    std::filesystem::rename(tmp_path, file_path);
  } catch (const std::exception& e) {
    std::cerr << "Failed to save state: " << e.what() << std::endl;
    throw;
  }
}

void Application::LoadState(const std::string& file_path) {
  try {
    if (!std::filesystem::exists(file_path)) {
      return;  // Файла нет - начинаем с чистого листа
    }

    std::ifstream in(file_path, std::ios::binary);
    if (!in)
      throw std::runtime_error("Failed to open file for reading");

    model::LoadGame(game_, players_, in);
  } catch (const std::exception& e) {
    std::cerr << "Failed to load state: " << e.what() << std::endl;
    throw;
  }
}

void Application::AddListener(std::shared_ptr<ApplicationListener> listener) {
  listeners_.emplace_back(std::move(listener));
}

void Application::Tick(std::chrono::milliseconds delta) {
  game_.Tick(static_cast<double>(delta.count()) / 1000.0);
  for (const auto& listener : listeners_) {
    listener->OnTick(delta);
  }
}

SerializingListener::SerializingListener(app::Application& app, std::string file_path,
                                         std::chrono::milliseconds save_period)
    : app_(app),
      file_path_(std::move(file_path)),
      save_period_(save_period),
      time_since_last_save_(0ms) {
}

void SerializingListener::OnTick(std::chrono::milliseconds delta) {
  time_since_last_save_ += delta;
  if (time_since_last_save_ >= save_period_) {
    time_since_last_save_ = 0ms;
    try {
      app_.SaveState(file_path_);
    } catch (const std::exception& e) {
      std::cerr << "Periodic save failed: " << e.what() << std::endl;
    }
  }
}
}  // namespace app
