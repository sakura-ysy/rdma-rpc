#include <client.h>
#include <util.h>
#include <macro.h>
#include <iostream>

Client::Client() {
  cm_event_channel_ = rdma_create_event_channel();
  checkNotEqual(cm_event_channel_, static_cast<rdma_event_channel*>(nullptr), "rdma_create_event_channel() failed");
  int ret = rdma_create_id(cm_event_channel_, &cm_client_id_, nullptr, RDMA_PS_TCP);
  checkEqual(ret, 0, "rdma_create_id() failed");
}

Client::~Client() {
  // the cm_id arg can be local_cm_id or remote_cm_id,
  // if the arg is local_cm_id, this operation will disconect all the connection.
  // Disconnect all.
  int ret = rdma_disconnect(cm_client_id_);
  wCheckEqual(ret, 0, "rdma_disconnect() failed to disconnect");
  rdma_cm_event* cm_event = waitEvent(RDMA_CM_EVENT_DISCONNECTED);
  wCheckNotEqual(cm_event, static_cast<rdma_cm_event*>(nullptr), "failed to get disconnection event");
  ret = rdma_ack_cm_event(cm_event);
  wCheckEqual(ret, 0, "rdma_ack_cm_event() failed to send ack");
  freeaddrinfo(dst_addr_);
  info("client disconnect");
}

void Client::connect(const char* host, const char* port) {
  int ret = 0;
  ret = getaddrinfo(host, port, nullptr, &dst_addr_);
  checkEqual(ret, 0, "getaddrinfo() failed");

  // When the resolution is completed, the cm_event_channel_ will generate an cm_event. 
  ret = rdma_resolve_addr(cm_client_id_, nullptr, dst_addr_->ai_addr, DEFAULT_CONNECTION_TIMEOUT);  // non-block
  checkEqual(ret, 0, "rdma_resolve_addr() failed");
  rdma_cm_event* cm_event = waitEvent(RDMA_CM_EVENT_ADDR_RESOLVED);  // block

  if(cm_event == nullptr) {
    std::cout << "cm_event == nullptr" << std::endl;
  }

  checkNotEqual(cm_event, static_cast<rdma_cm_event*>(nullptr), "failed to resolve the address");
  ret = rdma_ack_cm_event(cm_event);
  checkEqual(ret, 0, "rdma_ack_cm_event() failed to send ack");
  info("address resolution is completed");

  // same as above, resolve the route
  ret = rdma_resolve_route(cm_client_id_, DEFAULT_CONNECTION_TIMEOUT);
  checkEqual(ret, 0, "rdma_resolve_route failed");
  cm_event = waitEvent(RDMA_CM_EVENT_ROUTE_RESOLVED);
  checkNotEqual(cm_event, static_cast<rdma_cm_event*>(nullptr), "failed to resolve the route");
  ret = rdma_ack_cm_event(cm_event);
  checkEqual(ret, 0, "rdma_ack_cm_event() failed to send ack");
  info("route resolution is completed");
  
  setupConnection(cm_client_id_, 64);

}

rdma_cm_event* Client::waitEvent(rdma_cm_event_type expected) {
  rdma_cm_event* cm_event = nullptr;
  // block and wait the resolution
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

void Client::setupConnection(rdma_cm_id* cm_id, uint32_t n_buffer_page) {

  Connection* conn = new Connection(cm_id, n_buffer_page, -1);
  rdma_conn_param param = conn->copyConnParam();
  int ret = rdma_connect(cm_id, &param);
  checkEqual(ret, 0, "rdma_connect() failed");
  rdma_cm_event* ev = waitEvent(RDMA_CM_EVENT_ESTABLISHED);
  checkNotEqual(ev, static_cast<rdma_cm_event*>(nullptr), "fail to establish the connection");
  info("connection is established");
}