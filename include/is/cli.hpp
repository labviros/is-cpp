#pragma once

#include <boost/program_options.hpp>
#include <iostream>
#include "log.hpp"

namespace is {

namespace po = boost::program_options;

po::variables_map parse_program_options(int argc, char** argv,
                                        po::options_description const& user_options) {
  auto add_common_options = [](po::options_description const& others) {
    po::options_description description("Common");
    auto&& options = description.add_options();
    description.add(others);

    options("help", "show available options");
    options("loglevel", po::value<char>()->default_value('i'),
            "char indicating the desired log level: i[nfo], w[warn], e[error]");
    return description;
  };

  auto print_help = [](po::options_description const& description,
                       std::string const& message = "") {
    std::cout << message << '\n' << description << std::endl;
    std::exit(0);
  };

  auto environment_map = [](std::string env) -> std::string {
    std::string prefix("IS_");

    auto starts_with = [](std::string const& s, std::string const& prefix) {
      return s.compare(0, prefix.size(), prefix) == 0;
    };

    if (!starts_with(env, prefix)) return "";

    auto cropped_to_lower_snake_case = [&](std::string& s) {
      auto first = s.begin();
      std::advance(first, prefix.size());
      std::transform(first, s.end(), first,
                     [](char c) { return c == '_' ? '-' : std::tolower(c); });
      return s.substr(prefix.size());
    };

    return cropped_to_lower_snake_case(env);
  };

  auto description = add_common_options(user_options);
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, description), vm);
    po::store(po::parse_environment(description, environment_map), vm);
    po::notify(vm);
  } catch (std::exception const& e) {
    auto error = fmt::format("Error parsing program options: {}", e.what());
    print_help(description, error);
  }

  if (vm.count("help")) print_help(description);

  if (set_loglevel(vm["loglevel"].as<char>()))
    print_help(description, fmt::format("Invalid log level '{}'", vm["loglevel"].as<char>()));

  return vm;
}

}  // namespace is