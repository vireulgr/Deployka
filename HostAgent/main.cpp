#define BOOST_ASIO_NO_DEPRECATED
#define BOOST_ASIO_NO_TS_EXECUTORS
#include "boost/asio.hpp"
#include "boost/bind.hpp"
#include <fstream>
#include <iostream>
#include <streambuf>
#include <memory>
#include <iomanip>
#include <algorithm>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <filesystem>
#endif
//#include "boost/asio/impl/src.hpp"

#include "networkLib.h"

using namespace boost::asio;

inline void clearMemberInfo(std::vector<Deployka::MemberInfo>& miVec) {
  std::transform(miVec.begin(), miVec.end(), miVec.begin(), [](Deployka::MemberInfo& item) {
    item.done = false;
    item.buffer.clear();
    return item;
  });

}

template <typename T>
inline void serializeToMember(Deployka::MemberInfo& mi, T val) {
  static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "Type is not suitable");

  //std::cout << "stm " << val << '\n';
  char const* dataPtr = (char*)&val;
  std::copy(dataPtr, dataPtr + mi.memberSize, std::back_inserter(mi.buffer));
}

template <>
inline void serializeToMember(Deployka::MemberInfo& mi, std::string & val) {
  //std::cout << "stm " << val << '\n';
  std::copy(val.begin(), val.end(), std::back_inserter(mi.buffer));
}

inline void serializeToMember(Deployka::MemberInfo& mi, unsigned char * buf, unsigned long int bufSize) {
  //std::cout << "stm (" << bufSize << ") ";
  //std::cout.write(reinterpret_cast<char*>(buf), bufSize);
  //std::cout << '\n';
  std::copy(buf, buf + bufSize, std::back_inserter(mi.buffer));
}

