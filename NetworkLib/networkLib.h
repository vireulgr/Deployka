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

  struct FileMessage {
    std::string name;
    long long totalSize;
    long long offset;
    std::vector<unsigned char> data;
  };

  enum MessageType : size_t {
    DMT_Null,
    DMT_File,
    DMT_FileChunked,
    DMT_MessageTypeMax
  };

  struct Message {
    MessageType m_type;
    int m_size;
    std::vector<unsigned char> m_data;
  };
  // TODO интерфейс для извлечения частей команды из буфера в MemberInfo
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
      //        command code            file data size            file data            file Name Size              file name
      {Deployka::MT_longLong, Deployka::MT_dynamicSize, Deployka::MT_dynamic, Deployka::MT_dynamicSize, Deployka::MT_dynamic}
    }
    // TODO
  };

  std::vector<MemberInfo> buildMemberInfo(std::vector<MemberType> const mt);

  // TODO function receive member info vector
  // TODO function send member info vector

}
#endif