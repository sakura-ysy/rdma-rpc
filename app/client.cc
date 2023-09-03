#include <macro.h>
#include <client.h>
#include <util.h>

int main([[gnu::unused]] int argc, char *argv[]) {
  Client c;
  c.connect(argv[1], argv[2]);
}