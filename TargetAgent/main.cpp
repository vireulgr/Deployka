#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/shared_ptr.hpp"

#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <climits>

#include "networkLib.h"
#include "MessageReceiver.h"

#ifdef _MSC_VER
//#define WIN32_LEAN_AND_MEAN
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#else
#include "boost/filesystem.hpp"
#endif

//#define _TESTS_

#ifdef _TESTS_
#include "tests.h"
#endif
//#include "boost/asio/impl/src.hpp"

void processFileChunkMessage(std::vector<Deployka::MemberInfo>& message) {

  if (message.empty()) {
    std::cout << "[procFileChunkMsg] ERROR message is empty!\n";
    return;
  }

  Deployka::MemberInfo& mi = message.front();
  long long int val = 0;
  memcpy(&val, mi.buffer.data(), mi.memberSize);
  if (val != Deployka::DMT_FileChunk) {
    std::cout << "[procFileChunkMsg] ERROR message is not file chunk message!\n";
    return;
  }

  std::string receivedFilePath;
  receivedFilePath.reserve(message[2].memberSize);
  std::copy(message[2].buffer.begin(), message[2].buffer.end(), std::back_inserter(receivedFilePath));

  std::string directoryToPutFiles = "./uploads";
  //std::cout << "[procFileChunkMsg] filename: " << receivedFilePath << '\n';

#ifdef _MSC_VER
  if (!PathIsDirectory(directoryToPutFiles.c_str())) {
    //std::cout << "directory " << directoryToPutFiles << " not exists\n";
    if (!CreateDirectory(directoryToPutFiles.c_str(), NULL)) {
      std::cout << "ERROR! Cannot create directory! GLE" << GetLastError() << '\n';
    }
  }
#else
  // TODO
  boost::system::error_code ec;
  if (!exists(directoryToPutFiles)) {
    if (!create_directory(directoryToPutFiles, ec)) {
      throw std:exception(ec);
    }
  }
    is_directory(directoryToPutFiles)) {}
#endif

  size_t pos = receivedFilePath.find_last_of("/\\");
  std::string fileName = directoryToPutFiles + '/' + receivedFilePath.substr((pos == std::string::npos) ? 0 : (pos + 1));

  std::ios::openmode appendOrTruncate = std::ios::app;

  //memcpy(&val, message[6].buffer.data(), std::min(sizeof(val), message[6].buffer.size()));
  if (std::all_of(message[6].buffer.cbegin(), message[6].buffer.cend(), [](unsigned char const c) { return c == '\0'; })) {
    // chunk # is zero
    appendOrTruncate = std::ios::trunc;
  }

  std::ofstream ofs;
  ofs.open(fileName, std::ios::binary | std::ios::out | appendOrTruncate);

  if (ofs.is_open()) {
    std::cout << "[procFileChunkMsg] write " << message[8].buffer.size() << " bytes to file " << fileName << "\n";
    ofs.write(reinterpret_cast<char*>(message[8].buffer.data()), message[8].buffer.size());
    if (ofs.bad()) {
      std::cout << "[procFileChunkMsg] ERROR bad bit on file is set!\n";
    }
  }
  else {
    if (ofs.bad()) {
      std::cout << "[procFileChunkMsg] ERROR bad bit on file is set!\n";
    }
    std::cout << "[procFileChunkMsg] ERROR file open failed!\n";
    std::cout << fileName << '\n';
  }
  ofs.flush();
}


/******************************************************************************//*
*
*********************************************************************************/
class DeploykaTcpConnection: public boost::enable_shared_from_this<DeploykaTcpConnection> {

  std::array<unsigned char, Deployka::RECV_BUF_SIZE> m_receiveBuffer;

  Deployka::MessageReceiver m_messageReceiver;

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

    //std::cout << "[conn::start]          this: 0x" << this << '\n';

    m_sock.async_receive(boost::asio::buffer(m_receiveBuffer),
      boost::bind(&DeploykaTcpConnection::handleRead,
        shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
  }

  //================================================================================
  void processMessages(std::vector<std::vector<Deployka::MemberInfo>>& messages) {
    if (messages.empty()) {
      std::cout << "[processMessage] ERROR messages are empty!\n";
      return;
    }
    for (int i = 0; i < messages.size(); i++) {
    if (messages[i].empty()) {
      std::cout << "[processMessage] ERROR messages are empty!\n";
      return;
    }
    Deployka::MemberInfo& mi = messages[i].front();
    long long int val = 0;
    memcpy(&val, mi.buffer.data(), mi.memberSize);
    if (val >= Deployka::DMT_MessageTypeMax || val < Deployka::DMT_Null) {
      std::cout << "[processMessage] ERROR message type is incorrect!\n";
      return;
    }

    switch (val) {
    case Deployka::DMT_FileChunk:
      ::processFileChunkMessage(messages[i]);
      break;
    }
    }
  }

  //================================================================================
  void handleRead(boost::system::error_code const& ec, size_t received) {
    //std::cout << "[conn::handleRead]=====this: 0x" << this << "==========================\n";
    if (ec) {
      std::cerr << "[conn::handleRead] ERROR: " << ec.what() << " (" << ec.value() << ")\n";
      return;
    }

    m_messageReceiver.receive(m_receiveBuffer.data(), received);

    if (m_messageReceiver.done()) {
      std::vector<std::vector<Deployka::MemberInfo>> messages = m_messageReceiver.getReceivedMessages();
      //std::cout << "[conn::processMsg] msgSize: " << message.size() << '\n';
      processMessages(messages);
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
    //std::cout << "[serv::handleAccept] con ptr 0x" << newConnection.get() << '\n';
    if (!ec) {
      newConnection->start();
    }
    startAccept();
  }

  void startAccept() {
    DeploykaTcpConnection::pointer newConnection = DeploykaTcpConnection::create(m_ctx);
    //std::cout << "[serv::startAccept] con ptr: 0x" << newConnection.get() << '\n';

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
  TEST::readManyBytes();
  TEST::multipleMessagesInStream();
  TEST::multiplePartsMessage();

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
