#pragma once
#include <connection.h>

class Context {
public:
  Context(void* addr, uint32_t len);
  ~Context();

  void* addr();
  uint32_t length();

private:
  void* addr_;
  uint32_t length_;
};