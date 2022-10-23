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
  size_t dataOffset; // index in array where data is actually begins
  std::array<unsigned char, RECV_BUF_SIZE> bufData;
public:
  size_t bufOffset; // offset of buffer in message (stream)
  size_t bufSize; // buffer size

  ReceiveBuffer();

  size_t initialize(unsigned char const* data, size_t dataSize); // put data to beginning of receive buffer
  size_t readFromOffsetToEnd(unsigned char* data, size_t offset) const;
  size_t readCountFromOffset(unsigned char* data, size_t offset, size_t count) const;
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

  size_t getSize() const;

  void resetOffsets();
  bool empty() const;
};
} // namespace Deployka
#endif
