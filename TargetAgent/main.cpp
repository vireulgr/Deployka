#include <iostream>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/thread.hpp"
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/shared_ptr.hpp"

#include "..\common\DeploykaNet.h"
//#include "boost/asio/impl/src.hpp"

class DeploykaTcpConnection: public boost::enable_shared_from_this<DeploykaTcpConnection> {

  DeploykaTcpConnection(boost::asio::io_context& ctx)
    : m_sock(ctx)
    , m_msgReceived(0)
  {
    m_messageBuffer.fill(0);
    m_receiveBuffer.fill(0);
    m_msg.m_size = -1;
    m_msg.m_type = Deployka::DMT_Null;
  }

  std::array<unsigned char, 8196> m_receiveBuffer;
  std::array<unsigned char, 8196> m_messageBuffer;
  size_t m_msgReceived;

  boost::asio::ip::tcp::socket m_sock;

  Deployka::Message m_msg;

public:
  typedef boost::shared_ptr<DeploykaTcpConnection> pointer;

  boost::asio::ip::tcp::socket & socket() { return m_sock; }

  static pointer create(boost::asio::io_context& ctx) {
    DeploykaTcpConnection * conn = new DeploykaTcpConnection(ctx);
    std::cout << "[conn::create]          ptr: 0x" << conn << '\n';
    return pointer(conn);
  }

  void start() {
    std::cout << "[conn::start]          this: 0x" << this << '\n';
    // TODO
    m_sock.async_receive(boost::asio::buffer(m_receiveBuffer),
      boost::bind(&DeploykaTcpConnection::handleRead,
        shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
  }

  //void a (size_t received) {
  //  memcpy(m_messageBuffer.data(), m_receiveBuffer.data() + m_msgReceived, received);
  //  m_msgReceived += received;
  //  if (m_msgReceived > member.offset + member.size) {
  //    receiveMember(member);

  //  }
  //}

  void handleRead(boost::system::error_code const & ec, size_t received) {
    std::cout << "[conn::handleRead]=====this: 0x" << this << "========\n";
    if (ec) {
      std::cerr << "[conn::handleRead] error:" << ec.what() << " (" << ec.value() << ')' << std::endl;
      return;
    }

    std::cout << "[conn::handleRead] received " << received << '\n';
    size_t receivedUsed = 0;

    size_t thisMemberOffset = 0;
    size_t thisMemberSize = sizeof(m_msg.m_size);
    if (m_msgReceived + (received - receivedUsed) >= thisMemberOffset
      && m_msgReceived < thisMemberOffset + thisMemberSize) {

      size_t thisReceivedUse = std::min(sizeof(m_msg.m_size) - (m_msgReceived - receivedUsed), received - receivedUsed);
      std::cout << "[conn::handleRead] m_size thisReceivedUse: " << thisReceivedUse << std::endl;;
      memcpy(m_messageBuffer.data() + m_msgReceived, m_receiveBuffer.data(), thisReceivedUse);

      if (thisReceivedUse + m_msgReceived == sizeof(m_msg.m_size)) {
        std::cout << "[conn::handleRead] m_size rd from " << thisMemberOffset << " sz: " << thisMemberSize << '\n';
        memcpy(&(m_msg.m_size), m_messageBuffer.data() + thisMemberOffset, thisMemberSize);
        std::cout << "[conn::handleRead] m_size received!; Value: " << m_msg.m_size << '\n';
        m_msgReceived += thisReceivedUse;
      }
      receivedUsed += thisReceivedUse;
    }

    if (receivedUsed >= received) {
      m_msgReceived = receivedUsed;
      std::cout << "[conn::handleRead] received " << received << " total: " << m_msgReceived << '\n';
      start();
      return;
    }

    std::cout << "[conn::handleRead] received Used: " << receivedUsed << "; msgReceived " << m_msgReceived << std::endl;

    /*size_t*/ thisMemberOffset = sizeof(m_msg.m_size);
    /*size_t*/ thisMemberSize = sizeof(m_msg.m_type);
    if (m_msgReceived + (received - receivedUsed) >= thisMemberOffset
      && m_msgReceived < thisMemberOffset + thisMemberSize) {

      size_t thisReceivedUse = std::min(sizeof(m_msg.m_type) - (m_msgReceived - receivedUsed), received - receivedUsed);
      std::cout << "[conn::handleRead] m_type thisReceivedUse: " << thisReceivedUse << '\n';
      memcpy(m_messageBuffer.data() + m_msgReceived, m_receiveBuffer.data(), thisReceivedUse);

      if (thisReceivedUse + m_msgReceived == sizeof(m_msg.m_type)) {
        std::cout << "[conn::handleRead] m_type rd from " << thisMemberOffset << " sz: " << thisMemberSize << '\n';
        memcpy(&(m_msg.m_type), m_messageBuffer.data() + thisMemberOffset, thisMemberSize);
        std::cout << "[conn::handleRead] m_type received!; Value: " << m_msg.m_type << '\n';
        m_msgReceived += thisReceivedUse;
      }
      receivedUsed += thisReceivedUse;
    }

    if (receivedUsed >= received) {
      m_msgReceived = receivedUsed;
      std::cout << "[conn::handleRead] received " << received << " total: " << m_msgReceived << '\n';
      start();
      return;
    }

    std::cout << "[conn::handleRead] restarting receive" << std::endl;

    start();
  }

  void handleWrite() {
    std::cout << "[conn::handleWrite]\n";
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

  void handleAccept(DeploykaTcpConnection::pointer newConnection, boost::system::error_code const & ec) {
    std::cout << "[serv::handleAccept] con ptr 0x" << newConnection.get() << '\n';
    if (!ec) {
      newConnection->start();
    }
    startAccept();
  }

  void startAccept() {
    DeploykaTcpConnection::pointer newConnection = DeploykaTcpConnection::create(m_ctx);
    std::cout << "[serv::startAccept] con ptr: 0x" << newConnection.get() << '\n';

    m_acceptor.async_accept(newConnection->socket(),
      boost::bind(&DeploykaTcpServer::handleAccept,
        this,
        newConnection,
        boost::asio::placeholders::error));
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
  try {
    DeploykaTcpServer server(context);
    //
    std::cout << "[main]                after server creation\n";

    context.run();

    std::cout << "[main]                after context run\n";
  }
  catch (std::exception& e) {
    std::cerr << "exception in main: " << e.what() << std::endl;
  }

  //ip::tcp::endpoint ep(ip::tcp::v4(), 2001);
  //ip::tcp::acceptor acc(context, ep);

  //while (true) {
  //  boost::shared_ptr<ip::tcp::socket> sock(new ip::tcp::socket(context));
  //  acc.accept(*sock);
  //  boost::thread t(boost::bind(client_session, sock));
  //  t.join();
  //}

  return 0;
}
