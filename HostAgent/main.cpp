#include <iostream>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include "boost/array.hpp"

#include "..\common\DeploykaNet.h"

using namespace boost::asio;

void sendString(boost::asio::ip::tcp::socket& sock, std::string aStr) {
  std::cout << "sending \"" << aStr << "\"\n";
  for (;;) {
    boost::system::error_code ec;

    size_t written = sock.write_some(buffer(aStr), ec);

    if (written == 0) {
      std::cout 
        << "ERROR write_some returned 0! ec val " << ec.value()
        << " ec what: " << ec.what()
        << std::endl;
      break;
    }

    std::cout << "Written " << written << std::endl;

    if (ec.value() != 0) {
      std::cout << "ERROR " << ec.what() << " (" << ec.value() << ')' << std::endl;
      break;
    }

    if (written >= aStr.size()) {
      break;
    }
  }
}
template<typename T>
void sendIntegralOrEnum(boost::asio::ip::tcp::socket& sock, T value) {
  static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "Type is not suitable");

  std::cout << "sending \"" << value << "\"";
  for (;;) {
    boost::system::error_code ec;

    struct intPod { T whatever; } buf[1];

    buf[0].whatever = value;

    size_t written = sock.write_some(buffer(buf), ec);

    if (written == 0) {
      std::cout << "ERROR write_some returned 0! ec val " << ec.value() << " ec what: " << ec.what() << std::endl;
      break;
    }

    std::cout << " ok, written " << written << std::endl;

    if (ec.value() != 0) {
      std::cout << "ERROR " << ec.what() << " (" << ec.value() << ')' << std::endl;
      break;
    }
    
    if (written >= sizeof(T)) {
      break;
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

    std::cout << "enter q to quit; l to sending ints; j to send strings; t to send custom data set\n";

    for (;;) {

      char c;
      std::cin >> c;
      if (c == 'q') { break; }
      if (c == 'l') {
        std::cout << "send ints\n";
        sendIntegralOrEnum(sock, 1337);

        sendIntegralOrEnum(sock, 42);
      }
      if (c == 'j') {

        std::cout << "send strings\n";
        sendString(sock, "char str 1  of");
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        sendString(sock, " sz 20char");
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        sendString(sock, " str 2  of sz 20char str 3  of sz 20");
      }
      if (c == 't') {

        sendIntegralOrEnum<long long>(sock, 1337ll);

        std::this_thread::sleep_for(std::chrono::milliseconds(400));

        sendIntegralOrEnum<long long>(sock, 42ll);

        std::this_thread::sleep_for(std::chrono::milliseconds(400));

        sendIntegralOrEnum<size_t>(sock, 20ull);
        sendString(sock, "char str 3  of sz 20");

        sendIntegralOrEnum<size_t>(sock, 22ull);
        sendString(sock, "char string of size 22");

        sendIntegralOrEnum<long double>(sock, 3.14159265358979);
      }
    }
  }
  catch (std::exception & e) {
    std::cerr << "Exception in main: " << e.what() << std::endl;
  }

  return 0;
}
