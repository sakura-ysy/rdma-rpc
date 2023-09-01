#include <server.h>
#include <util.h>
#include <macro.h>

Server::Server(const char* host, const char* port) : cur_conn_id_(0) {
  int ret = 0;
  ret = getaddrinfo(host, port, nullptr, &addr_);
  checkEqual(ret, 0, "getaddrinfo() failed");

  // Open a channel used to report asynchronous communication event
  cm_event_channel_ = rdma_create_event_channel();
  checkNotEqual(cm_event_channel_, static_cast<rdma_event_channel*>(nullptr), "rdma_create_event_channel() failed, cm_event_channel_ == nullptr");
  
  // rdma_cm_id is the connection identifier (like socket) which is used to define an RDMA connection. 
	ret = rdma_create_id(cm_event_channel_, &cm_server_id_, nullptr, RDMA_PS_TCP);
  checkEqual(ret, 0, "rdma_create_id() failed");

  // Explicit binding of rdma cm id to the socket credentials
  ret = rdma_bind_addr(cm_server_id_, addr_->ai_addr);
  checkEqual(ret, 0 ,"rdma_bind_addr() failed");

  // Now we start to listen on the passed IP and port. However unlike
	// normal TCP listen, this is a non-blocking call. When a new client is 
	// connected, a new connection management (CM) event is generated on the 
	// RDMA CM event channel from where the listening id was created. Here we
	// have only one channel, so it is easy.
  ret = rdma_listen(cm_server_id_, DEFAULT_BACK_LOG); // backlog is the max clients num, same as tcp, see 'man listen'
  checkEqual(ret, 0, "rdma_listen() failed");
  info("start listening, address: %s:%s", host, port); 

  // Now, register a conn_event
  // In order to avoid block
  base_ = event_base_new();
  checkNotEqual(base_, static_cast<event_base*>(nullptr), "event_base_new() failed");
  conn_event_ = event_new(base_, cm_event_channel_->fd, EV_READ | EV_PERSIST, &Server::onConnecetionEvent, this); // bind the event to channel fd
  checkNotEqual(conn_event_, static_cast<event*>(nullptr), "event_new() failed to create conn_event");
  ret = event_add(conn_event_, nullptr); // register the event
  checkEqual(ret, 0, "event_add() failed to register conn_event");

  // register a exit_event
  exit_event_ = event_new(base_, SIGINT, EV_SIGNAL, &Server::onExitEvent, this);
  checkNotEqual(exit_event_, static_cast<event*>(nullptr), "event_new() failed to create exit_event");
  ret = event_add(exit_event_, nullptr);
  checkEqual(ret, 0, "event_add() failed to register exit_event");
}


void Server::onConnecetionEvent([[gnu::unused]]evutil_socket_t fd, [[gnu::unused]]short what, void* arg) {
  reinterpret_cast<Server*>(arg) -> handleConnecetionEvent();
}

void Server::handleConnecetionEvent() {
  rdma_cm_event* cm_ev;
  // wait the client connect, blocking
  // But! must get value immediately because conn_event, so it's like non-blocking
  int ret = rdma_get_cm_event(cm_event_channel_, &cm_ev);
  checkEqual(ret, 0, "rdma_get_cm_event() failed");

  if (cm_ev->status != 0) {
    info("got a bad cm_event");
    return;
  }

  switch(cm_ev->event) {
    case RDMA_CM_EVENT_CONNECT_REQUEST: {
      info("start to handle a connection request");
      setupConnection(cm_ev, 1024, cur_conn_id_++);
      break;
    }
    case RDMA_CM_EVENT_ESTABLISHED: {
      int ret = rdma_ack_cm_event(cm_ev);
      wCheckEqual(ret, 0, "rdma_ack_cm_event() failed to ack event");
      info("establish the connection");
      break;
    }
    case RDMA_CM_EVENT_DISCONNECTED: {
      Connection* conn = reinterpret_cast<Connection*>(cm_ev->id->context);
      conn_map_[conn->getId()] = nullptr;
      delete conn;
      info("delete the connection");

      int ret = rdma_ack_cm_event(cm_ev);
      wCheckEqual(ret, 0, "rdma_ack_cm_event() failed to ack event");
      break;
    }
    default: {
      info("unexpected event: %s", rdma_event_str(cm_ev->event));
      break;
    }
  }

}

void Server::onExitEvent([[gnu::unused]]evutil_socket_t fd, [[gnu::unused]]short what, void* arg) {
  reinterpret_cast<Server*>(arg) -> handleExitEvent();
}

void Server::handleExitEvent() {
  int ret = event_base_loopbreak(base_);
  checkEqual(ret, 0, "event_base_loopbreak() failed");
  info("stop event loop");
}

void Server::run() {
  info("start event loop for conection");
  event_base_dispatch(base_);
}

void Server::setupConnection(rdma_cm_event* cm_event, uint32_t n_buffer_page, uint32_t conn_id) {
  Connection* conn = new Connection(this, cm_event->id, n_buffer_page, conn_id);
  rdma_conn_param param = conn->copyConnParam();
  int ret = rdma_accept(cm_event->id, &param);
  checkEqual(ret, 0, "rdma_accept() failed");
  uint32_t rkey = *(uint32_t*)(cm_event->param.conn.private_data);
  conn->setRkey(rkey);
  
  ret = rdma_ack_cm_event(cm_event);
  wCheckEqual(ret, 0, "rdma_ack_cm_event() failed to ack event");
  info("accept the connection");

  cm_event->id->context = conn; // in order to release when client disconnect

  conn_map_[conn->getId()] = conn;
}