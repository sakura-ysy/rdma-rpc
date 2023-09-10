#include <message.h>
#include <string.h>

Message::Message(char* buf, uint32_t len, MessageType type) {
  memcpy(meta_.buf_, buf, len); 
  header_.data_len_ = len;
  header_.type_ = type;
};

Message::~Message() {

};

char* Message::dataAddr() {
  return meta_.buf_;
}

uint32_t Message::dataLen() {
  return header_.data_len_;
}

MessageType Message::msgType() {
  return header_.type_;
}


