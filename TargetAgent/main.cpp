#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/shared_ptr.hpp"
#include <vector>
#include <iostream>
#include <iomanip>
#include <map>
#include <climits>

//#define _TESTS_

#ifdef _TESTS_
#include "tests.h"
#endif
//#include "boost/asio/impl/src.hpp"

#include "networkLib.h"


/******************************************************************************//*
*
*********************************************************************************/
struct DeploykaMessageReceiver {
  std::vector<Deployka::MemberInfo> m_memberInfo;

  std::array<unsigned char, Deployka::RECV_BUF_SIZE> m_messageBuffer;
  std::vector<unsigned char> m_pending;

  Deployka::ReceiveStream drs;

  int m_nextMemberIndex;
  size_t m_consumed;
  size_t m_msgReceived;
  size_t m_dynamicOffset; // sum of length of all dynamic data members that has been processed so far

  Deployka::MessageType m_currentMessageType;

  DeploykaMessageReceiver() 
    : m_msgReceived(0)
    , m_currentMessageType(Deployka::DMT_Null)
    , m_dynamicOffset(0)
    , m_nextMemberIndex(0)
    , m_consumed(0)
  {
    m_messageBuffer.fill(0);
  }

  //================================================================================
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

  //================================================================================
  void cleanupReceiveState() {
    m_messageBuffer.fill(0);
    m_msgReceived = 0;
    m_currentMessageType = Deployka::DMT_Null;
    m_dynamicOffset = 0;
    m_nextMemberIndex = 0;
    m_consumed = 0;
  }

  //================================================================================
  void receive(unsigned char const* data, size_t dataSize) {
    drs.addBuffer(data, dataSize);
    //memcpy(m_messageBuffer.data(), data, dataSize);

    m_msgReceived += dataSize;

    std::cout << "[recvr::receive] received: " << dataSize << "; msgReceived: " << m_msgReceived << "; consumed: " << m_consumed << '\n';

    if (m_currentMessageType == Deployka::DMT_Null
      && m_msgReceived >= sizeof(m_currentMessageType))
    {
      // here we copy always from zero offset
      memcpy(&m_currentMessageType, m_messageBuffer.data(), sizeof(m_currentMessageType));

      if (m_currentMessageType >= Deployka::MessageType::DMT_MessageTypeMax) {
        // DEBUG
        //std::vector<unsigned char> vec;
        //vec.reserve(m_msgReceived);
        //vec.assign(m_messageBuffer.begin(), m_messageBuffer.begin()+m_msgReceived);
        //Deployka::printHex(vec);

        //vec.assign(m_messageBuffer.begin(), m_messageBuffer.begin() + 44);
        //Deployka::printString(vec);

        //std::transform(m_messageBuffer.begin() + 44, m_messageBuffer.begin() + m_msgReceived, m_messageBuffer.begin(), [](char c) { return c; });

        //memcpy(&m_currentMessageType, m_messageBuffer.data(), sizeof(m_currentMessageType));
        //std::cout << "cur msg type: " << m_currentMessageType << '\n';
        // /DEBUG

        std::cout << "[recvr::receive] ERROR: message type received is " << m_currentMessageType << '\n';
        throw std::logic_error("Wrong message type");
      }

      std::cout << "[recvr::receive] received command: >>>>>>" << m_currentMessageType << "<<<<<<\n";
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
      std::cout << "[recvr::receive] after cleanup received state\n";
    }

  }
  //================================================================================
  void saveResidualToPending() {
    if (m_consumed >= m_msgReceived) {
      return;
    }

    size_t toCopy = std::min(m_msgReceived - m_consumed, Deployka::RECV_BUF_SIZE);
    size_t offset = Deployka::RECV_BUF_SIZE - toCopy;
    std::cout << "[recvr::saveResidualToPending] offs: " << offset << "; toCopy: " << toCopy << '\n';
    std::copy(m_messageBuffer.begin() + offset, m_messageBuffer.end(), std::back_inserter(m_pending));
    std::cout << "[recvr::saveResidualToPending] pending size: " << m_pending.size() << '\n';
  }

