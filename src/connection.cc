#include <connection.h>
#include <util.h>
#include <macro.h>

Connection::Connection(Server* server, rdma_cm_id* client_id, uint32_t n_buffer_page, uint32_t conn_id)
    : server_(server),
      client_id_(client_id),
      n_buffer_page_(n_buffer_page),
      id_(conn_id) {

  // create pd
  int ret = 0;
  server_pd_ = ibv_alloc_pd(client_id_->verbs);
  checkNotEqual(server_pd_, static_cast<ibv_pd*>(nullptr), "ibv_alloc_pd() failed, server_pd_ == nullptr"); 

  // create cq
  server_cq_ = ibv_create_cq(client_id_->verbs, DEFAULT_CQ_CAPACITY, this, nullptr, 0);
  checkNotEqual(server_cq_, static_cast<ibv_cq*>(nullptr), "ibv_create_cq() failed, server_cq_ == nullptr");
  client_id_->recv_cq = server_cq_;
  client_id_->send_cq = server_cq_;
  info("create protection domain(pd) and completion queue(cq)");

  ibv_qp_init_attr init_attr = defaultQpInitAttr();
  init_attr.send_cq = server_cq_;
  init_attr.recv_cq = server_cq_;

  // create qp
  ret = rdma_create_qp(client_id_, server_pd_, &init_attr);
  checkEqual(ret, 0, "rdma_create_qp() failed");
  clietn_qp_ = client_id_->qp;
  info("create queue pair(qp)");

  // create mr
  size_t size = n_buffer_page * BUFFER_PAGE_SIZE;
  buffer_ = malloc(n_buffer_page * BUFFER_PAGE_SIZE);
  checkNotEqual(buffer_, static_cast<void*>(nullptr), "failedto malloc buffer_");
  int access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  buffer_mr_ = ibv_reg_mr(server_pd_, buffer_, size, access);
  checkNotEqual(buffer_mr_, static_cast<ibv_mr*>(nullptr), "ibv_reg_mr() falied, buffer_mr_ == nullptr");
  info("create memory region(mr)");
  
  // set param
  memset(&param_, 0, sizeof(rdma_conn_param));
  param_.private_data = reinterpret_cast<void*>(&(buffer_mr_->rkey));
  param_.private_data_len = sizeof(buffer_mr_->rkey);
  param_.responder_resources = 16;
  param_.initiator_depth = 16;
  param_.rnr_retry_count = 7;
  info("initialize connection parameters");
}

Connection::~Connection() {
  int ret = 0;

  ret = ibv_destroy_qp(clietn_qp_);
  wCheckEqual(ret, 0, "fail to destroy qp");

  ret = ibv_destroy_cq(server_cq_);
  wCheckEqual(ret, 0, "fail to destroy cq");

  ret = rdma_destroy_id(client_id_);
  wCheckEqual(ret, 0, "fail to destroy id");

  ret = ibv_dereg_mr(buffer_mr_);
  wCheckEqual(ret, 0, "fail to deregister buffer memory region");

  ret = ibv_dealloc_pd(server_pd_);
  wCheckEqual(ret, 0, "fail to deallocate pd");

  free(buffer_);

  info("cleanup connection resources");
}

ibv_qp_init_attr Connection::defaultQpInitAttr() {
  ibv_qp_init_attr init_attr;
  init_attr.qp_context = nullptr;
  init_attr.send_cq = nullptr;
  init_attr.recv_cq = nullptr;
  init_attr.srq = nullptr;
  init_attr.cap = ibv_qp_cap {
    MAX_SEND_WR_NUM,  // max_send_wr
    MAX_RECV_WR_NUM,  // max_recv_wr
    1,                // max_send_sge
    1,                // max_recv_sge
    0,                // max_inline_data
  };
  init_attr.qp_type = IBV_QPT_RC;
  init_attr.sq_sig_all = 0;
  return init_attr;
}

rdma_conn_param Connection::copyConnParam() {
  return param_;
}

void Connection::setRkey(uint32_t rkey) {
  rkey_ = rkey;
}

uint32_t Connection::getId() {
  return id_;
}
