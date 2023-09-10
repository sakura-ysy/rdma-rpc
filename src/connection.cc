#include <connection.h>
#include <util.h>
#include <macro.h>
#include <iostream>
#include <mutex>
#include <message.h>
#include <assert.h>
#include <context.h>

/* Connection */
Connection::Connection(Role role, rdma_cm_id* cm_id, uint32_t n_buffer_page)
    : role_(role),
      cm_id_(cm_id),
      n_buffer_page_(n_buffer_page) {

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
  
  // prepare recv
  prepare();
  info("connection prepares");
}

Connection::~Connection() {

  int ret = 0;

  // clear the memory
  memset(buffer_mr_->addr, 0 ,buffer_mr_->length);

  ret = rdma_destroy_id(cm_id_);
  wCheckEqual(ret, 0, "fail to destroy cm_id");

  ret = ibv_destroy_qp(local_qp_);
  wCheckEqual(ret, 0, "fail to destroy qp");

  // warnning! you must ack all events before destroy cq,
  // otherwise, the func would never end.
  ret = ibv_destroy_cq(local_cq_);
  wCheckEqual(ret, 0, "fail to destroy cq");
  
  ret = ibv_dereg_mr(buffer_mr_);
  wCheckEqual(ret, 0, "fail to deregister buffer memory region");

  ret = ibv_dealloc_pd(local_pd_);
  wCheckEqual(ret, 0, "fail to deallocate pd");

  free(buffer_);

  info("clean up connection resources");
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

rdma_cm_id* Connection::getCmId() {
  return cm_id_;
}

uint32_t Connection::getLKey() {
  return buffer_mr_->lkey;
}

uint32_t Connection::getRKey() {
  return buffer_mr_->rkey;
}

void* Connection::getMRAddr() {
  return buffer_mr_->addr;
}


void Connection::fillMR(void* data, uint32_t size) {
  assert(size <= buffer_mr_->length);
  for (int i = 0; i < size; i++)
  {
    *((char*)(buffer_mr_->addr) + i) = *((char*)(data) + i);  
  }
}

void Connection::prepare() {
  postRecv(buffer_mr_->addr, buffer_mr_->length, buffer_mr_->lkey);
  switch (role_) {
  case Role::ServerConn : {
    state_ = State::WaitingForRequest;
    break;
  }
  case Role::ClientConn : {
    state_ = State::WaitingForResponse;
    break;
  }
  default : {
    checkNotEqual(0, 0, "Connection has wrong role");
  }
  }
}

void Connection::poll() {
  static ibv_wc wc[DEFAULT_CQ_CAPACITY];
  int ret = ibv_poll_cq(local_cq_, DEFAULT_CQ_CAPACITY, wc);
  if (ret < 0) {
    info("poll cq error");
    return;
  } else if (ret == 0) {
    //info("get no wc");
    return;
  }

  for (int i = 0; i < ret; i++) {
    switch (role_){
    case Role::ServerConn: {
      serverAdvance(wc[i]);
      break;
    }
    case Role::ClientConn: {
      clientAdvance(wc[i]);
      break;
    }
    }
  }
}

void Connection::serverAdvance(const ibv_wc &wc) {
  switch (wc.opcode) {
  case IBV_WC_RECV: {
    assert(state_ == WaitingForRequest);
    Context* ctx = reinterpret_cast<Context*>(wc.wr_id);
    Message* req = reinterpret_cast<Message*>(ctx->addr());
    state_ = HandlingRequest;
    info("recive from client, start handling the request");
    Message resp = handler_.handlerRequest(req);
    info("handle over");
    // send resp
    fillMR((void*)&resp, sizeof(resp));
    postSend(getMRAddr(), sizeof(resp), getLKey(), false);
    delete ctx;
    break;
  }
  case IBV_WC_SEND: {
    assert(state_ == HandlingRequest);
    prepare();
    info("response send completed, waiting for next request");
    state_ == WaitingForRequest;
    break;
  }
  case IBV_WC_RDMA_READ: {
    // todo
    break;
  }
  case IBV_WC_RDMA_WRITE: {
    // todo
    break;
  }
  default: {
    info("unexpected wc opcode: %d", wc.opcode);
    break;
  }
  }
}


void Connection::clientAdvance(const ibv_wc &wc) {
  switch (wc.opcode) {
  case IBV_WC_SEND: {
    prepare();
    Context* ctx = reinterpret_cast<Context*>(wc.wr_id);
    delete ctx;
    state_ = WaitingForResponse;
    info("request send completed, waiting for response");
    break;
  }
  case IBV_WC_RECV: {
    assert(state_ == WaitingForResponse);
    Context* ctx = reinterpret_cast<Context*>(wc.wr_id);
    Message* resp = reinterpret_cast<Message*>(ctx->addr());
    if(resp->msgType() == Response) {
      info("receive response from server, resp data is: %s", resp->dataAddr());
      unlock();
      state_ = Vacant;
    }
    break;
  }
  case IBV_WC_RDMA_READ: {
    break;
  }
  case IBV_WC_RDMA_WRITE: {
    break;
  }
  default: {
    info("unexpected wc opcode: %d", wc.opcode);
    break;
  }
  }
}

void Connection::postSend(void* local_addr, uint32_t length, uint32_t lkey, bool need_inline){
  ibv_sge sge {
    (uint64_t) local_addr, // addr
    length,                // length
    lkey,                  // lkey
  };
  ibv_send_wr wr {
    (uint64_t)(new Context(local_addr, length)),           // wr_id
    nullptr,               // next
    &sge,                  // sg_list
    1,                     // num_sge
    IBV_WR_SEND,           // opcode
    IBV_SEND_SIGNALED,     // send_flags
    {},
    {},
    {},
    {},
  };

  if (need_inline) {
    wr.send_flags |= IBV_SEND_INLINE;
  }

  ibv_send_wr* bad_wr = nullptr;

  int ret = ibv_post_send(local_qp_, &wr, &bad_wr);
  checkEqual(ret, 0, "ibv_post_send() failed");
}


void Connection::postRecv(void* local_addr, uint32_t length, uint32_t lkey) {
  ibv_sge sge {
    (uint64_t) local_addr, // addr
    length,                // length
    lkey,                  // lkey
  };
  ibv_recv_wr wr {
    (uint64_t)(new Context(local_addr, length)),           // wr_id
    nullptr,               // next
    &sge,                  // sg_list
    1,                     // num_sge
  };

  //std::cout << "postRecv local_addr: " << local_addr << std::endl;

  ibv_recv_wr* bad_wr = nullptr;
  int ret = ibv_post_recv(local_qp_, &wr, &bad_wr);
  checkEqual(ret, 0, "ibv_post_recv() failed");
}

void Connection::lock() {
  lock_.lock();
}

void Connection::unlock() {
  lock_.unlock();
}