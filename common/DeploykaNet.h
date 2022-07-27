#ifndef __MY_NET_H__
#define __MY_NET_H__

namespace Deployka {

  int constexpr TCP_PORT = 27182;

  struct FileMessage {
    std::string name;
    long long totalSize;
    long long offset;
    std::vector<unsigned char> data;
  };

  enum MessageType {
    DMT_Null,
    DMT_File,
    DMT_MessageTypeMax
  };

  struct Message {
    int m_size;
    MessageType m_type;
    std::vector<unsigned char> m_data;
  };
}

#endif