#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "log.h"
#include "request_logger.h"
#include "command_line.h"
#include "ticker.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
  n = std::max(1u, n);
  std::vector<std::jthread> workers;
  workers.reserve(n - 1);
  // Запускаем n-1 рабочих потоков, выполняющих функцию fn
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
    game.GetSettings().random_spawn = config->randomize_spawn_points;
    if (config->tick_period > 0) {
      game.GetSettings().ticker = std::make_shared<game_time::Ticker>(
          strand,  // Уже правильный тип - преобразуется в Ticker::Strand
          std::chrono::milliseconds(config->tick_period),  // Явное преобразование
          [&game](std::chrono::milliseconds delta) {       // Явно указываем тип параметра
            game.Tick(static_cast<double>(delta.count()) / 1000.0);
          });
      game.GetSettings().ticker->Start();
    }
    // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
      if (!ec) {
        ioc.stop();
      }
    });
    // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
    http_handler::RequestHandler handler{game, strand, arg.www_root, maps_extra};
    LoggingRequestHandler logging_handler(handler);

    // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;

    http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
      logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
    });

    // Cообщает тестам о том, что сервер запущен и готов обрабатывать запросы
    ServerStartLog(port, address);

    // 6. Запускаем обработку асинхронных операций
    RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });
  } catch (const std::exception& ex) {
    ServerStopLog(EXIT_FAILURE, ex.what());
  }
}
