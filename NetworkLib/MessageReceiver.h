#ifndef __MESSAGE_RECEIVER_H__
#define __MESSAGE_RECEIVER_H__
#include "networkLib.h"
#include "ReceiveStream.h"
#include <vector>

/******************************************************************************//*
*
*********************************************************************************/
namespace Deployka {
struct MessageReceiver {
  std::vector<MemberInfo> m_memberInfo;

  ReceiveStream drs;

  size_t m_msgReceived;
  bool m_msgReceiveComplete;
  size_t m_dynamicOffset; // sum of length of all dynamic data members that has been processed so far
  int m_nextMemberIndex;

  MessageType m_currentMessageType;

  //================================================================================
  MessageReceiver();

  //================================================================================
  bool haveReceivedMessages() const;

  //================================================================================
  bool done() const;

  //================================================================================
  std::vector<MemberInfo> getReceivedMessages(); // TODO

  //================================================================================
  void cleanupReceiveState();

  //================================================================================
  void receive(unsigned char const* data, size_t dataSize);

  //================================================================================
  void parseData();
};

}

#endif
