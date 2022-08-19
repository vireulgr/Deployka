#include "MessageReceiver.h"
#include <vector>
#include <iostream>
#include <exception>
#include <algorithm>

namespace Deployka {
MessageReceiver::MessageReceiver()
  : m_notEnoughBuffer(false)
  , m_msgReceiveComplete(false)
  , m_dynamicOffset(0)
  , m_nextMemberIndex(0)
  , m_currentMessageType(DMT_Null)
{ }

//================================================================================
bool MessageReceiver::done() const {
  return m_msgReceiveComplete;
}

//================================================================================
bool MessageReceiver::haveReceivedMessages() const {
  return !m_messages.empty();
}

//================================================================================

std::vector<std::vector<MemberInfo>> MessageReceiver::getReceivedMessages() {
  return std::move(m_messages);
}

//================================================================================
void MessageReceiver::cleanupReceiveState() {
  m_dynamicOffset = 0;
  m_nextMemberIndex = 0;
  m_currentMessageType = DMT_Null;
  m_notEnoughBuffer = false;
}

//================================================================================
void MessageReceiver::receive(unsigned char const* data, size_t dataSize) {

  drs.addBuffer(data, dataSize);
  m_notEnoughBuffer = false;

  //std::cout << "[recvr::receive] received: " << dataSize << "; msgReceived: " << m_msgReceived << '\n';
  while (!m_notEnoughBuffer && !drs.empty()) {

  size_t msgReceived = drs.getSize();

  if (m_currentMessageType == DMT_Null
    && msgReceived >= sizeof(m_currentMessageType))
  {
    drs.readAndPop(reinterpret_cast<unsigned char*>(& m_currentMessageType), sizeof(m_currentMessageType));

    if (m_currentMessageType >= MessageType::DMT_MessageTypeMax) {
      std::cout << "[recvr::receive] ERROR: message type received is " << m_currentMessageType << '\n';
      throw std::logic_error("Wrong message type");
    }

    //std::cout << "[recvr::receive] received command: >>>>>>" << m_currentMessageType << "<<<<<<\n";
    m_memberInfo.clear();
    m_memberInfo = buildMemberInfo((MessageType)m_currentMessageType);
    m_nextMemberIndex = 1; // first (0) is already readed as m_currentMessageType
    m_msgReceiveComplete = false;
  }

  // process rest of received data in deployka receive stream
  parseData();

  // if all command is received: cleanup receive state
  if (!m_memberInfo.empty()
    && std::all_of(m_memberInfo.cbegin(), m_memberInfo.cend(), [] (MemberInfo const& mi) { return mi.done; })) {
    if (drs.empty()) {
      drs.resetOffsets();
      //std::cout << "[recvr::receive] stream is empty\n";
      cleanupReceiveState();
    }
    m_messages.emplace_back(std::move(m_memberInfo));
    m_dynamicOffset = 0;
    m_msgReceiveComplete = true;
    //std::cout << "receive is complete\n";
    m_currentMessageType = DMT_Null;
    //std::cout << "[recvr::receive] after cleanup received state\n";
  }
  }
}

//================================================================================
void MessageReceiver::parseData() {
  //std::cout << "[recvr::procRdData] in               done?    mbrOff          mbrSize           dynOffs\n";

  for (int i = m_nextMemberIndex; i < m_memberInfo.size(); ++i) {
    MemberInfo& mi = m_memberInfo[i];
    //std::cout << "[recvr::procRdData] "
    //   << std::setw(20) << std::boolalpha << mi.done << std::noboolalpha
    //   << std::setw(10) << mi.memberOffset
    //   << std::setw(17) << mi.memberSize
    //   << std::setw(17) << m_dynamicOffset
    //   << '\n';

    size_t msgReceived = drs.getSize();

    if (msgReceived < mi.memberOffset + mi.memberSize + m_dynamicOffset
      && (i > m_nextMemberIndex || i == 1))
    {
      m_nextMemberIndex = i;
      m_notEnoughBuffer = true;
      //std::cout << "[recvr::procRdData] Next member (" << i << ") is not received yet; max offset: "<< drs.maxOffset << "\n";
      break;
    }

    if (msgReceived >= mi.memberOffset + mi.memberSize + m_dynamicOffset
      && !mi.done)
    {
      mi.buffer.reserve(mi.memberSize);
      size_t readed = drs.readAndPop(mi.buffer, mi.memberSize);

      if (readed != mi.memberSize) {
        std::cout << "ERROR readed less data than required! readed: " << readed << "; member size: " << mi.memberSize << "\n";
      }

      mi.done = true;

      switch (mi.memberType) {
      //case MT_longLong: {
      //  long long int val = 0;
      //  memcpy(&val, mi.buffer.data(), mi.memberSize);
      //  std::cout << "[recvr::procRdData] long long val: " << val << '\n';
      //  break;
      //}
      //case MT_longDouble: {
      //  long double val = 0;
      //  memcpy(&val, mi.buffer.data(), mi.memberSize);
      //  std::cout << "[recvr::procRdData] long double val: " << val << '\n';
      //  break;
      //}
      case MT_dynamic: {
        //std::cout << "[recvr::procRdData] dynamic ";
        //printString(mi.buffer);
        m_dynamicOffset += mi.memberSize;
        break;
      }
      case MT_dynamicSize: {
        if ((i < m_memberInfo.size()-1)
          && (m_memberInfo[i + 1].memberType == MT_dynamic))
        {
          size_t & dynamicMemberSize = m_memberInfo[i + 1].memberSize;
          memcpy(&dynamicMemberSize, mi.buffer.data(), mi.memberSize);
          //std::cout << "[recvr::procRdData] dynamic SIZE: " << m_memberInfo[i+1].memberSize << "\n";
        }
        break;
      }
      }
    }
  }
  //std::cout << "[recvr::procRdData] out\n";
}
} // namespace Deployka
