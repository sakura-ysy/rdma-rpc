#include <client.h>
#include <util.h>
#include <macro.h>
#include <iostream>
#include <mutex>
#include <message.h>

Client::Client() {
  cm_event_channel_ = rdma_create_event_channel();
  checkNotEqual(cm_event_channel_, static_cast<rdma_event_channel*>(nullptr), "rdma_create_event_channel() failed");
}

Client::~Client() {
  int ret = rdma_disconnect(cm_id_);
  wCheckEqual(ret, 0, "rdma_disconnect() failed to disconnect");
  rdma_cm_event* cm_event = waitEvent(RDMA_CM_EVENT_DISCONNECTED);
  wCheckNotEqual(cm_event, static_cast<rdma_cm_event*>(nullptr), "failed to get disconnection event");
  ret = rdma_ack_cm_event(cm_event);
  wCheckEqual(ret, 0, "rdma_ack_cm_event() failed to send ack");
  freeaddrinfo(dst_addr_);
  rdma_destroy_event_channel(cm_event_channel_);
  poller_.deregisterConn();
  info("client disconnect");
}

void Client::connect(const char* host, const char* port) {

  int ret = 0;
  ret = rdma_create_id(cm_event_channel_, &cm_id_, nullptr, RDMA_PS_TCP);
  checkEqual(ret, 0, "rdma_create_id() failed");

  ret = getaddrinfo(host, port, nullptr, &dst_addr_);
  checkEqual(ret, 0, "getaddrinfo() failed");

  // When the resolution is completed, the cm_event_channel_ will generate an cm_event. 
  ret = rdma_resolve_addr(cm_id_, nullptr, dst_addr_->ai_addr, DEFAULT_CONNECTION_TIMEOUT);  // non-block
  checkEqual(ret, 0, "rdma_resolve_addr() failed");
  rdma_cm_event* cm_event = waitEvent(RDMA_CM_EVENT_ADDR_RESOLVED);  // block
  checkNotEqual(cm_event, static_cast<rdma_cm_event*>(nullptr), "failed to resolve the address");
  ret = rdma_ack_cm_event(cm_event);
  checkEqual(ret, 0, "rdma_ack_cm_event() failed to send ack");
  info("address resolution is completed");

  // same as above, resolve the route
  ret = rdma_resolve_route(cm_id_, DEFAULT_CONNECTION_TIMEOUT);
  checkEqual(ret, 0, "rdma_resolve_route failed");
  cm_event = waitEvent(RDMA_CM_EVENT_ROUTE_RESOLVED);
  checkNotEqual(cm_event, static_cast<rdma_cm_event*>(nullptr), "failed to resolve the route");
  ret = rdma_ack_cm_event(cm_event);
  checkEqual(ret, 0, "rdma_ack_cm_event() failed to send ack");
  info("route resolution is completed");
  
  setupConnection(cm_id_, 64);

  // run poller
  poller_.run();
}

rdma_cm_event* Client::waitEvent(rdma_cm_event_type expected) {
  rdma_cm_event* cm_event = nullptr;
  int ret = rdma_get_cm_event(cm_event_channel_, &cm_event);
  if (ret != 0) {
    info("fail to get cm event");
    return nullptr;
  }
  if (cm_event->status != 0) {
    info("get a bad cm event");
    return nullptr;
  }
  else if (cm_event->event != expected) {
    info("got: %s, expected: %s", rdma_event_str(cm_event->event),
         rdma_event_str(expected));
    return nullptr;
  }
  else
    return cm_event;
}

void Client::setupConnection(rdma_cm_id* client_id, uint32_t n_buffer_page) {
  Connection* conn = new Connection(Role::ClientConn, client_id, n_buffer_page);
  rdma_conn_param param = conn->copyConnParam();
  int ret = rdma_connect(client_id, &param);
  checkEqual(ret, 0, "rdma_connect() failed");
  rdma_cm_event* ev = waitEvent(RDMA_CM_EVENT_ESTABLISHED);
  checkNotEqual(ev, static_cast<rdma_cm_event*>(nullptr), "fail to establish the connection");
  info("connection is established");

  poller_.registerConn(conn);
}

void Client::sendRequest(std::string msg) {
  Message req((char*)msg.c_str(), msg.length(), MessageType::ImmRequest);
  poller_.sendRequest(req);  
  info("post send reqeust, req data is: %s", msg.c_str());
}


// /* ClientPoller */
ClientPoller::ClientPoller() {

}

ClientPoller::~ClientPoller() {

}

void ClientPoller::registerConn(Connection* conn) {
  std::lock_guard<Spinlock> lock(lock_);
  conn_ = conn;
}

void ClientPoller::deregisterConn() {
  std::lock_guard<Spinlock> lock(lock_);
  delete conn_;
}

void ClientPoller::sendRequest(Message req) {
  conn_->lock(); // unlock when receive response;
  conn_->fillMR((void*)&req, sizeof(req));
  conn_->postSend(conn_->getMRAddr(), sizeof(req),  conn_->getLKey(), false);
}


void ClientPoller::run() {
  running_.store(true, std::memory_order_release);
  poll_thread_ = std::thread(&ClientPoller::poll, this);
  info("start running client poller");
}

void ClientPoller::stop() {
  if (running_.load(std::memory_order_acquire)) {
    running_.store(false, std::memory_order_release);
    poll_thread_.join();
    info("connection poller stopped");
  }
}

void ClientPoller::poll() {
  while (running_.load(std::memory_order_acquire)) {
    std::lock_guard<Spinlock> lock(lock_);
    conn_->poll();
  }
}
