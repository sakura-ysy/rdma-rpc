#include <message.h>

BufferMeta::BufferMeta(void* buf) {
  buf_ = buf;
}

Message::Message(void* buf, uint32_t len, MessageType type) {
  meta_.buf_ = buf;
  header_.addr_ = buf;
  header_.data_len_ = len;
  header_.type_ = type;
};

Message::~Message() {

};

void* Message::dataAddr() {
  return header_.addr_;
}

uint32_t Message::dataLen() {
  return header_.data_len_;
}
