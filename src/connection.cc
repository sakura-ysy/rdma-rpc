#include <connection.h>
#include <util.h>
#include <macro.h>
#include <iostream>

Connection::Connection(rdma_cm_id* cm_id, uint32_t n_buffer_page, uint32_t conn_id)
    : cm_id_(cm_id),
      n_buffer_page_(n_buffer_page),
      id_(conn_id) {

  info("start new connection");

  // create pd
  int ret = 0;
  local_pd_ = ibv_alloc_pd(cm_id_->verbs);
  checkNotEqual(local_pd_, static_cast<ibv_pd*>(nullptr), "ibv_alloc_pd() failed, server_pd_ == nullptr"); 

  // create cq
  local_cq_ = ibv_create_cq(cm_id_->verbs, DEFAULT_CQ_CAPACITY, this, nullptr, 0);
  checkNotEqual(local_cq_, static_cast<ibv_cq*>(nullptr), "ibv_create_cq() failed, server_cq_ == nullptr");
  cm_id_->recv_cq = local_cq_;
  cm_id_->send_cq = local_cq_;
  info("create protection domain(pd) and completion queue(cq)");

  ibv_qp_init_attr init_attr = defaultQpInitAttr();
  init_attr.send_cq = local_cq_;
  init_attr.recv_cq = local_cq_;

  // create qp
  // rdma_create_qp() will create a qp and store it's address to cm_id->qp.
  // we should record the qp in Connection.
  ret = rdma_create_qp(cm_id_, local_pd_, &init_attr);
  local_qp_ = cm_id_->qp;
  checkEqual(ret, 0, "rdma_create_qp() failed");
  info("create queue pair(qp)");

  // create mr
  size_t size = n_buffer_page * BUFFER_PAGE_SIZE;
  buffer_ = malloc(size);
  checkNotEqual(buffer_, static_cast<void*>(nullptr), "malloc() failed to alloc buffer");

  int access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  buffer_mr_ = ibv_reg_mr(local_pd_, buffer_, size, access);
  checkNotEqual(buffer_mr_, static_cast<ibv_mr*>(nullptr), "ibv_reg_mr() falied, buffer_mr_ == nullptr");
  info("create memory region(mr), size is %zu", size);
  
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

  ret = ibv_destroy_qp(local_qp_);
  wCheckEqual(ret, 0, "fail to destroy qp");

  // ret = ibv_destroy_cq(local_cq_);
  // wCheckEqual(ret, 0, "fail to destroy cq");

  // ret = rdma_destroy_id(cm_id_);
  // wCheckEqual(ret, 0, "fail to destroy cm_id");
  
  ret = ibv_dereg_mr(buffer_mr_);
  wCheckEqual(ret, 0, "fail to deregister buffer memory region");

  ret = ibv_dealloc_pd(local_pd_);
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
