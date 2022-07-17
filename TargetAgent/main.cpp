#include <iostream>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/thread.hpp"
#include "boost/asio.hpp"
#include "boost/bind.hpp"
//#include "boost/asio/impl/src.hpp"


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
