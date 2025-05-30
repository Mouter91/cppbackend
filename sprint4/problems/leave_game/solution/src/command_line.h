#pragma once

#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

using namespace std::literals;

struct Args {
  unsigned int tick_period = 0;
  std::string config_file;
  std::string www_root;
  bool randomize_spawn_points = false;
  std::string state_file;
  int save_state_period = 0;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]);
