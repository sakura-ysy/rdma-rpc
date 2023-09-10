#include <message.h>

class Handler {
public:
  Handler();
  ~Handler();

  Message handlerRequest(Message* req);

private:
};