  //================================================================================
  void processReadedData() {
    std::cout << "[recvr::procRdData] in                        mbrOff          mbrSize           dynOffs\n";

    for (int i = m_nextMemberIndex; i < m_memberInfo.size(); ++i) {
      Deployka::MemberInfo& mi = m_memberInfo[i];
      std::cout << "[recvr::procRdData] "
         << std::setw(30) << mi.memberOffset
         << std::setw(17) << mi.memberSize
         << std::setw(17) << m_dynamicOffset
         << '\n';

      if (m_msgReceived < mi.memberOffset + mi.memberSize + m_dynamicOffset
        && (i > m_nextMemberIndex || i == 0)) {
        m_nextMemberIndex = i;
        m_consumed += mi.memberOffset + m_dynamicOffset;
        saveResidualToPending();
        std::cout << "[recvr::procRdData] Next member (" << i << ") is not received yet; consumed " << m_consumed << "\n";
        break;
      }

      if (m_msgReceived >= mi.memberOffset + mi.memberSize + m_dynamicOffset // mi.memberOffset + m_dynamicOffset == m_consumed
        && !mi.done)
      { //std::cout << "mo: " << mi.memberOffset << "; ms: " << mi.memberSize  << "; dynoffset: " << m_dynamicOffset << '\n';

        mi.buffer.reserve(mi.memberSize);
        if (mi.memberSize < m_pending.size()) {
          std::cout << "[recvr::procRdData] ERROR pending size cannot be more than member size!";
        }
        if (!m_pending.empty()) {
          size_t thisBufDataSize = mi.memberSize - m_pending.size();
          std::cout << "[recvr::procRdData] thisBufDataSize " << thisBufDataSize << "\n";
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
          std::cout << "[recvr::procRdData] long long val: " << val << '\n';
          break;
        }
        case Deployka::MT_longDouble: {
          long double val = 0;
          memcpy(&val, mi.buffer.data(), mi.memberSize);
          std::cout << "[recvr::procRdData] long double val: " << val << '\n';
          break;
        }
        case Deployka::MT_dynamic: {
          //Deployka::printDynamic(mi);
          Deployka::printString(mi.buffer);

          m_dynamicOffset += mi.memberSize;
          
          break;
        }
        case Deployka::MT_dynamicSize: {
          if ((i < m_memberInfo.size()-1)
            && (m_memberInfo[i + 1].memberType == Deployka::MT_dynamic))
          {
            size_t & dynamicMemberSize = m_memberInfo[i + 1].memberSize;
            memcpy(&dynamicMemberSize, mi.buffer.data(), mi.memberSize);

            std::cout << "[recvr::procRdData] dynamic SIZE: " << m_memberInfo[i+1].memberSize << "\n";
          }
          break;
        }
        }
      }
    }

    Deployka::MemberInfo& mi = m_memberInfo.back();

    size_t processedMsgSize = mi.memberOffset + mi.memberSize;
    std::cout << "[recvr::procRdData] Total message size: " << processedMsgSize
              << "; message received " << m_msgReceived << '\n';


    std::vector<unsigned char> tmp;
    //auto startIt = 
    tmp.reserve(Deployka::RECV_BUF_SIZE - processedMsgSize);
    tmp.assign(m_messageBuffer.begin() + processedMsgSize, m_messageBuffer.end());
    //m_consumed += processedMsgSize;
    if (processedMsgSize < m_msgReceived) {
      // cleanup
      cleanupReceiveState();

      receive(tmp.data(), tmp.size());
    }

    std::cout << "[recvr::procRdData] out\n";
  }
};

/******************************************************************************//*
*
*********************************************************************************/
class DeploykaTcpConnection: public boost::enable_shared_from_this<DeploykaTcpConnection> {

  std::array<unsigned char, Deployka::RECV_BUF_SIZE> m_receiveBuffer;

  DeploykaMessageReceiver m_messageReceiver;

  boost::asio::ip::tcp::socket m_sock;

  //================================================================================
  DeploykaTcpConnection(boost::asio::io_context& ctx)
    : m_sock(ctx)
  {
    m_receiveBuffer.fill(0);
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
  void handleRead(boost::system::error_code const& ec, size_t received) {
    std::cout << "[conn::handleRead]=====this: 0x" << this << "==========================\n";
    if (ec) {
      std::cerr << "[conn::handleRead] error: " << ec.what() << " (" << ec.value() << ")\n";
      return;
    }

    m_messageReceiver.receive(m_receiveBuffer.data(), received);

    //m_messageReceiver.done();

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
  std::ostream::sync_with_stdio(false);

#ifdef _TESTS_
  TEST::readAndPop_twoBuffers();
  TEST::getFromOffset();
  TEST::readAndPop_twice();
#else
  boost::asio::io_context context;
  try {
    DeploykaTcpServer server(context);

    std::cout << "[main]                after server creation\n";

    context.run();

    std::cout << "[main]                after context run\n";
  }
  catch (std::exception& e) {
    std::cerr << "exception in main: " << e.what() << std::endl;
  }
#endif

  return 0;
}
