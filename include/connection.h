#pragma once
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <string>
#include <signal.h>
#include <event2/event.h>
#include <server.h>

class Server;

class Connection {
public:
  Connection(Server* server, rdma_cm_id* client_id, uint32_t n_buffer_page, uint32_t conn_id);
  ~Connection();

  rdma_conn_param copyConnParam();
  void setRkey(uint32_t rkey);
  uint32_t getId();
  
private:
  static ibv_qp_init_attr defaultQpInitAttr();

  uint32_t id_;
  Server* server_;
  rdma_cm_id* client_id_;
  ibv_pd* server_pd_;
  ibv_cq* server_cq_;
  ibv_qp* clietn_qp_;
  void* buffer_;
  uint32_t n_buffer_page_;
  ibv_mr* buffer_mr_;
  rdma_conn_param param_;
  uint32_t rkey_;
};