#ifndef __RECEIVE_STREAM_H__
#define __RECEIVE_STREAM_H__
#include <list>
#include <array>
#include <vector>
#include "networkLib.h"

namespace Deployka {
/******************************************************************************//*
*
*********************************************************************************/
struct ReceiveBuffer {
protected:
  size_t dataOffset;
public:
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
} // namespace Deployka
#endif
