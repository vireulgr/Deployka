#include "networkLib.h"
#include <iostream>
#include <iomanip>

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
}
