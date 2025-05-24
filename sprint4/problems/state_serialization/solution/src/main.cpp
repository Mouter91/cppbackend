#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "log.h"
#include "request_logger.h"
#include "command_line.h"
#include "application.h"

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

    const unsigned num_threads = std::thread::hardware_concurrency();
    net::io_context ioc(num_threads);
    net::strand strand = net::make_strand(ioc);

    auto [game, maps_extra] = json_loader::LoadGamePackage(config->config_file);
    app::Application app(std::move(game), std::move(maps_extra), &config.value());

    app.SetGameSettings(config, strand);

    if (!config->state_file.empty()) {
      try {
        app.LoadState(config->state_file);
      } catch (const std::exception& e) {
        ServerStopLog(EXIT_FAILURE, e.what());
        return EXIT_FAILURE;
      }
    }

    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc, &app, &config](const sys::error_code& ec, int) {
      if (!ec) {
        if (!config->state_file.empty()) {
          try {
            app.SaveState(config->state_file);
          } catch (const std::exception& e) {
            std::cerr << "Failed to save state: " << e.what() << std::endl;
          }
        }
        ioc.stop();
      }
    });

    http_handler::RequestHandler handler{app, strand, arg.www_root};
    LoggingRequestHandler logging_handler(handler);

    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;

    http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
      logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
    });

    ServerStartLog(port, address);

    RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

    // Сохраняем состояние при нормальном завершении работы
    if (!config->state_file.empty()) {
      try {
        app.SaveState(config->state_file);
      } catch (const std::exception& e) {
        std::cerr << "Failed to save state: " << e.what() << std::endl;
      }
    }
  } catch (const std::exception& ex) {
    ServerStopLog(EXIT_FAILURE, ex.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
