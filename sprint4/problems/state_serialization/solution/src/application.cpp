#include "application.h"

namespace app {

Application::Application(model::Game&& game, extra_data::MapsExtra&& extra)
    : game_(std::move(game)), maps_extra_(std::move(extra)) {
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
    game_.GetSettings().ticker =
        std::make_shared<game_time::Ticker>(strand, std::chrono::milliseconds(config->tick_period),
                                            [this](std::chrono::milliseconds delta) {
                                              this->Tick(delta);  // вызываем тик
                                            });
    game_.GetSettings().ticker->Start();
  }
}

void Application::Tick(double seconds) {
  using milliseconds = std::chrono::milliseconds;
  game_.Tick(seconds);
  tick_signal_(milliseconds(static_cast<int64_t>(seconds * 1000)));
}

// Сигнальный тик. Вызывается из тикера
void Application::Tick(milliseconds delta) {
  game_.Tick(static_cast<double>(delta.count()) / 1000.0);
  tick_signal_(delta);  // Уведомляем слушателей
}

void Application::EnableStateSerialization(const std::filesystem::path& file_path,
                                           milliseconds period) {
  serializer_ = std::make_unique<SerializingListener>(*this, file_path, period);
}

}  // namespace app
