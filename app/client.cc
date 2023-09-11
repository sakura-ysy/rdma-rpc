#include <macro.h>
#include <client.h>
#include <util.h>
#include <string>

std::string rand_str(const int len)
{

    std::string str;              
    char c;                         
    for(int i = 0; i < len; i++)
    {
        c = 'a' + rand()%26;
        str.push_back(c);
    }
    return str;
}

int main([[gnu::unused]] int argc, char *argv[]) {
  Client c;
  c.connect(argv[1], argv[2]);
  sleep(1);

  int len = 10;
  int cnt = 5;
  for (int i = 0; i < cnt; i++)
  {
    c.sendRequest(rand_str(len));
  }
  
  sleep(3);
}