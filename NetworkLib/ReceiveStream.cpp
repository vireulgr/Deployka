#include "ReceiveStream.h"
#include <algorithm>
#include <array>
#include <memory>

namespace Deployka {
  /******************************************************************************//*
  * ReceiveBuffer:
  *********************************************************************************/
  ReceiveBuffer::ReceiveBuffer()
    : bufOffset(0)
    , bufSize(0)
    , dataOffset(0)
  {
    bufData.fill(0);
  }

  size_t ReceiveBuffer::initialize(unsigned char const* data, size_t dataSize) {
    size_t writeCount = std::min(dataSize, RECV_BUF_SIZE);
    auto it = std::copy(data, data + writeCount, bufData.data());
    auto dist = std::distance(bufData.data(), it);
    //assert(static_cast<size_t>(dist) == writeCount);
    bufSize = writeCount;
    return dist;
  }

  size_t ReceiveBuffer::readFromOffsetToEnd(unsigned char* data, size_t offset) const {
    unsigned char * resPtr = std::copy(bufData.begin() + offset + dataOffset, bufData.end(), data);
    return resPtr - data;
  }

  size_t ReceiveBuffer::pop(size_t count) {
    //std::cout << "[recvBuffer]::pop count: " << count << "; bufSize: " << bufSize << "; off: " << bufOffset << '\n';
    if (count > bufSize) {
      size_t tmp = bufSize;
      bufOffset = bufOffset + bufSize;
      dataOffset += bufSize;
      bufSize = 0;
      return tmp;
    }

    bufOffset += count;
    bufSize -= count;

    dataOffset += count;
    return count;
  }
  /******************************************************************************//*
  * ReceiveStream
  *********************************************************************************/

  ReceiveStream::ReceiveStream()
    : minOffset(0)
    , maxOffset(0)
  {}
  
  bool ReceiveStream::empty() const {
    //std::cout << "[recvStream::empty] min off: " << minOffset << "; max off: " << maxOffset << '\n';
    //if (!buffers.empty()) {

    //  ReceiveBuffer const & rbb = buffers.front();
    //  std::cout << "[recvStream::empty] 1st buf offs " << rbb.bufOffset << "; 1st buf size: " << rbb.bufSize << '\n';

    //  ReceiveBuffer const & rbe = buffers.back();
    //  std::cout << "[recvStream::empty] last buf offs " << rbe.bufOffset << "; last buf size: " << rbe.bufSize << '\n';
    //}
    return buffers.empty();
  }

  void ReceiveStream::resetOffsets() {
    minOffset = 0;
    maxOffset = 0;
  }

  void ReceiveStream::addBuffer(unsigned char const* data, size_t dataSize, size_t offset) {
    offset = (offset != ULLONG_MAX) ? offset : (maxOffset == 0) ? 0 : maxOffset; //?? +1;

    size_t curBufOffset = offset; // offset inside all stream
    size_t dataOffset = 0; // offset insied data;

    while (dataOffset < dataSize) {
      ReceiveBuffer drb;

      size_t curBufDataSize = std::min(dataSize - dataOffset, RECV_BUF_SIZE);
      drb.bufOffset = curBufOffset;
      drb.initialize(data + dataOffset, curBufDataSize);

      dataOffset += curBufDataSize;
      curBufOffset += curBufDataSize;

      buffers.emplace_back(std::move(drb));
    }

    minOffset = std::min(minOffset, offset);
    maxOffset = std::max(maxOffset, offset + dataSize);

    //std::cout << "[RecvStream::addBuffer] min off: " << minOffset << "; max off: " << maxOffset << '\n';
  }

  size_t ReceiveStream::getFromOffset(unsigned char* destBuf, size_t count, size_t offset) const {
    //std::cout << "[RecvStream::getFromOff] min off: " << minOffset << "; max off: " << maxOffset << '\n';
    if (offset > maxOffset || offset < minOffset) {
      return 0;
    }

    size_t destPutOffset = 0;

    for (auto bufIt = buffers.cbegin(); bufIt != buffers.cend(); ++bufIt) {
      //std::cout << "[RecvStream::getFromOff] r offs " << offset << "; r count: " << count << '\n';

      size_t const & off = bufIt->bufOffset;
      size_t const & sz = bufIt->bufSize;

      //std::cout << "[RecvStream::getFromOff] c offs " << off << "; c count: " << sz << '\n';
      if (off <= offset && offset < off + sz) {
        std::array<unsigned char, RECV_BUF_SIZE> const & aBuf = bufIt->bufData;
        size_t localFrom = (offset - off);
        size_t localTo = std::min((offset + count), (off + sz)) - off;

        //std::cout << "[RecvStream::getFromOff] l from: " << localFrom << "; l to: " << localTo << '\n';

        auto startIt = aBuf.begin() + localFrom;
        auto endIt = aBuf.begin() + localTo;

        std::copy(startIt, endIt, destBuf + destPutOffset);

        offset = offset + (std::min((offset + count), (off + sz)) - off); //?? +1;
        count = count - (localTo - localFrom);
        destPutOffset += localTo - localFrom;
      }

      if (count == 0) {
        break;
      }
    }
    return destPutOffset;
  }

  size_t ReceiveStream::popData(size_t count) {
    //std::cout << "[RecvStream::popData] pop count: " << count << '\n';

    auto bufIt = buffers.begin();
    while (bufIt != buffers.end() && count != 0) {
      size_t& sz = bufIt->bufSize;

      if (count < sz) {
        count -= bufIt->pop(count);
      }
      else {
        count -= sz;
        bufIt = buffers.erase(bufIt);
      }
    }

    if (buffers.empty()) {
      minOffset = maxOffset;
    }
    else {
      if (bufIt != buffers.end()) {
        //std::cout << "[RecvStream::popData] c off: " << bufIt->bufOffset << "; min offset: " << minOffset << '\n';
        minOffset = std::max(bufIt->bufOffset, minOffset);
      }
      else {
        minOffset = maxOffset;
      }
    }

    return count;
  }

  size_t ReceiveStream::readAndPop(std::vector<unsigned char> & destBuf, size_t count) { // offset is always minOffset
    //std::cout << "[RecvStream::readAndPop] vector version\n";
    std::unique_ptr<unsigned char[]> tmpBuf = std::make_unique<unsigned char[]>(count);
    
    size_t result = readAndPop(tmpBuf.get(), count);

    destBuf.assign(tmpBuf.get(), tmpBuf.get() + count);

    return result;
  }

  size_t ReceiveStream::readAndPop(unsigned char* destBuf, size_t count) { // offset is always minOffset
    //std::cout << "[RecvStream::readAndPop] count: " << count << "; off: " << minOffset << "; max Off: " << maxOffset << '\n';
    size_t result = getFromOffset(destBuf, count, minOffset);

    size_t toPop = result;
    popData(toPop);
    return result;
  }
} // namespace Deployka
