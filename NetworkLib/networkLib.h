#ifndef __NETWORK_LIB_H__
#define __NETWORK_LIB_H__
#include <map>
#include <vector>
#include <string>
// CLIENT REQUEST
// size_t  | size_t   | array<char, dataSize> |
// command | dataSize | serialized data       |

// SERVER RESPONSE
// size_t  | size_t   | array<char, dataSize> |
// command | dataSize | serialized data       |

namespace Deployka {

  int constexpr TCP_PORT = 27182;
  size_t constexpr RECV_BUF_SIZE = 8192;

  struct FileMessage {
    std::string name;
    long long totalSize;
    long long offset;
    std::vector<unsigned char> data;
  };

  enum MessageType : size_t {
    DMT_Null = 0,
    DMT_File,
    DMT_FileChunk,
    DMT_MessageTypeMax
  };

  struct Message {
    MessageType m_type;
    int m_size;
    std::vector<unsigned char> m_data;
  };

  enum MemberType : size_t {
    MT_notype = 0,
    MT_dynamic = 0,
    MT_dynamicSize,
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

  std::map<MessageType, std::vector<MemberType>> const g_messageTypes = { 
    {
      DMT_File,
      // cmd code   file name size  file name   file data Size  file data
      {MT_longLong, MT_dynamicSize, MT_dynamic, MT_dynamicSize, MT_dynamic}
    },
    {
      DMT_FileChunk,
      // | file chunk command | file name length | file name | file last mod time length |
      {MT_longLong,              MT_dynamicSize,   MT_dynamic,  MT_dynamicSize,
      // file last mod time (ISD date string) | file size | chunk number | chunk length | chunk file data |
      MT_dynamic,                              MT_longLong, MT_longLong,  MT_dynamicSize, MT_dynamic}
    }
    // TODO
  };

  std::vector<MemberInfo> buildMemberInfo(MessageType mt2);
  void clearMemberInfo(std::vector<MemberInfo> & miVec);

  void printHexRange(unsigned char const * buf, size_t start, size_t count);
  void printHexRange(std::vector<unsigned char> const & vec, size_t start, size_t count);

  void printHex(std::vector<unsigned char>& vec);
  void printHex(unsigned char const* buf, size_t bufSize);
  void printString(std::vector<unsigned char>& vec);

  // TODO интерфейс для извлечения частей команды из буфера в MemberInfo
  // TODO function receive member info vector
  // TODO function send member info vector

}
#endif
