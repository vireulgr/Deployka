#include <iostream>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/thread.hpp"
#include "boost/asio.hpp"
#include "boost/bind.hpp"

#include "..\common\DeploykaNet.h"
//#include "boost/asio/impl/src.hpp"

class DeploykaTcpConnection: boost::enable_shared_from_this<DeploykaTcpConnection> {
  boost::asio::ip::tcp::socket m_sock;

  DeploykaTcpConnection(boost::asio::io_context& ctx)
    : m_sock(ctx)
  {
    m_msg.m_size = -1;
    m_msg.m_type = Deployka::DMT_Null;
  }

  std::array<unsigned char, 8196> m_arrayBuffer;

  Deployka::Message m_msg;

public:
  typedef boost::shared_ptr<DeploykaTcpConnection> pointer;

  boost::asio::ip::tcp::socket & socket() { return m_sock; }

  static pointer create(boost::asio::io_context& ctx) {
    pointer result = boost::make_shared<DeploykaTcpConnection>(ctx);
    return result;
  }

  void start() {
    // TODO
    m_sock.async_receive(boost::asio::buffer(m_arrayBuffer),
      boost::bind(DeploykaTcpConnection::handleRead, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
  }

  void handleRead(boost::system::error_code const & ec, size_t received) {
    if (m_msg.m_size < 0) {
      if (received >= sizeof(Deployka::Message::m_size)) {
        int size = 0;
        std::memcpy(&size, m_arrayBuffer.data(), 4);
      }
    }
  }

  void handleWrite() {
  }
};

class DeploykaTcpServer {
  boost::asio::io_context& m_ctx;
  boost::asio::ip::tcp::acceptor m_acceptor;
public:
  DeploykaTcpServer(boost::asio::io_context& ctx)
    : m_ctx(ctx)
    , m_acceptor(ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), Deployka::TCP_PORT))
  {
    startAccept();
  }

  void handleAccept(DeploykaTcpConnection* newConnection, boost::system::error_code const & ec) {
    if (!ec) {
      newConnection->start();
    }
    startAccept();
  }

  void startAccept() {
    DeploykaTcpConnection::pointer newConnection = DeploykaTcpConnection::create(m_ctx);

    m_acceptor.async_accept(newConnection->socket(),
      boost::bind(DeploykaTcpServer::handleAccept, this, newConnection, boost::asio::placeholders::error));
  }

};


using namespace boost::asio;

//typedef boost::shared_ptr<ip::tcp::socket> socket_ptr;

void client_session(boost::shared_ptr<ip::tcp::socket> sock) {
  while (true) {
    char data[512];
    size_t len = sock->read_some(buffer(data));

    if (len > 0) {
      write(*sock, buffer("ok", 2));
    }
  }
}


int main() {
  
  std::cout << "Hello from Target Agent!" << std::endl;
  io_context context;
  ip::tcp::endpoint ep(ip::tcp::v4(), 2001);
  ip::tcp::acceptor acc(context, ep);

  while (true) {
    boost::shared_ptr<ip::tcp::socket> sock(new ip::tcp::socket(context));
    acc.accept(*sock);
    boost::thread(boost::bind(client_session, sock));
  }
  return 0;
}
