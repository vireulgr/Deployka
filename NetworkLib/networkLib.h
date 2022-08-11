#ifndef __NETWORK_LIB_H__
#define __NETWORK_LIB_H__
#include <map>
#include <vector>
#include <string>
#include <list>
#include <array>
// CLIENT REQUEST
// size_t  | size_t   | array<char, dataSize> |
// command | dataSize | serialized data       |

// SERVER RESPONSE
// size_t  | size_t   | array<char, dataSize> |
// command | dataSize | serialized data       |

namespace Deployka {

  int constexpr TCP_PORT = 27182;
  size_t constexpr RECV_BUF_SIZE = 8196;

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

  std::map<MessageType, std::vector<MemberType>> const g_commands = { 
    {
      DMT_File,
      // cmd code   file data size  file data   file Name Size  file name
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

  void printHex(std::vector<unsigned char>& vec);
  void printString(std::vector<unsigned char>& vec);

  // TODO интерфейс для извлечения частей команды из буфера в MemberInfo
  // TODO function receive member info vector
  // TODO function send member info vector

  /******************************************************************************//*
  *
  *********************************************************************************/
  struct ReceiveBuffer {
    size_t bufOffset; // offset of buffer in message
    size_t bufSize; // buffer size
    std::array<unsigned char, RECV_BUF_SIZE> bufData;

    ReceiveBuffer();

    size_t initialize(unsigned char const* data, size_t dataSize);
    size_t readFromOffsetToEnd(unsigned char* data, size_t offset) const;
    size_t pop(size_t count);
  };

  /******************************************************************************//*
  *
  *********************************************************************************/
  struct ReceiveStream {
    size_t minOffset;
    size_t maxOffset;
    std::list<ReceiveBuffer> buffers;

    ReceiveStream();

    void addBuffer(unsigned char const* data, size_t dataSize, size_t offset = ULLONG_MAX);
    size_t getFromOffset(unsigned char* destBuf, size_t count, size_t offset) const;
    size_t popData(size_t count);
    size_t readAndPop(unsigned char* destBuf, size_t count); // offset is always minOffset
    size_t readAndPop(std::vector<unsigned char>& destBuf, size_t count); // offset is always minOffset

    void resetOffsets();
    bool empty() const;
  };
}
#endif
