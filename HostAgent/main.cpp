#include <iostream>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/array.hpp"

#include "..\common\DeploykaNet.h"

using namespace boost::asio;

/******************************************************************************//*
*
Printer class usage:
int main() {
  io_context context;
  boost::system::error_code ec;

  Printer printer(context);

  printer.printFunc(ec);

  context.run();
}
*********************************************************************************/
class Printer {
  int m_count;
  steady_timer m_timer;

public:
  Printer(io_context & ctx)
    : m_count(0)
    , m_timer(ctx, chrono::seconds(1))
  {}

  ~Printer() {
    std::cout << "\ncount: " << m_count << std::endl;
  }

  void printFunc(boost::system::error_code const & ec) {
    if (ec.value() != 0) {
      std::cout << "error: " << ec.value() << "\n";
    }
    if (m_count < 5) {
      std::cout << this->m_count << " ";
      m_count += 1;
      m_timer.expires_at(this->m_timer.expiry() + chrono::seconds(1));
      m_timer.async_wait(boost::bind(&Printer::printFunc, this, placeholders::error));
    }
  }
};

int main(int argc, char* argv[]) {
  std::ostream::sync_with_stdio(false);

  if (argc != 2) {
    std::cout << "Provide hostname\n";
    return -1;
  }

  io_context context;

  ip::tcp::resolver resolver(context);

  try {
    ip::tcp::resolver::results_type gaiResults = resolver.resolve(argv[1], "daytime");

    ip::tcp::socket sock(context);

    connect(sock, gaiResults);

    for (;;) {
      boost::array<char, 128> buf;
      boost::system::error_code ec;

      size_t len = sock.read_some(buffer(buf), ec);

      if (ec == boost::asio::error::eof) {
        break;
      }
      else if (ec) {
        throw boost::system::system_error(ec);
      }

      std::cout.write(buf.data(), len);

    }
  }
  catch (std::exception & e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
