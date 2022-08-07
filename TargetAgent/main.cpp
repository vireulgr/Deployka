#include <iostream>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/shared_ptr.hpp"
#include <map>
//#include "boost/asio/impl/src.hpp"

#include "networkLib.h"

/******************************************************************************//*
*
*********************************************************************************/
class DeploykaTcpConnection: public boost::enable_shared_from_this<DeploykaTcpConnection> {

  std::vector<Deployka::MemberType> m_memberInfoDeclare;
  std::vector<Deployka::MemberInfo> m_memberInfo;

  std::array<unsigned char, 8196> m_receiveBuffer;
  std::array<unsigned char, 8196> m_messageBuffer;

  size_t m_msgReceived;

  Deployka::MessageType m_currentMessageType;

  boost::asio::ip::tcp::socket m_sock;

  //================================================================================
  DeploykaTcpConnection(boost::asio::io_context& ctx)
    : m_sock(ctx)
    , m_msgReceived(0)
    , m_currentMessageType(Deployka::DMT_Null)
  {
    cleanupReceiveState();
    // m_memberInfo will be initialized after command is received
  }

  void cleanupMemberInfo() {
    std::transform(
      m_memberInfo.begin(),
      m_memberInfo.end(),
      m_memberInfo.begin(),
      [](Deployka::MemberInfo& mi) {
        mi.buffer.clear();
        mi.done = false;
        return mi; 
      });
  }

  void cleanupReceiveState() {
    m_messageBuffer.fill(0);
    m_receiveBuffer.fill(0);
    m_msgReceived = 0;
    m_currentMessageType = Deployka::DMT_Null;
  }

public:
  typedef boost::shared_ptr<DeploykaTcpConnection> pointer;

  boost::asio::ip::tcp::socket & socket() { return m_sock; }

  //================================================================================
  static pointer create(boost::asio::io_context& ctx) {
    DeploykaTcpConnection * conn = new DeploykaTcpConnection(ctx);
    std::cout << "[conn::create]          ptr: 0x" << conn << '\n';
    return pointer(conn);
  }

  //================================================================================
  void start() {

    std::cout << "[conn::start]          this: 0x" << this << '\n';

    m_sock.async_receive(boost::asio::buffer(m_receiveBuffer),
      boost::bind(&DeploykaTcpConnection::handleRead,
        shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
  }

  //================================================================================
  void processReadedData() {
    std::cout << "[conn::processReadedData]\n";

    if (m_currentMessageType == Deployka::DMT_Null
      && m_msgReceived >= sizeof(m_currentMessageType))
    {
      memcpy(&m_currentMessageType, m_messageBuffer.data(), sizeof(m_currentMessageType));

      std::cout << "received command: " << m_currentMessageType << '\n';
      m_memberInfo.clear();
      Deployka::MessageType msgType = (Deployka::MessageType)m_currentMessageType;
      m_memberInfo = Deployka::buildMemberInfo(Deployka::g_commands.at(msgType));
    }

    // process rest of received data in m_messageBuffer

    size_t dynamicOffset = 0;
    for (int i = 0; i < m_memberInfo.size(); ++i) {
      Deployka::MemberInfo& mi = m_memberInfo[i];
      //std::cout << "offs: " << mi.memberOffset << "; size: " << mi.memberOffset << '\n';
      if (m_msgReceived >= mi.memberOffset + mi.memberSize + dynamicOffset
        && !mi.done)
      { //std::cout << "mo: " << mi.memberOffset << "; ms: " << mi.memberSize  << "; dynoffset: " << dynamicOffset << '\n';

        mi.buffer.reserve(mi.memberSize);
        auto arrayItBeg = m_messageBuffer.begin() + mi.memberOffset + dynamicOffset;
        std::copy(arrayItBeg, arrayItBeg + mi.memberSize, std::back_inserter(mi.buffer));

        mi.done = true;
        //std::cout << "buf size: " << mi.buffer.size() << " ????????????????\n";
        switch (mi.memberType) {
        case Deployka::MT_longLong: {
          long long int val = 0;
          memcpy(&val, mi.buffer.data(), mi.memberSize);
          std::cout << "long long val: " << val << '\n';
          break;
        }
        case Deployka::MT_longDouble: {
          long double val = 0;
          memcpy(&val, mi.buffer.data(), mi.memberSize);
          std::cout << "long double val: " << val << '\n';
          break;
        }
        case Deployka::MT_dynamic: {
          std::string val;
          val.reserve(mi.buffer.size());
          for (size_t j = 0; j < mi.memberSize; j++) {
            val.push_back(*(mi.buffer.data() + j));
          }
          std::cout << "dynamic: \"" << val << "\"\n";

          dynamicOffset += mi.memberSize;
          break;
        }
        case Deployka::MT_dynamicSize: {
          if ((i < m_memberInfo.size()-1)
            && (m_memberInfo[i + 1].memberType == Deployka::MT_dynamic))
          {
            size_t & dynamicMemberSize = m_memberInfo[i + 1].memberSize;
            memcpy(&dynamicMemberSize, mi.buffer.data(), mi.memberSize);
          }
          break;
        }
        }
      }
    }
  }

  //================================================================================
  void handleRead(boost::system::error_code const& ec, size_t received) {
    std::cout << "[conn::handleRead]=====this: 0x" << this << "========\n";
    if (ec) {
      std::cerr << "[conn::handleRead] error:" << ec.what() << " (" << ec.value() << ")\n";
      return;
    }

    memcpy(m_messageBuffer.data() + m_msgReceived, m_receiveBuffer.data(), received);

    m_msgReceived += received;

    std::cout << "received: " << received << "; msgReceived: " << m_msgReceived << '\n';

    processReadedData();

    // if all command is received: cleanup receive state
    if (std::all_of(m_memberInfo.cbegin(), m_memberInfo.cend(), [](Deployka::MemberInfo const& mi) { return mi.done; })) {
      cleanupMemberInfo();
      cleanupReceiveState();
    }

    m_sock.async_receive(boost::asio::buffer(m_receiveBuffer),
      boost::bind(&DeploykaTcpConnection::handleRead,
        shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
  }

  //================================================================================
  void handleWrite() {
    std::cout << "[conn::handleWrite]\n";
  }
};

/******************************************************************************//*
*
*********************************************************************************/
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

/******************************************************************************//*
*
*********************************************************************************/
int main() {
  std::cout << "Hello from Target Agent!" << std::endl;
  boost::asio::io_context context;
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

  return 0;
}
