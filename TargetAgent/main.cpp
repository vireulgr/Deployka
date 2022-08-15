#include <iostream>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/enable_shared_from_this.hpp"
#include "boost/shared_ptr.hpp"

#include "..\common\DeploykaNet.h"
//#include "boost/asio/impl/src.hpp"

namespace Deployka {
  enum MemberType {
    MT_notype = 0,
    MT_dynamic = 0,
    MT_stringSize,
    MT_bool,
    MT_longLong,
    MT_longDouble
  };

  struct MemberInfo {
    MemberType memberType;
    bool done;
    size_t memberOffset;
    size_t memberSize;
    std::vector<unsigned char> buffer;
  };

  constexpr size_t g_memberTypeSizes[] = {0, sizeof(size_t), sizeof(bool), sizeof(long long), sizeof(long double)};

  std::vector<MemberInfo> buildMemberInfo(std::vector<MemberType> mt) {
    std::vector<MemberInfo> result;
    size_t memberOffset = 0;
    for (auto const& item : mt) {
      MemberInfo mi;
      mi.memberType = item;
      mi.memberOffset = memberOffset;
      mi.memberSize = g_memberTypeSizes[item];
      mi.done = false;

      if (mi.memberSize > 0) {
        mi.buffer.reserve(mi.memberSize);
      }

      memberOffset += mi.memberSize;
      result.push_back(mi);
    }
    return result;
  }
}


class DeploykaTcpConnection: public boost::enable_shared_from_this<DeploykaTcpConnection> {

  std::vector<Deployka::MemberType> m_memberInfoDeclare;
  std::vector<Deployka::MemberInfo> m_memberInfo;

  std::array<unsigned char, 8196> m_receiveBuffer;
  std::array<unsigned char, 8196> m_messageBuffer;

  size_t m_msgReceived;

  boost::asio::ip::tcp::socket m_sock;

  DeploykaTcpConnection(boost::asio::io_context& ctx)
    : m_sock(ctx)
    , m_msgReceived(0)
  {
    m_receiveBuffer.fill(0);
    m_messageBuffer.fill(0);

    m_memberInfoDeclare.push_back(Deployka::MT_longLong);
    m_memberInfoDeclare.push_back(Deployka::MT_longLong);
    m_memberInfoDeclare.push_back(Deployka::MT_stringSize);
    m_memberInfoDeclare.push_back(Deployka::MT_dynamic);
    m_memberInfoDeclare.push_back(Deployka::MT_stringSize);
    m_memberInfoDeclare.push_back(Deployka::MT_dynamic);
    m_memberInfoDeclare.push_back(Deployka::MT_longDouble);

    m_memberInfo = Deployka::buildMemberInfo(m_memberInfoDeclare);
  }

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

    m_sock.async_receive(boost::asio::buffer(m_receiveBuffer),
      boost::bind(&DeploykaTcpConnection::handleRead,
        shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
  }

  void handleRead(boost::system::error_code const & ec, size_t received) {
    std::cout << "[conn::handleRead]=====this: 0x" << this << "========\n";
    if (ec) {
      std::cerr << "[conn::handleRead] error:" << ec.what() << " (" << ec.value() << ")\n";
      return;
    }

    memcpy(m_messageBuffer.data() + m_msgReceived, m_receiveBuffer.data(), received);
    m_msgReceived += received;

    std::cout << "received: " << received << "; msgReceived: " << m_msgReceived << '\n';

    size_t dynamicOffset = 0;
    for (int i = 0; i < m_memberInfo.size(); ++i) {
      Deployka::MemberInfo& mi = m_memberInfo[i];
      std::cout << "offs: " << mi.memberOffset << "; size: " << mi.memberOffset << '\n';
      if (m_msgReceived >= mi.memberOffset + mi.memberSize + dynamicOffset
        && !mi.done)
      {
        std::cout
          << "mo: " << mi.memberOffset
          << "; ms: " << mi.memberSize
          << "; dynoffset: " << dynamicOffset << '\n';

        mi.buffer.reserve(mi.memberSize);
        memcpy(mi.buffer.data(), m_messageBuffer.data() + mi.memberOffset + dynamicOffset, mi.memberSize);

        mi.done = true;

        switch (mi.memberType) {
        case Deployka::MT_longDouble: {
          long double val = 0;
          memcpy(&val, mi.buffer.data(), mi.memberSize);
          std::cout << "long double val: " << val << '\n';
          break;
        }
        case Deployka::MT_longLong: {
          long long val = 0;
          memcpy(&val, mi.buffer.data(), mi.memberSize);
          std::cout << "long long val: " << val << '\n';
          break;
        }
        case Deployka::MT_dynamic: {
          std::string val;
          val.reserve(mi.buffer.size());
          for (size_t j = 0; j < mi.memberSize; j++) {
            val.push_back(*(mi.buffer.data() + j));
          }
          std::cout << "sz: " << mi.buffer.size() << " dynamic: \"" << val << "\"\n";

          dynamicOffset += mi.memberSize;
          break;
        }
        case Deployka::MT_stringSize: {
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

    m_sock.async_receive(boost::asio::buffer(m_receiveBuffer),
      boost::bind(&DeploykaTcpConnection::handleRead,
        shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
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
