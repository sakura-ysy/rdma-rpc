#pragma once
#include <stdexcept>

inline auto die(const char* err_msg) {
  throw std::runtime_error(err_msg);
}

inline void info(const char* msg) {
  fprintf(stderr, "%s\n", msg);
}

template <typename... Args>
static inline auto info(const char *fmt, Args... args) -> void {
  fprintf(stderr, fmt, args...);
  fprintf(stderr, "\n");
}

// Fail if ret != type
template <typename Type>
inline void checkEqual(Type ret, Type cmp, const char* err_msg) {
  if (ret != cmp)
    die(err_msg);
}

// Fail if ret == type
template <typename Type>
inline void checkNotEqual(Type ret, Type cmp, const char* err_msg) {
  if (ret == cmp)
    die(err_msg);
}

// Warn if ret != type
template <typename Type>
inline void wCheckEqual(Type ret, Type cmp, const char* err_msg) {
  if (ret != cmp)
    info(err_msg);
}

// Warn if ret == type
template <typename Type>
inline void wCheckNotEqual(Type ret, Type cmp, const char* err_msg) {
  if (ret == cmp)
    info(err_msg);
}
