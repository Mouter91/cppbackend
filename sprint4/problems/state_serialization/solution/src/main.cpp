#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "log.h"
#include "request_logger.h"
#include "command_line.h"
#include "application.h"  // Add this include

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
  n = std::max(1u, n);
  std::vector<std::jthread> workers;
  workers.reserve(n - 1);
  while (--n) {
    workers.emplace_back(fn);
  }
  fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
  try {
    StartLogging();
    Args arg;

    auto config = ParseCommandLine(argc, argv);
    if (!config)
      return EXIT_FAILURE;

    // 2. Инициализируем io_context
    const unsigned num_threads = std::thread::hardware_concurrency();
    net::io_context ioc(num_threads);
    net::strand strand = net::make_strand(ioc);

    // 1. Загружаем карту из файла и построить модель игры
    auto [game, maps_extra] = json_loader::LoadGamePackage(config->config_file);
    app::Application app(std::move(game), std::move(maps_extra));
    app.SetGameSettings(config);
    app.SetGameTicker(config, strand);

    // === ВОССТАНОВЛЕНИЕ СОСТОЯНИЯ ===
    if (!config->state_file.empty() && std::filesystem::exists(config->state_file)) {
      try {
        std::ifstream in(config->state_file);
        model::LoadGame(app.GetGame(), app.GetPlayers(), in);
      } catch (const std::exception& ex) {
        std::cerr << "Failed to load state from file " << config->state_file << ": " << ex.what()
                  << std::endl;
        ServerStopLog(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
      }
    }

    // === ПЕРИОДИЧЕСКОЕ СОХРАНЕНИЕ ===
    if (!config->state_file.empty() && config->save_state_period > 0) {
      app.EnableStateSerialization(config->state_file,
                                   std::chrono::milliseconds(config->save_state_period));
    }

    // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
      if (!ec) {
        ioc.stop();
      }
    });
    // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
    http_handler::RequestHandler handler{app, strand, config->www_root};
    LoggingRequestHandler logging_handler(handler);

    // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;

    http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
      logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
    });

    ServerStartLog(port, address);

    // 6. Запускаем обработку асинхронных операций
    RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

    // === СОХРАНЕНИЕ ПРИ ЗАВЕРШЕНИИ ===
    if (!config->state_file.empty()) {
      try {
        // Создаем временный файл
        auto tmp_path = config->state_file;
        tmp_path += ".tmp";

        {
          std::ofstream out(tmp_path, std::ios::trunc);
          if (!out) {
            throw std::runtime_error("Failed to open temporary file for writing");
          }
          model::SaveGame(app.GetGame(), app.GetPlayers(), out);
          out.close();

          // Проверяем, что файл был записан
          if (!std::filesystem::exists(tmp_path)) {
            throw std::runtime_error("Temporary file was not created");
          }
        }

        // Заменяем основной файл временным
        std::filesystem::rename(tmp_path, config->state_file);

      } catch (const std::exception& ex) {
        std::cerr << "Failed to save game state: " << ex.what() << std::endl;
        ServerStopLog(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
      }
    }
  } catch (const std::exception& ex) {
    ServerStopLog(EXIT_FAILURE, ex.what());
  }
}
