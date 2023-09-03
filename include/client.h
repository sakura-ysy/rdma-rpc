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

// one client can only connect one server
class Client {
public:
  Client();
  ~Client();

  void connect(const char* host, const char* port);
  rdma_cm_event* waitEvent(rdma_cm_event_type expected);
  void setupConnection(rdma_cm_id* cm_id, uint32_t n_buffer_page);

private:
  addrinfo* dst_addr_;
  rdma_event_channel* cm_event_channel_;
  rdma_cm_id* cm_client_id_;

  // event-driven, to avoid the block
  event_base* base_;
  event* conn_event_;
  event* exit_event_;

};