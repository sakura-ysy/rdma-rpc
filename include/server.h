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
#include <unordered_map>

class Connection;

class Server {
public:
  Server(const char* host, const char* port);
  ~Server(){};

  void run();
  void handleConnecetionEvent(); 
  void handleExitEvent();

  void setupConnection(rdma_cm_event* cm_event, uint32_t n_buffer_page, uint32_t conn_id);

private:
  static void onConnecetionEvent(evutil_socket_t fd, short what, void* arg);
  static void onExitEvent(evutil_socket_t fd, short what, void* arg);

  addrinfo* addr_;
  rdma_event_channel* cm_event_channel_;
  rdma_cm_id* cm_server_id_;

  // event-driven, to avoid the block
  event_base* base_;
  event* conn_event_;
  event* exit_event_;

  // client connection related
  uint32_t cur_conn_id_;
  std::unordered_map<uint32_t, Connection*> conn_map_;
};