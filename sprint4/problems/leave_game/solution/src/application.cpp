// application.cpp
#include "application.h"

namespace app {

Application::Application(model::Game&& game, extra_data::MapsExtra&& extra,
                         const std::string db_url)
    : game_(std::move(game)),
      maps_extra_(std::move(extra)),
      database_(std::make_unique<db::Database>(db_url)) {
  database_->Initialize();  // Проверяем и создаем таблицы при необходимости
  players_.SetTimeWaitDog(game.GetSettings().dog_retirement_time);
}

Application::~Application() {
  if (players_connection_.connected()) {
    players_connection_.disconnect();
  }
  if (save_connection_.connected()) {
    save_connection_.disconnect();
  }

  if (save_stream_.is_open()) {
    save_stream_.close();
  }
}

model::Game& Application::GetGame() {
  return game_;
}

extra_data::MapsExtra& Application::GetExtraData() {
  return maps_extra_;
}

model::Players& Application::GetPlayers() {
  return players_;
}

void Application::SetGameSettings(const std::optional<Args>& config) {
  if (!config)
    return;

  game_.GetSettings().random_spawn = config->randomize_spawn_points;
}

void Application::SetGameTicker(const std::optional<Args>& config, Strand& strand) {
  if (!config)
    return;

  if (config->tick_period > 0) {
    game_.GetSettings().ticker = std::make_shared<game_time::Ticker>(
        strand, std::chrono::milliseconds(config->tick_period),
        [&](std::chrono::milliseconds delta) {
          // Основной тик игры
          game_.Tick(static_cast<double>(delta.count()) / 1000.0);

          // Сигнал тика (может использоваться другими подписчиками)
          tick_signal_(delta);
        });
    game_.GetSettings().ticker->Start();
  }
  players_connection_ = tick_signal_.connect([this](milliseconds delta) {
    players_.OnTick(static_cast<double>(delta.count()) / 1000.0, &*database_);
  });
}

void Application::SetSaveSettings(const std::optional<Args>& config) {
  if (!config || config->state_file.empty() || config->save_state_period <= 0)
    return;

  save_filepath_ = config->state_file;
  temp_save_filepath_ = save_filepath_.string() + ".tmp";  // Временный файл
  save_period_ = std::chrono::milliseconds(config->save_state_period);

  // Открываем временный файл для записи
  save_stream_.open(temp_save_filepath_);
  if (!save_stream_.is_open()) {
    throw std::runtime_error("Failed to open save file: " + temp_save_filepath_.string());
  }

  // Подписываемся на сигнал тика
  if (game_.GetSettings().ticker) {
    save_connection_ =
        tick_signal_.connect([this](milliseconds delta) { this->TrySaveState(delta); });
  }
}

void Application::ManualTick(milliseconds delta) {
  game_.Tick(static_cast<double>(delta.count()) / 1000.0);
  tick_signal_(delta);

  // Если нет автоматических тиков, проверяем сохранение здесь
  if (!game_.GetSettings().ticker) {
    TrySaveState(delta);
  }
}

void Application::TrySaveState(milliseconds delta) {
  if (!save_stream_.is_open() || save_period_.count() <= 0)
    return;

  time_since_last_save_ += delta;

  if (time_since_last_save_ >= save_period_) {
    AtomicSave();
  }
}

void Application::SaveStateBeforeExit() {
  if (!save_filepath_.empty()) {
    AtomicSave();
  }
}

void Application::AtomicSave() {
  if (!save_stream_.is_open())
    return;

  try {
    // 1. Записываем данные во временный файл
    save_stream_.seekp(0);
    save_stream_.clear();
    model::SaveGame(game_, players_, save_stream_);
    save_stream_.flush();

    // 2. Атомарно переименовываем временный файл в целевой
    std::filesystem::rename(temp_save_filepath_, save_filepath_);

    // 3. Сбрасываем таймер
    time_since_last_save_ = milliseconds{0};
  } catch (const std::exception& e) {
    std::cerr << "Failed to save game state: " << e.what() << std::endl;
  }
}

}  // namespace app
