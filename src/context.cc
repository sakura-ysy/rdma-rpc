#include <context.h>

Context::Context(void* addr, uint32_t len)
    : addr_(addr),
      length_(len) {
}

Context::~Context() {

}

void* Context::addr(){
  return addr_;
}
uint32_t Context::length() {
  return length_;
}
