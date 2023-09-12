#include <server.h>
#include <util.h>
#include <const.h>
#include <iostream>
#include <mutex>

Server::Server(const char* host, const char* port) {

  int ret = 0;
  ret = getaddrinfo(host, port, nullptr, &addr_);
  checkEqual(ret, 0, "getaddrinfo() failed");

  // Open a channel used to report asynchronous communication event
  cm_event_channel_ = rdma_create_event_channel();
  checkNotEqual(cm_event_channel_, static_cast<rdma_event_channel*>(nullptr), "rdma_create_event_channel() failed, cm_event_channel_ == nullptr");
  
  // rdma_cm_id is the connection identifier (like socket) which is used to define an RDMA connection. 
	ret = rdma_create_id(cm_event_channel_, &listen_cm_id_, nullptr, RDMA_PS_TCP);
  checkEqual(ret, 0, "rdma_create_id() failed");

  // Explicit binding of rdma cm id to the socket credentials
  ret = rdma_bind_addr(listen_cm_id_, addr_->ai_addr);
  checkEqual(ret, 0 ,"rdma_bind_addr() failed");

  // Now we start to listen on the passed IP and port. However unlike
	// normal TCP listen, this is a non-blocking call. When a new client is 
	// connected, a new connection management (CM) event is generated on the 
	// RDMA CM event channel from where the listening id was created. Here we
	// have only one channel, so it is easy.
  ret = rdma_listen(listen_cm_id_, DEFAULT_BACK_LOG); // backlog is the max clients num, same as tcp, see 'man listen'
  checkEqual(ret, 0, "rdma_listen() failed");
  info("start listening, address: %s:%s", host, port); 

  // Now, register a conn_event
  // In order to avoid block
  base_ = event_base_new();
  checkNotEqual(base_, static_cast<event_base*>(nullptr), "event_base_new() failed");
  conn_event_ = event_new(base_, cm_event_channel_->fd, EV_READ | EV_PERSIST, &Server::onConnectionEvent, this); // bind the event to channel fd
  checkNotEqual(conn_event_, static_cast<event*>(nullptr), "event_new() failed to create conn_event");
  ret = event_add(conn_event_, nullptr); // register the event
  checkEqual(ret, 0, "event_add() failed to register conn_event");
}

Server::~Server() {
  event_base_free(base_);
  event_free(conn_event_);
  event_free(exit_event_);
  int ret = event_base_loopbreak(base_);
  rdma_destroy_event_channel(cm_event_channel_);
  freeaddrinfo(addr_);
  info("clean up the server resources");
}


void Server::onConnectionEvent([[gnu::unused]]evutil_socket_t fd, [[gnu::unused]]short what, void* arg) {
  reinterpret_cast<Server*>(arg) -> handleConnectionEvent();
}

void Server::handleConnectionEvent() {
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
      setupConnection(cm_ev, 64);
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
      int ret = rdma_ack_cm_event(cm_ev);
      info("send disconnect ack");
      wCheckEqual(ret, 0, "rdma_ack_cm_event() failed to ack event");
      poller_.deregisterConn(conn);
      info("delete the connection");
      break;
    }
    default: {
      info("unexpected event: %s", rdma_event_str(cm_ev->event));
      break;
    }
  }

}


void Server::run() {
  // run poller
  poller_.run();
  // listen connection
  info("start event loop for conection");
  info("==============================");
  event_base_dispatch(base_);
}

void Server::setupConnection(rdma_cm_event* cm_event, uint32_t n_buffer_page) {

  rdma_cm_id* client_id = cm_event->id;
  Connection* conn = new Connection(Role::ServerConn, client_id, n_buffer_page);
  rdma_conn_param param = conn->copyConnParam();
  int ret = rdma_accept(client_id, &param);
  checkEqual(ret, 0, "rdma_accept() failed");
  uint32_t rkey = *(uint32_t*)(cm_event->param.conn.private_data);
  conn->setRkey(rkey);
  
  ret = rdma_ack_cm_event(cm_event);
  wCheckEqual(ret, 0, "rdma_ack_cm_event() failed to ack event");
  info("accept the connection");

  poller_.registerConn(conn);

  client_id->context = reinterpret_cast<void*>(conn); // in order to release when client disconnect
}


/* ServerPoller */
ServerPoller::ServerPoller() {

}

ServerPoller::~ServerPoller() {
  std::lock_guard<Spinlock> lock(lock_);
  conn_list_.clear();
}

void ServerPoller::registerConn(Connection* conn) {
  std::lock_guard<Spinlock> lock(lock_);
  conn_list_.emplace_back(conn);
}

void ServerPoller::deregisterConn(Connection* conn) {
  std::lock_guard<Spinlock> lock(lock_);
  for (auto it = conn_list_.begin(); it != conn_list_.end(); it++) {
    if ((*it) == conn) {
      conn_list_.erase(it);
      delete conn;
      break;
    }
  }
}

void ServerPoller::run() {
  running_.store(true, std::memory_order_release);
  poll_thread_ = std::thread(&ServerPoller::poll, this);
  info("start running server poller");
}

void ServerPoller::stop() {
  if (running_.load(std::memory_order_acquire)) {
    running_.store(false, std::memory_order_release);
    poll_thread_.join();
    info("connection poller stopped");
  }
}

void ServerPoller::poll() {
  while (running_.load(std::memory_order_acquire)) {
    std::lock_guard<Spinlock> lock(lock_);
    for (auto conn : conn_list_) {
      conn->poll();
    }
  }
}
