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

void sendInt(boost::asio::ip::tcp::socket& sock, int value) {
  std::cout << "sending " << value << '\n';
  for (;;) {
    boost::system::error_code ec;

    //boost::array<char, 128> buf;
    //::memcpy(buf.data(), &numberNetwork, sizeof(int));

    struct intPod { int whatever; } buf[1];

    buf[0].whatever = value;

    size_t written = sock.write_some(buffer(buf), ec);

    if (written == 0) {
      std::cout << "ERROR write_some returned 0" << std::endl;
    std::cout << "ec val " << ec.value() << " ec what: " << ec.what() << std::endl;
      break;
    }

    std::cout << "Written " << written << std::endl;
    std::cout << "ec val " << ec.value() << std::endl;

    if (ec.value() == 0 || written >= sizeof(int)) {
      std::cout << "no error" << std::endl;
      if (written >= sizeof(int)) {
        break;
      }
    }
    else {
      std::cout << "ERROR " << ec.what() << " (" << ec.value() << ')' << std::endl;
    }
  }
}

int main(int argc, char* argv[]) {
  std::ostream::sync_with_stdio(false);

  if (argc != 2) {
    std::cout << "Provide hostname\n";
    return -1;
  }

  io_context context;

  ip::tcp::resolver resolver(context);

  std::string portStr = std::to_string(Deployka::TCP_PORT);

  try {
    ip::tcp::resolver::results_type gaiResults = resolver.resolve(argv[1], portStr);

    ip::tcp::socket sock(context);

    connect(sock, gaiResults);

    //for (; false;) {
    //  boost::array<char, 128> buf;
    //  boost::system::error_code ec;

    //  size_t len = sock.read_some(buffer(buf), ec);

    //  if (ec == boost::asio::error::eof) {
    //    break;
    //  }
    //  else if (ec) {
    //    throw boost::system::system_error(ec);
    //  }

    //  std::cout.write(buf.data(), len);
    //}

    for (;;) {

      char c;
      std::cin >> c;
      std::cout << "got: " << c << "(" << (int)c << ")\n";
      if (c == 'q') { break; }
      if (c == 'l') {
        sendInt(sock, 1337);

        sendInt(sock, 42);
      }
    }

    //sock.close() ???
  }
  catch (std::exception & e) {
    std::cerr << "Exception in main: " << e.what() << std::endl;
  }

  return 0;
}
