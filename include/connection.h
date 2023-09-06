#pragma once
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <string>
#include <signal.h>
#include <event2/event.h>
#include <server.h>
#include <misc.h>

class Server;

class Connection {
public:

  // for client, the cm_id is local,
  // for server, the cm_id is remote
  Connection(rdma_cm_id* cm_id, uint32_t n_buffer_page);
  ~Connection();

  rdma_conn_param copyConnParam();
  void setRkey(uint32_t rkey);
  rdma_cm_id* getCmId();
  
private:
  static ibv_qp_init_attr defaultQpInitAttr();

  // for client, it presents the local cm_id,
  // for server, it presents the remote(client) cm_id
  rdma_cm_id* cm_id_;
  ibv_pd* local_pd_;
  ibv_cq* local_cq_;
  ibv_qp* local_qp_;
  void* buffer_;
  uint32_t n_buffer_page_;
  ibv_mr* buffer_mr_;
  rdma_conn_param param_;
  uint32_t rkey_;
  uint32_t lkey_;
};
