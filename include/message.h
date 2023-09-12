#pragma once
#include <stdint.h>
#include <string>
#include <const.h>

class [[gnu::packed]] BufferMeta {
public:
   char buf_[MESSAGE_BUF_SIZE]{};
};

enum MessageType {
  Dummy,
  Request,
  ImmRequest,
  Response,
};

class [[gnu::packed]] Header {
public:
  uint32_t data_len_{};
  MessageType type_{Dummy};
};


class [[gnu::packed]] Message {
public:
  explicit Message(char* buf, uint32_t len, MessageType type);
  ~Message();

  char* dataAddr();
  uint32_t dataLen();
  MessageType msgType();

private:
  Header header_{};
  BufferMeta meta_;
};

