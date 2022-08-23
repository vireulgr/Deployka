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
  std::vector<std::vector<MemberInfo>> m_messages;
  std::vector<MemberInfo> m_memberInfo;

  ReceiveStream drs;

  bool m_msgReceiveComplete;
  bool m_notEnoughBuffer;
  int m_nextMemberIndex;

  MessageType m_currentMessageType;

  //================================================================================
  MessageReceiver();

  //================================================================================
  bool haveReceivedMessages() const;

  //================================================================================
  bool done() const;

  //================================================================================
  std::vector<std::vector<MemberInfo>> getReceivedMessages();

  //================================================================================
  void cleanupReceiveState();

  //================================================================================
  void receive(unsigned char const* data, size_t dataSize);

  //================================================================================
  void parseData();
};

}

#endif
