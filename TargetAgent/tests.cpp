#include "tests.h"
#include "networkLib.h"
#include "ReceiveStream.h"
#include "MessageReceiver.h"

#include <memory>
#include <iostream>
#include <algorithm>


namespace TEST {
  char const g_sampleMessage[] = 
                                "\x01\x00\x00\x00\x00\x00\x00\x00" // file message type
                                "\x11\x00\x00\x00\x00\x00\x00\x00" // file name size
                                "\x73\x6f\x6d\x65\x6f\x74\x68\x65\x72\x66\x69\x6c\x65\x2e\x74\x78\x74" // file name
                                "\x5c\x00\x00\x00\x00\x00\x00\x00" // file data size
                                "\x53\x6f\x6d\x65\x20\x6f\x74\x68\x65\x72\x20\x66\x69\x6c\x65\x20" // file data
                                "\x63\x6f\x6e\x74\x65\x6e\x74\x2e\x0d\x0a\x54\x68\x65\x20\x71\x75"
                                "\x69\x63\x6b\x20\x62\x72\x6f\x77\x6e\x20\x66\x6f\x78\x20\x6a\x75"
                                "\x6d\x70\x73\x20\x6f\x76\x65\x72\x20\x61\x20\x6c\x61\x7a\x79\x20"
                                "\x64\x6f\x67\x2e\x0d\x0a\x65\x6e\x64\x20\x65\x6e\x64\x20\x65\x6e"
                                "\x64\x20\x65\x6e\x64\x20\x65\x6e\x64\x2e\x0d\x0a"
                                ;

  char const g_zeros[] = "0000000000000000000000000000000000000000000000000";
  char const g_ones[] = "1111111111111111111111111111111111111111111111111";
  char const g_twos[] = "2222222222222222222222222222222222222222222222222";
  char const g_threes[] = "3333333333333333333333333333333333333333333333333";

  char const g_sequental0[] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09"
                        "\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13"
                        "\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d"
                        "\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27"
                        "\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31"
                        "\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b"
                        "\x3c\x3d\x3e\x3f\x40\x41\x42\x43\x44\x45"
                        "\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"; // 80 / 0..79

  char const g_sequental1[] = "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59"
                        "\x5a\x5b\x5c\x5d\x5e\x5f\x60\x61\x62\x63"
                        "\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d"
                        "\x6e\x6f\x70\x71\x72\x73\x74\x75\x76\x77"
                        "\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f\x80\x81"
                        "\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b"
                        "\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95"
                        "\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"; // 80 / 80..159

  char const g_sequental2[] = "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9"
                        "\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3"
                        "\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd"
                        "\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7"
                        "\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1"
                        "\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb"
                        "\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5"
                        "\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"; // 80 / 160..239

  char const g_sequental3[] = "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9"
                        "\xfa\xfb\xfc\xfd\xfe\xff\x00\x01\x02\x03"
                        "\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d"
                        "\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17"
                        "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21"
                        "\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b"
                        "\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35"
                        "\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
                        "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49"
                        "\x4a\x4b\x4c\x4d\x4e\x4f";                 // / 240..


// ================================================================================
void printBuffersBoundries(Deployka::ReceiveStream & drs) {

  std::cout << "Buffers boundries:\n";

  size_t curBufSize = 0;

  for (auto it = drs.buffers.begin(); it != drs.buffers.end(); ++it) {
    curBufSize = it->bufSize;
    std::cout << "cur buf size: " << curBufSize << '\n';
    Deployka::printHexRange(it->bufData.data(), 0, 6);
    std::cout << "... ";
    Deployka::printHexRange(it->bufData.data(), curBufSize-7, curBufSize);
    std::cout << '\n';
  }
}


// ================================================================================
inline unsigned char * cleanupCharArray(unsigned char* ptr, size_t size) {
  return std::transform(ptr, ptr + size, ptr, [](unsigned char) { return '\0'; });
}

// ================================================================================
void readManyBytes() {
  struct MyGenerator {
    unsigned char curValue;
    MyGenerator() : curValue(0) {}
    unsigned char operator()() {
      return curValue++;
    }
  };

  std::cout << __FUNCTION__ << '\n';
  MyGenerator generatorState;

  size_t constexpr bufSize = 1ull << 16 ;
  std::unique_ptr<unsigned char[]>aBuffer = std::make_unique<unsigned char[]>(bufSize);

  std::transform(aBuffer.get(), aBuffer.get() + bufSize, aBuffer.get(), [&generatorState](char) { return generatorState(); });

  Deployka::ReceiveStream drs;

  drs.addBuffer(aBuffer.get(), bufSize);

  printBuffersBoundries(drs);
}

// ================================================================================
void getFromOffset() {
  std::cout << __FUNCTION__ << '\n';
  Deployka::ReceiveStream rcvStream;

  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_sequental0), sizeof(g_sequental0)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_zeros), sizeof(g_zeros)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_ones), sizeof(g_ones)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_twos), sizeof(g_twos)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_threes), sizeof(g_threes)-1);

  constexpr size_t myBufSize = 100;
  std::unique_ptr<unsigned char[]> myBuffer = std::make_unique<unsigned char[]>(myBufSize);

  rcvStream.getFromOffset(myBuffer.get(), myBufSize, 19);

  Deployka::printHex(myBuffer.get(), myBufSize);
}

