#include "networkLib.h"
#include <iostream>
#include <iomanip>
//#include <cassert>

namespace Deployka {

  std::vector<MemberInfo> buildMemberInfo(std::vector<MemberType> const mt) {
    std::vector<MemberInfo> result;
    size_t memberOffset = 0;
    for (auto const& item : mt) {
      MemberInfo mi;
      mi.memberType = item;
      mi.memberOffset = memberOffset;
      mi.memberSize = g_memberTypeSizes[item];
      mi.done = false;

      if (mi.memberSize > 0) {
        mi.buffer.reserve(mi.memberSize);
      }

      memberOffset += mi.memberSize;
      result.push_back(mi);
    }
    return result;
  }

  void printString(std::vector<unsigned char> & vec) {
    std::cout << "STR:\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n";
    if (vec.size() > 262) {
      size_t endClamp = vec.size() - 128;
      for (size_t i = 0; i < 128; i++) {
        std::cout << vec[i];
      }
      std::cout << "\n...\n";
      for (size_t i = endClamp; i < vec.size(); i++) {
        std::cout << vec[i];
      }
    }
    else {
      for (size_t i = 0; i < vec.size(); i++) {
        std::cout << vec[i];
      }
    }
    std::cout << "<<<<<<<\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
  }

  void printHex(std::vector<unsigned char> & vec) {
    std::cout << "HEX:\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n";
    std::cout << std::hex << std::setfill('0');
    if (vec.size() > 262) {
      size_t endClamp = vec.size() - 128;
      for (size_t i = 0; i < 128; i++) {
        std::cout << "0x" <<  std::setw(2) << static_cast<unsigned int>(vec[i]) << ' ';
        if ((i + 1) % 16 == 0) {
          std::cout << '\n';
        }
      }
      std::cout << "\n...\n";
      for (size_t i = endClamp; i < vec.size(); i++) {
        std::cout << "0x" <<  std::setw(2) << static_cast<unsigned int>(vec[i]) << ' ';
        if ((i + 1 )% 16 == 0) {
          std::cout << '\n';
        }
      }
    }
    else {
      for (size_t i = 0; i < vec.size(); i++) {
        std::cout << "0x" <<  std::setw(2) << static_cast<unsigned int>(vec[i]) << ' ';
        if ((i + 1 )% 16 == 0) {
          std::cout << '\n';
        }
      }
    }
    std::cout << "<<<<<<<\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    std::cout << std::dec << std::setfill(' ');
  }

  /******************************************************************************//*
  * ReceiveBuffer:
  *********************************************************************************/
  ReceiveBuffer::ReceiveBuffer()
    : bufOffset(0)
    , bufSize(0)
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
    unsigned char * resPtr = std::copy(bufData.begin() + offset, bufData.end(), data);
    return resPtr - data;
  }

  size_t ReceiveBuffer::pop(size_t count) {
    if (count > bufSize) {
      size_t tmp = bufSize;
      bufOffset = bufOffset + bufSize;
      bufSize = 0;
      return tmp;
    }

    bufOffset += count;
    bufSize -= count;

    auto it = std::copy(bufData.begin() + count, bufData.end(), bufData.begin());
    return RECV_BUF_SIZE - std::distance(bufData.begin(), it);
  }

  /******************************************************************************//*
  * ReceiveStream
  *********************************************************************************/

  ReceiveStream::ReceiveStream()
    : minOffset(0)
    , maxOffset(0)
  {}
  
  bool ReceiveStream::empty() const {
    return buffers.empty();
  }

  void ReceiveStream::resetOffsets() {
    minOffset = 0;
    maxOffset = 0;
  }

  void ReceiveStream::addBuffer(unsigned char const* data, size_t dataSize, size_t offset) {

    offset = (offset != ULLONG_MAX) ? offset : (maxOffset == 0) ? 0 : maxOffset; //?? +1;

    ReceiveBuffer drb;
    drb.bufOffset = offset;
    drb.initialize(data, dataSize);

    buffers.emplace_back(std::move(drb));

    minOffset = std::min(minOffset, offset);
    maxOffset = std::max(maxOffset, offset + dataSize);

    std::cout << "[RecvStream::addBuffer] min off: " << minOffset << "; max off: " << maxOffset << '\n';
  }

  size_t ReceiveStream::getFromOffset(unsigned char* destBuf, size_t count, size_t offset) const {
    std::cout << "[RecvStream::getFromOff] min off: " << minOffset << "; max off: " << maxOffset << '\n';
    if (offset > maxOffset || offset < minOffset) {
      return 0;
    }

    size_t destPutOffset = 0;

    for (auto bufIt = buffers.cbegin(); bufIt != buffers.cend(); ++bufIt) {
      std::cout << "[RecvStream::getFromOff] r offs " << offset << "; r count: " << count << '\n';

      size_t const & off = bufIt->bufOffset;
      size_t const & sz = bufIt->bufSize;

      std::cout << "[RecvStream::getFromOff] c offs " << off << "; c count: " << sz << '\n';
      if (off <= offset && offset < off + sz) {
        std::array<unsigned char, RECV_BUF_SIZE> const & aBuf = bufIt->bufData;
        size_t localFrom = (offset - off);
        size_t localTo = std::min((offset + count), (off + sz)) - off;

        std::cout << "[RecvStream::getFromOff] l from: " << localFrom << "; l to: " << localTo << '\n';

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
    std::cout << "[RecvStream::popData] pop count: " << count << '\n';

    auto bufIt = buffers.begin();
    while (bufIt != buffers.end() && count != 0) {
      //size_t& off = bufIt->bufOffset;
      size_t& sz = bufIt->bufSize;

      if (count < sz) {
        //ReceiveBuffer drb;
        //drb.bufOffset = off + count;
        //drb.bufSize = sz - count;

        //std::cout << "l off: " << off << "; l sz: " << sz << '\n';

        //std::array<unsigned char, RECV_BUF_SIZE> & aBuf = bufIt->bufData;

        //std::copy(aBuf.begin() + count, aBuf.end(), drb.bufData.begin());

        //*bufIt = std::move(drb);

        //count = 0;

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
        std::cout << "[RecvStream::popData] c off: " << bufIt->bufOffset << "; min offset: " << minOffset << '\n';
        minOffset = std::max(bufIt->bufOffset, minOffset);
      }
      else {
        minOffset = maxOffset;
      }
    }

    return count;
  }

  size_t ReceiveStream::readAndPop(std::vector<unsigned char> & destBuf, size_t count) {// offset is always minOffset
    std::cout << "[RecvStream::readAndPop] vector version\n";
    std::unique_ptr<unsigned char[]> tmpBuf = std::make_unique<unsigned char[]>(count);
    
    size_t result = readAndPop(tmpBuf.get(), count);

    destBuf.assign(tmpBuf.get(), tmpBuf.get() + count);

    return result;
  }

  size_t ReceiveStream::readAndPop(unsigned char* destBuf, size_t count) {// offset is always minOffset
    std::cout << "[RecvStream::readAndPop] count: " << count << "; off: " << minOffset << "; max Off: " << maxOffset << '\n';
    size_t result = getFromOffset(destBuf, count, minOffset);

    size_t toPop = result;
    popData(toPop);
    return result;
  }
} // namespace Deployka
