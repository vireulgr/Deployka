#include "networkLib.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
//#include <cassert>

namespace Deployka {

  std::vector<MemberInfo> buildMemberInfo(MessageType msgType) {
    std::vector<MemberType> const memberTypeVec = Deployka::g_commands.at(msgType);

    std::vector<MemberInfo> result;
    size_t memberOffset = 0;
    for (size_t i = 0; i < memberTypeVec.size(); ++i) {
      MemberType const item = memberTypeVec[i];

      MemberInfo mi;
      mi.memberType = item;
      mi.memberSize = g_memberTypeSizes[item];
      if (i == 0) {
        unsigned char const* ptr = reinterpret_cast<unsigned char const*>(&msgType);
        mi.buffer.assign(ptr, ptr + sizeof(msgType));
        mi.done = true;
        mi.memberOffset = 0;
      }
      else {
        mi.done = false;
        mi.memberOffset = memberOffset;
      }

      if (mi.memberSize > 0) {
        mi.buffer.reserve(mi.memberSize);
      }

      memberOffset += mi.memberSize;
      result.push_back(mi);
    }
    return result;
  }

  void clearMemberInfo(std::vector<Deployka::MemberInfo>& miVec) {
    std::transform(
      miVec.begin() + 1, 
      miVec.end(),
      miVec.begin() + 1,
      [] (Deployka::MemberInfo& item) {
      item.done = false;
      item.buffer.clear();
      return item;
    });
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


  //================================================================================
  void printHexRange(std::vector<unsigned char> const & buf, size_t start, size_t count) {
    std::cout << std::hex << std::setfill('0');
    size_t minCount = std::min(0ull, buf.size() - start - count);
    size_t minStart = std::min(start, buf.size());
    for (size_t i = minStart; i < minCount; i++) {
      std::cout << "0x" <<  std::setw(2) << static_cast<unsigned int>(buf[i]) << ' ';
      if ((i + 1) % 16 == 0) {
        std::cout << '\n';
      }
    }
    std::cout << std::dec << std::setfill(' ');
  }

  //================================================================================
  void printHexRange(unsigned char const * buf, size_t start, size_t count) {
    std::cout << std::hex << std::setfill('0');
    for (size_t i = start; i < count; i++) {
      std::cout << "0x" <<  std::setw(2) << static_cast<unsigned int>(buf[i]) << ' ';
      if ((i + 1) % 16 == 0) {
        std::cout << '\n';
      }
    }
    std::cout << std::dec << std::setfill(' ');
  }

  //================================================================================
  void printHex(unsigned char const * buf, size_t bufSize) {
    std::cout << "HEX:\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n";
    if (bufSize > 262) {
      size_t endClamp = bufSize - 128;
      printHexRange(buf, 0, 128);
      std::cout << "\n...\n";
      printHexRange(buf, endClamp, bufSize);
    }
    else {
      printHexRange(buf, 0, bufSize);
    }
    std::cout << "<<<<<<<\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
  }

  //================================================================================
  void printHex(std::vector<unsigned char> & vec) {
    std::cout << "HEX:\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n";
    std::cout << std::hex << std::setfill('0');
    if (vec.size() > 262) {
      size_t endClamp = vec.size() - 128;
      printHexRange(vec, 0, 128);
      std::cout << "\n...\n";
      printHexRange(vec, endClamp, vec.size());
    }
    else {
      printHexRange(vec, 0, vec.size());
    }
    std::cout << "<<<<<<<\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    std::cout << std::dec << std::setfill(' ');
  }

} // namespace Deployka
