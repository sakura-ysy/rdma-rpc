#pragma once
#include <stdint.h>
#include <string>

class [[gnu::packed]] BufferMeta {
public:
  explicit BufferMeta(void *buf = nullptr);
  ~BufferMeta() = default;

public:
  void *buf_{nullptr};
};

enum MessageType {
  Dummy,
  Request,
  ImmRequest,
  Response,
};

class [[gnu::packed]] Header {
public:
  void *addr_{nullptr};
  uint32_t data_len_{0};
  MessageType type_{Dummy};
};


class [[gnu::packed]] Message {
public:
  explicit Message(void* buf, uint32_t len, MessageType type);
  ~Message();

  void* dataAddr();
  uint32_t dataLen();
  
private:
  Header header_{};
  BufferMeta meta_{};
};

