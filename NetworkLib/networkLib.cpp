#include "networkLib.h"
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
}
