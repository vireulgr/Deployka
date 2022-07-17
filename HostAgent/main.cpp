#include <iostream>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
//#include "boost/asio/impl/src.hpp"

using namespace boost::asio;

int main() {
  std::cout << "Hello from Host Agent!" << std::endl;
  io_context context;
  ip::tcp::endpoint ep(ip::make_address_v4("127.0.0.1"), 2001);
  ip::tcp::socket sock(context);
  sock.connect(ep);

  //sock.send(


  return 0;
}
