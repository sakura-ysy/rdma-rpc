#include <const.h>
#include <server.h>
#include <util.h>

int main([[gnu::unused]] int argc, char *argv[]) {
  Server s(argv[1], argv[2]);
  s.run();
}