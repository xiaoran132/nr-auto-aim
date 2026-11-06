#pragma once

#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>

namespace tools
{
inline YAML::Node load(const std::string & path)
{
  try {
    return YAML::LoadFile(path);
  } catch (const std::exception & e) {
    throw std::runtime_error("[yaml] failed to load " + path + ": " + e.what());
  }
}

template <typename T>
inline T read(const YAML::Node & yaml, const std::string & key)
{
  if (!yaml[key]) throw std::runtime_error("[yaml] key not found: " + key);
  return yaml[key].as<T>();
}
}  // namespace tools
