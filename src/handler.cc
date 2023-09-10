#include <handler.h>
#include <string.h>
#include <string>
#include <util.h>
#include <algorithm>
#include <assert.h>

Handler::Handler(){

}

Handler::~Handler(){

}

Message Handler::handlerRequest(Message* req) {
  assert(req->msgType() == ImmRequest || req->msgType() == Request);
  char data[req->dataLen()];
  memcpy(data, req->dataAddr(), req->dataLen());
  std::sort(data, data + sizeof(data));
  Message resp(data, sizeof(data), Response);
  return resp;
}