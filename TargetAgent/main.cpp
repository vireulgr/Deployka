#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/shared_ptr.hpp"
#include <iostream>
#include <map>
//#include "boost/asio/impl/src.hpp"

#include "networkLib.h"

constexpr size_t RECV_BUF_SIZE = 8196;


/******************************************************************************//*
*
*********************************************************************************/
class DeploykaTcpConnection: public boost::enable_shared_from_this<DeploykaTcpConnection> {

  std::vector<Deployka::MemberType> m_memberInfoDeclare;
  std::vector<Deployka::MemberInfo> m_memberInfo;


  std::array<unsigned char, RECV_BUF_SIZE> m_receiveBuffer;
  std::array<unsigned char, RECV_BUF_SIZE> m_messageBuffer;
  std::vector<unsigned char> m_pending;

  int m_nextMemberIndex;
  size_t m_consumed;
  size_t m_msgReceived;
  size_t m_dynamicOffset; // sum of length of all dynamic data members that has been processed so far

  Deployka::MessageType m_currentMessageType;

  boost::asio::ip::tcp::socket m_sock;

  //================================================================================
  DeploykaTcpConnection(boost::asio::io_context& ctx)
    : m_sock(ctx)
    , m_msgReceived(0)
    , m_dynamicOffset(0)
    , m_nextMemberIndex(0)
    , m_consumed(0)
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
      [] (Deployka::MemberInfo& mi) {
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
    m_dynamicOffset = 0;
    m_nextMemberIndex = 0;
    m_consumed = 0;
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
  void saveResidualToPending() {
    if (m_consumed >= m_msgReceived) {
      return;
    }
    size_t toCopy = std::min(m_msgReceived - m_consumed, RECV_BUF_SIZE);
    size_t offset = RECV_BUF_SIZE - toCopy;
    std::cout << "[conn::saveResidualToPending] offs: " << offset << "; toCopy: " << toCopy << '\n';
    std::copy(m_messageBuffer.begin() + offset, m_messageBuffer.end(), std::back_inserter(m_pending));
    std::cout << "[conn::saveResidualToPending] pending size: " << m_pending.size() << '\n';
  }

  //================================================================================
  void processReadedData() {
    std::cout << "[conn::procRdData] in\n";

    for (int i = m_nextMemberIndex; i < m_memberInfo.size(); ++i) {
      Deployka::MemberInfo& mi = m_memberInfo[i];
      std::cout << "[conn::procRdData] "
        "               mbrOff: " << mi.memberOffset << "; mbrSize: " << mi.memberSize << "; dynOffs: " << m_dynamicOffset << '\n';

      if (m_msgReceived < mi.memberOffset + mi.memberSize + m_dynamicOffset
        && (i > m_nextMemberIndex || i == 0)) {
        m_nextMemberIndex = i;
        m_consumed += mi.memberOffset + m_dynamicOffset;
        saveResidualToPending();
        std::cout << "[conn::procRdData] Next member (" << i << ") is not received yet; consumed " << m_consumed << "\n";
        break;
      }

      if (m_msgReceived >= mi.memberOffset + mi.memberSize + m_dynamicOffset // mi.memberOffset + m_dynamicOffset == m_consumed
        && !mi.done)
      { //std::cout << "mo: " << mi.memberOffset << "; ms: " << mi.memberSize  << "; dynoffset: " << m_dynamicOffset << '\n';

        mi.buffer.reserve(mi.memberSize);
        if (mi.memberSize < m_pending.size()) {
          std::cout << "ERROR pending size cannot be more than member size!";
        }
        if (!m_pending.empty()) {
          size_t thisBufDataSize = mi.memberSize - m_pending.size();
          std::cout << "[conn::procRdData] thisBufDataSize " << thisBufDataSize << "\n";
          std::copy(m_pending.begin(), m_pending.end(), std::back_inserter(mi.buffer));
          std::copy(m_messageBuffer.begin(), m_messageBuffer.begin() + thisBufDataSize, std::back_inserter(mi.buffer));
          m_pending.clear();
        }
        else {
          auto arrayItBeg = m_messageBuffer.begin() + mi.memberOffset + m_dynamicOffset - m_consumed;
          std::copy(arrayItBeg, arrayItBeg + mi.memberSize, std::back_inserter(mi.buffer));
        }

        mi.done = true;

        switch (mi.memberType) {
        case Deployka::MT_longLong: {
          long long int val = 0;
          memcpy(&val, mi.buffer.data(), mi.memberSize);
          std::cout << "[conn::procRdData] long long val: " << val << '\n';
          break;
        }
        case Deployka::MT_longDouble: {
          long double val = 0;
          memcpy(&val, mi.buffer.data(), mi.memberSize);
          std::cout << "[conn::procRdData] long double val: " << val << '\n';
          break;
        }
        case Deployka::MT_dynamic: {
          std::string val(mi.buffer.begin(), mi.buffer.end());
          //val.reserve(mi.buffer.size());
          //for (size_t j = 0; j < mi.memberSize; j++) {
          //  val.push_back(*(mi.buffer.data() + j));
          //}
          val.back() = '\0';

          if (val.size() > 515) {
            size_t endClamp = val.size() - 256;
            std::cout << "[conn::procRdData] dynamic VAL (offset: " << m_dynamicOffset << "): " << val.substr(0, 256) << "\n...\n" << val.substr(endClamp) << "[end val]\n";
          }
          else {
            std::cout << "[conn::procRdData] dynamic VAL (offset: " << m_dynamicOffset << "): " << val << "[end val]\n";
          }

          m_dynamicOffset += mi.memberSize;
          
          break;
        }
        case Deployka::MT_dynamicSize: {
          if ((i < m_memberInfo.size()-1)
            && (m_memberInfo[i + 1].memberType == Deployka::MT_dynamic))
          {
            size_t & dynamicMemberSize = m_memberInfo[i + 1].memberSize;
            memcpy(&dynamicMemberSize, mi.buffer.data(), mi.memberSize);

            std::cout << "[conn::procRdData] dynamic SIZE: \"" << m_memberInfo[i+1].memberSize << "\"\n";
          }
          break;
        }
        }
      }
    }

    std::cout << "[conn::procRdData] out\n";
  }

  //================================================================================
  void handleRead(boost::system::error_code const& ec, size_t received) {
    std::cout << "[conn::handleRead]=====this: 0x" << this << "==========================\n";
    if (ec) {
      std::cerr << "[conn::handleRead] error: " << ec.what() << " (" << ec.value() << ")\n";
      return;
    }

    //if (m_msgReceived - m_consumed + received > 8196) {
    //  std::cout << "[conn::handleRead] ERROR! Receive buffer overflow!\n";
    //  std::cout << "[conn::handleRead] m_msgReceived: " << m_msgReceived << "; this received: " << received << "; total message buf size: " << 8196 << '\n';
    //  cleanupReceiveState();
    //  return;
    //}

    //memcpy(m_messageBuffer.data() + m_msgReceived - m_consumed, m_receiveBuffer.data(), received);

    memcpy(m_messageBuffer.data(), m_receiveBuffer.data(), received);

    m_msgReceived += received;

    std::cout << "[conn::handleRead] received: " << received << "; msgReceived: " << m_msgReceived << "; consumed: " << m_consumed << '\n';

    if (m_currentMessageType == Deployka::DMT_Null
      && m_msgReceived >= sizeof(m_currentMessageType))
    {
      // here we copy always from zero offset
      memcpy(&m_currentMessageType, m_messageBuffer.data(), sizeof(m_currentMessageType));

      if (m_currentMessageType >= Deployka::MessageType::DMT_MessageTypeMax) {
        std::cout << "[conn::handleRead] ERROR: message type received is " << m_currentMessageType << '\n';
        throw std::logic_error("Wrong message type");
      }

      std::cout << "[conn::handleRead] received command: >>>>>>" << m_currentMessageType << "<<<<<<\n";
      m_memberInfo.clear();
      Deployka::MessageType msgType = (Deployka::MessageType)m_currentMessageType;
      m_memberInfo = Deployka::buildMemberInfo(Deployka::g_commands.at(msgType));
    }

    // process rest of received data in m_messageBuffer

    processReadedData();

    // if all command is received: cleanup receive state
    if (std::all_of(m_memberInfo.cbegin(), m_memberInfo.cend(), [](Deployka::MemberInfo const& mi) { return mi.done; })) {
      cleanupMemberInfo();
      cleanupReceiveState();
      std::cout << "[conn::handleRead] after cleanup received state\n";
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