// ================================================================================
boost::system::error_code sendBuffer(ip::tcp::socket &sock, const_buffer & bf) {

  for (;;) {
    boost::system::error_code ec;

    size_t written = sock.write_some(bf, ec);

    if (ec.value() != 0) {
      std::cout << "[sendBuffer] ERROR " << ec.what() << " (" << ec.value() << ')' << std::endl;
      return ec;
    }

    if (written >= bf.size()) {
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return boost::system::error_code();
}


// ================================================================================
boost::system::error_code sendMessage(ip::tcp::socket & sock, std::vector<Deployka::MemberInfo> & mi) {
  std::cout << "      sent |    total\n";
  size_t totalSent = 0;
  for (Deployka::MemberInfo& item : mi) {
    const_buffer aConstBuffer = buffer(item.buffer);
    boost::system::error_code ec = sendBuffer(sock, aConstBuffer);
    if (ec.value() != 0) {
      return ec;
    }
    totalSent += aConstBuffer.size();
    std::cout << std::setw(9) << aConstBuffer.size() << std::setw(9) << totalSent << '\n';
  }
  return boost::system::error_code();
}


// ================================================================================
template <typename T>
void sendArithmeticOrEnum(ip::tcp::socket& sock, T value) {
  static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "Type is not suitable");

  std::cout << "sending something \"" << value << "\"";

  struct TempPodStruct { T whatever; } tmpStruct[1] = { {value} };

  const_buffer aConstBuf = buffer(tmpStruct);

  sendBuffer(sock, aConstBuf);
}


// ================================================================================
void sendFileCommand(ip::tcp::socket& sock, std::string filename) {
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

  sendMessage(sock, memInfo);
}
 

// file chunk:
// |  long long         | dynamic size     | dynamic   |   dynamic size            | dynamic (ISO date string) | long long | long long    | dynamic size | dynamic         |
// | file chunk command | file name length | file name | file last mod time length | file last mod time        | file size | chunk number | chunk length | chunk file data |
void sendFileChunkedCommand(ip::tcp::socket& sock, std::string filename) {

  size_t const fileChunkCommand = Deployka::DMT_FileChunk;
  std::vector<Deployka::MemberInfo> memInfoVec = Deployka::buildMemberInfo(Deployka::g_commands.at((Deployka::MessageType)fileChunkCommand));

  std::string fileLastModTimeIsoStr;
  long long unsigned int fileSize = 0ull;
  long long unsigned int chunkSize = (1ull << 13) - 256; // 

#ifdef _MSC_VER
  size_t converted;
  std::unique_ptr<wchar_t[]> wcBuf = std::make_unique<wchar_t[]>(MAX_PATH);
  /*errno_t cvtErr = */mbstowcs_s(&converted, wcBuf.get(), MAX_PATH, filename.c_str(), _TRUNCATE);
  if (converted < filename.size()) {
    std::cout << "Converted " << converted << " chars out of " << filename.size() << "!\n";
    throw std::logic_error("Cannot convert file name to wide char string");
  }

  //std::wcout << L"Converted file name: " << wcBuf.get() << L'\n';
  //std::cout << "errno: " << cvtErr << "; T?: " << (cvtErr == STRUNCATE) << "; converted: " << converted << "; filename size: " << filename.size() << '\n';

  HANDLE hFile = CreateFileW(wcBuf.get(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, /*FILE_FLAG_RANDOM_ACCESS FILE_FLAG_SEQUENTIAL_SCAN*/ 0, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    std::cout << "Error! GLE: " << GetLastError() << std::endl;
    throw std::logic_error("Cannot open file");
  }

  LARGE_INTEGER liFileSize{};
  if (!GetFileSizeEx(hFile, &liFileSize)) {
    std::cout << "Error! GLE: " << GetLastError() << std::endl;
    CloseHandle(hFile);
    throw std::logic_error("Cannot get file size");
  }

  FILETIME ftAccess{}, ftCreate{}, ftWrite{};
  if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite)) {
    std::cout << "Error! GLE: " << GetLastError() << std::endl;
    CloseHandle(hFile);
    throw std::logic_error("Cannot get file time");
  }

  SYSTEMTIME stTemp{};
  if (!FileTimeToSystemTime(&ftWrite, &stTemp)) {
    std::cout << "Error! GLE: " << GetLastError() << std::endl;
    CloseHandle(hFile);
    throw std::logic_error("Cannot convert file time to system time");
  }

  std::ostringstream tmpOss;
  tmpOss << std::setfill('0');
  tmpOss << stTemp.wYear << "-" << std::setw(2) << stTemp.wMonth << "-" <<
    std::setw(2) << stTemp.wDay << "T" << std::setw(2) << stTemp.wHour << ":" <<
    std::setw(2) << stTemp.wMinute << ":" << std::setw(2) << stTemp.wSecond;

  fileLastModTimeIsoStr = tmpOss.str();
  fileSize = liFileSize.QuadPart;
#else
  namespace fs = std::filesystem;
  fs::last_write_time(filename);
#endif

  std::unique_ptr<unsigned char[]> buffer = std::make_unique<unsigned char[]>(chunkSize);
  for (int i = 0; ; i++) {
    // read file chunk
    unsigned long int bytesRead = 0;
#ifdef _MSC_VER
    if (!ReadFile(hFile, buffer.get(), static_cast<unsigned long>(chunkSize), &bytesRead, nullptr)) {
      CloseHandle(hFile);
      std::cout << "Error! GLE: " << GetLastError() << "; chunk size: " << chunkSize << "; bytes read: " << bytesRead << std::endl;
      throw std::logic_error("Error reading from file!");
    }
#else
    std::ifstream ifs;
    ifs.open(filename, std::ios_base::binary | std::ios_base::ate);
    if (!ifs.is_open()) {
      std::cout << "Error! File path is: " << filename << std::endl;
      throw std::logic_error("file is not open");
    }
    /*size_t */longLongBuf = ifs.tellg();
    ifs.seekg(0, std::ios_base::beg);
#endif
    if (bytesRead == 0) {
      break;
    }


    // member #0 file chunk command
    serializeToMember(memInfoVec[0], fileChunkCommand);
    // member #1 file name length
    serializeToMember(memInfoVec[1], filename.size());
    // member #2 file name
    serializeToMember<std::string&>(memInfoVec[2], filename);
    // member #3 file last mod time length
    serializeToMember(memInfoVec[3], fileLastModTimeIsoStr.size());
    // member #4 file last mod time
    serializeToMember<std::string&>(memInfoVec[4], fileLastModTimeIsoStr);
    // member #5 file size
    serializeToMember(memInfoVec[5], fileSize);
    // member #6 file chunk number
    serializeToMember(memInfoVec[6], i);
    // member #7 file chunk length
    serializeToMember(memInfoVec[7], bytesRead);
    // member #8 file chunk
    serializeToMember(memInfoVec[8], buffer.get(), bytesRead);

    //Deployka::printString(memInfoVec[8].buffer);

    boost::system::error_code ec = sendMessage(sock, memInfoVec);
    if (ec.value() != 0) {
      break;
    }

    clearMemberInfo(memInfoVec);

    // DEBUG
    //std::this_thread::sleep_for(std::chrono::milliseconds(800));

    //if (bytesRead > 0) {
    //  break;
    //}
    // /DEBUG
  }

#ifdef _MSC_VER
  CloseHandle(hFile);
#else
#endif
}


// ================================================================================
void sendString(ip::tcp::socket& sock, std::string aStr) {
  std::cout << "sending string \"" << aStr << "\"... ";
  const_buffer aConstBuf = buffer(aStr);
  sendBuffer(sock, aConstBuf);
}


//================================================================================
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

    for (char c = '\0'; c != 'q'; std::cin >> c) {
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
        char const fileName[] = "CMakeLists.txt";
        sendFileCommand(sock, fileName);
      }
      if (c == 'h') {
        //char const fileName[] = "E:\\prog\\cpp\\Deployka\\build\\TargetAgent\\Debug\\TargetAgent.pdb";
        char const fileName[] = "E:\\prog\\cpp\\Deployka\\somefile.txt";
        sendFileChunkedCommand(sock, fileName);
      }

      std::cout << "Enter q to quit; h to send chukned file; g to send small file\n";
    }
  }
  catch (std::exception & e) {
    std::cerr << "Exception in main: " << e.what() << std::endl;
  }

  return 0;
}
