#pragma once
#include <cstdlib>
#include <optional>
#include <string>

namespace numen {

// owning copy; raw getenv is rejected by the MSVC secure CRT (C4996)
inline std::optional<std::string> getEnv(const char *name) {
#ifdef _WIN32
  char *buf = nullptr;
  size_t len = 0;
  if (_dupenv_s(&buf, &len, name) != 0 || !buf) return std::nullopt;
  std::string value{buf};
  std::free(buf);
  return value;
#else
  if (const char *v = std::getenv(name)) return v;
  return std::nullopt;
#endif
}

} // namespace numen
