#include "command_line.h"

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
  namespace po = boost::program_options;

  po::options_description desc{"All options"s};
  Args args;

  // Allowed options:
  // -h [ --help ]                     produce help message
  // -t [ --tick-period ] milliseconds set tick period
  // -c [ --config-file ] file         set config file path
  // -w [ --www-root ] dir             set static files root
  // --randomize-spawn-points          spawn dogs at random positions

  desc.add_options()("help,h", "produce help message")(
      "tick-period,t", po::value<unsigned int>(&args.tick_period)->value_name("milliseconds"s),
      "set tick period")("config-file,c", po::value(&args.config_file)->value_name("file"s),
                         "set config file path")(
      "www-root,w", po::value(&args.www_root)->value_name("dir"s), "set static files root")(
      "randomize-spawn-points", po::bool_switch(&args.randomize_spawn_points),
      "spawn dogs at random positions")("state-file",
                                        po::value(&args.state_file)->value_name("file"),
                                        "application state file for backup")(
      "save-state-period", po::value(&args.save_state_period)->value_name("milliseconds"),
      "period of make backup");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.contains("help"s)) {
    std::cout << desc;
    return std::nullopt;
  }

  if (vm.contains("config-file") && vm.contains("www-root")) {
    return args;
  } else {
    throw std::runtime_error(
        "Usage: game_server --config-file <game-config-json> --www-root <dir-to-content>");
  }

  return std::nullopt;
}
