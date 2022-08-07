#include <iostream>
#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include <fstream>
#include <streambuf>
//#include "boost/asio/impl/src.hpp"

#include "networkLib.h"

using namespace boost::asio;

// ================================================================================
void sendBuffer(boost::asio::ip::tcp::socket &sock, boost::asio::const_buffer & bf) {

  for (;;) {
    boost::system::error_code ec;

    size_t written = sock.write_some(bf, ec);

    std::cout << "write_some returned " << written << '\n';

    if (ec.value() != 0) {
      std::cout << "ERROR " << ec.what() << " (" << ec.value() << ')' << std::endl;
      break;
    }

    if (written >= bf.size()) {
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}


// ================================================================================
void sendCommand(boost::asio::ip::tcp::socket & sock, std::vector<Deployka::MemberInfo> & mi) {
  for (Deployka::MemberInfo& item : mi) {
    boost::asio::const_buffer aConstBuffer = buffer(item.buffer);
    sendBuffer(sock, aConstBuffer);
  }
}


// ================================================================================
template<typename T>
void sendArithmeticOrEnum(boost::asio::ip::tcp::socket& sock, T value) {
  static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "Type is not suitable");

  std::cout << "sending something \"" << value << "\"";

  struct intPod { T whatever; } tmpStruct[1];

  tmpStruct[0].whatever = value;

  boost::asio::const_buffer aConstBuf = buffer(tmpStruct);

  sendBuffer(sock, aConstBuf);
}


// ================================================================================
void sendFileCommand(boost::asio::ip::tcp::socket& sock, std::string filename) {
  long long int longLongBuf = Deployka::DMT_File;
  std::vector<Deployka::MemberInfo> memInfo = Deployka::buildMemberInfo(Deployka::g_commands.at((Deployka::MessageType)longLongBuf));

  // member #1
  char const* cmdPtr = (char*)&longLongBuf;
  std::copy(cmdPtr, cmdPtr + memInfo[0].memberSize, std::back_inserter(memInfo[0].buffer));

  std::ifstream ifs;
  ifs.open(filename, std::ios_base::binary | std::ios_base::ate);
  if (!ifs.is_open()) {
    throw std::logic_error("file is not open");
  }
  /*size_t */longLongBuf = ifs.tellg();
  ifs.seekg(0, std::ios_base::beg);

  // member #2
  /*char const* */cmdPtr = (char*)&longLongBuf;
  std::copy(cmdPtr, cmdPtr + memInfo[1].memberSize, std::back_inserter(memInfo[1].buffer));

  // member #3
  memInfo[2].buffer.reserve(longLongBuf);

  std::copy(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>(), std::back_inserter(memInfo[2].buffer));

  // member #4
  /*size_t*/ longLongBuf = filename.size();
  /*char const**/ cmdPtr = (char*)&longLongBuf;
  std::copy(cmdPtr, cmdPtr + memInfo[3].memberSize, std::back_inserter(memInfo[3].buffer));

  // member #5
  std::copy(filename.begin(), filename.end(), std::back_inserter(memInfo[4].buffer));

  sendCommand(sock, memInfo);
}
 

// ================================================================================
void sendString(boost::asio::ip::tcp::socket& sock, std::string aStr) {
  std::cout << "sending string \"" << aStr << "\"... ";
  boost::asio::const_buffer aConstBuf = buffer(aStr);
  sendBuffer(sock, aConstBuf);
}


//================================================================================
int main(int argc, char* argv[]) {
  std::ostream::sync_with_stdio(false);
  system("cd ");

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


    for (;;) {

      std::cout << "Enter q to quit; l to sending ints; j to send strings; t to send custom data set\n";
      char c;
      std::cin >> c;
      if (c == 'q') { break; }
      if (c == 'l') {
        std::cout << "send ints\n";
        sendArithmeticOrEnum(sock, 1337);
        sendArithmeticOrEnum(sock, 42);
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

        sendArithmeticOrEnum<long long>(sock, 1337ll);

        std::this_thread::sleep_for(std::chrono::milliseconds(400));

        sendArithmeticOrEnum<long long>(sock, 42ll);

        std::this_thread::sleep_for(std::chrono::milliseconds(400));

        sendArithmeticOrEnum<size_t>(sock, 20ull);
        sendString(sock, "char str 3  of sz 20");

        sendArithmeticOrEnum<size_t>(sock, 22ull);
        sendString(sock, "char string of size 22");

        sendArithmeticOrEnum<long double>(sock, 3.14159265358979);
      }
      if (c == 'g') {
        sendFileCommand(sock, "CMakeLists.txt");
      }
    }
  }
  catch (std::exception & e) {
    std::cerr << "Exception in main: " << e.what() << std::endl;
  }

  return 0;
}
