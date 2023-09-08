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
#include <string>
#include <message.h>

class ClientPoller {
public:
  ClientPoller();
  ~ClientPoller();

  void registerConn(Connection* conn);
  void deregisterConn();
  void sendRequest(Message req);

private:
  Spinlock lock_{};
  Connection* conn_;
};


// one client can only connect one server
class Client {
public:
  Client();
  ~Client();

  void connect(const char* host, const char* port);
  rdma_cm_event* waitEvent(rdma_cm_event_type expected);
  void setupConnection(rdma_cm_id* cm_id, uint32_t n_buffer_page);

  void sendRequest(std::string msg);

private:
  rdma_cm_id* cm_id_;  // only one qp, so only one cm_id
  addrinfo* dst_addr_{nullptr};
  rdma_event_channel* cm_event_channel_{nullptr};

  // event-driven, to avoid the block
  event_base* base_{nullptr};
  event* conn_event_{nullptr};
  event* exit_event_{nullptr};

  // connection related
  ClientPoller poller_{};
};
