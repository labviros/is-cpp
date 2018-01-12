#pragma once

#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <boost/optional.hpp>
#include <fstream>
#include <string>

namespace is {

namespace pb {
using namespace google::protobuf::util;
using namespace google::protobuf;
}  // namespace pb

inline void save_to_json(std::string const& filename, pb::Message const& message) {
  pb::JsonPrintOptions options;
  options.add_whitespace = true;
  options.always_print_primitive_fields = true;
  std::string json;

  pb::MessageToJsonString(message, &json, options);

  std::ofstream file(filename);
  file << json;
  file.close();
}

template <typename T>
inline boost::optional<T> load_from_json(std::string const& filename) {
  T message;
  std::ifstream in(filename);
  std::string buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return is::pb::JsonStringToMessage(buffer, &message).ok() ? boost::optional<T>(message)
                                                            : boost::none;
}

}  // namespace is