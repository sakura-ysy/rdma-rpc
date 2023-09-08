#pragma once
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <string>
#include <signal.h>
#include <event2/event.h>
#include <connection.h>
#include <misc.h>
#include <list>
#include <thread>

class Connection;


class ServerPoller {
public:
  ServerPoller();
  ~ServerPoller();

  void registerConn(Connection* conn);
  void deregisterConn(Connection* conn);

  void run();
  void stop();
  void poll();

private:
  std::atomic_bool running_{false};
  Spinlock lock_{};
  std::list<Connection*> conn_list_;
  std::thread poll_thread_;
};


// one server can connect multiple clients;
class Server {
public:
  Server(const char* host, const char* port);
  ~Server();

  void run();
  void handleConnectionEvent(); 
  void handleExitEvent();

  void setupConnection(rdma_cm_event* cm_event, uint32_t n_buffer_page);

private:
  static void onConnectionEvent(evutil_socket_t fd, short what, void* arg);

  addrinfo* addr_{nullptr};
  rdma_event_channel* cm_event_channel_{nullptr};
  rdma_cm_id* listen_cm_id_{nullptr};

  // event-driven, to avoid the block
  event_base* base_{nullptr};
  event* conn_event_{nullptr};
  event* exit_event_{nullptr};

  // poller
  ServerPoller poller_{};
};