// ================================================================================
void readAndPop_twoBuffers() {
  std::cout << __FUNCTION__ << '\n';
  Deployka::ReceiveStream rcvStream;

  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_sequental0), sizeof(g_sequental0)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_sequental1), sizeof(g_sequental1)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_sequental2), sizeof(g_sequental2)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_sequental3), sizeof(g_sequental3)-1);

  constexpr size_t myBufSize = 168;
  std::unique_ptr<unsigned char[]> myBuffer = std::make_unique<unsigned char[]>(myBufSize);

  size_t result = rcvStream.readAndPop(myBuffer.get(), 160);

  std::cout << "Read and pop result: " << result << '\n';

  Deployka::printHex(myBuffer.get(), myBufSize);
}

// ================================================================================
void readAndPop_twice() {
  std::cout << __FUNCTION__ << '\n';
  Deployka::ReceiveStream rcvStream;

  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_sequental0), sizeof(g_sequental0)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_sequental1), sizeof(g_sequental1)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_sequental2), sizeof(g_sequental2)-1);
  rcvStream.addBuffer(reinterpret_cast<unsigned char const *>(g_sequental3), sizeof(g_sequental3)-1);

  constexpr size_t myBufSize = 100;
  std::unique_ptr<unsigned char[]> myBuffer = std::make_unique<unsigned char[]>(myBufSize);

  size_t result = rcvStream.readAndPop(myBuffer.get(), 80);

  std::cout << "Read and pop result: " << result << '\n';

  Deployka::printHex(myBuffer.get(), myBufSize);

  cleanupCharArray(myBuffer.get(), myBufSize);

  /*size_t*/ result = rcvStream.readAndPop(myBuffer.get(), 80);

  std::cout << "Read and pop result: " << result << '\n';

  Deployka::printHex(myBuffer.get(), myBufSize);
}

// ================================================================================
void multipleMessagesInStream() {
  std::cout << __FUNCTION__ << '\n';

  size_t constexpr bufSize = 9000;
  std::unique_ptr<unsigned char[]> aBuffer = std::make_unique<unsigned char[]>(bufSize);

  size_t constexpr msgSize = sizeof(g_sampleMessage)-1;

  size_t bufOffset = 0;
  while (bufOffset < bufSize) {
    size_t const toCopy = std::min(msgSize, bufSize - bufOffset);

    memcpy_s(aBuffer.get() + bufOffset, bufSize - bufOffset, g_sampleMessage, toCopy);
    bufOffset += toCopy;
  }

  Deployka::MessageReceiver msgReceiver;

  msgReceiver.receive(aBuffer.get(), bufSize);
  if (msgReceiver.haveReceivedMessages()) {
    msgReceiver.getReceivedMessages();
  }
}


} // namespace TEST
