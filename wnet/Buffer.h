#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <arpa/inet.h>

#include "Config.h"
#include "Noncopyable.h"
#include "ProtoBuf.h"

namespace wnet {

class Buffer : public noncopyable, public std::enable_shared_from_this<Buffer> {
  private:
    char* buffer;
    size_t  begin_pos,      // offset of where actual data begins
            end_pos,        // offset of where actual data ends + 1
            capacity;       // size of buffer currently

    void shiftData() { 
      if(begin_pos == 0) {
        return;
      }
      std::copy(begin(), end(), buffer);        // std::copy works fine with overlap problem
      end_pos = end_pos - begin_pos; 
      begin_pos = 0; 
    }

    void expand(size_t suggestLen = 0) {
      // make new newCapacity
      size_t newCapacity = std::max({2 * capacity, size() + suggestLen});
      capacity = newCapacity;

      // allocate new storage for larger buffer
      char* newBuffer = new char[newCapacity + 1];
      std::copy(begin(), end(), newBuffer);
      delete[] buffer;
      buffer = newBuffer;

      // update data offset
      end_pos -= begin_pos;
      begin_pos = 0;
    }

  public:
    Buffer(size_t bufSize = DEFAULT_BUFFER_SIZE): buffer(nullptr), 
                                                  begin_pos(0), 
                                                  end_pos(0), 
                                                  capacity(bufSize) {
      buffer = new char[bufSize + 1];
    }

    ~Buffer() { 
      delete[] buffer; 
    }

    char* begin() const {
      return buffer + begin_pos; 
    }

    char* end() const { 
      return buffer + end_pos; 
    }

    const char* data() { 
      *(end()) = '\0';   // make it const char *    won't affect data
      return begin(); 
    }

    size_t size() const { 
      return end_pos - begin_pos; 
      }

    bool empty() const { 
      return end_pos == begin_pos; 
    }

    size_t space() const {    // char length after remaining data, space before data excluded
      return capacity - end_pos + 1; 
    }

    void clear() {
      begin_pos = end_pos = 0; 
    }
    
    void allocate(size_t suggestLen = 0) {
      // if suggestLen not specified, make capacity twice larger than size()
      if(suggestLen == 0 || space() < suggestLen) {
        if(size() + suggestLen < capacity / 2) {
          shiftData();
        } else {
          expand(suggestLen);
        }
      }
    }
    
    // update offset after fetching data from buffer
    void consume(size_t len) { 
      begin_pos += len; 
      if (empty()) {
        clear();
      }
    }
    
    // update offset after writing data into buffer NOT via append() functions
    void addSize(size_t len) {
      end_pos += len;
    }

    char* occupy(size_t len) {   // occupy some space for writing data
      if(space() < len) {
        allocate(len);
      }
      auto curr_pos = end();
      addSize(len);
      return curr_pos;
    }

    void append(const char* src, size_t len) {
      std::copy(src, src + len, occupy(len)); 
    }

    void append(const char* src) {
      append(src, ::strlen(src)); 
    }

    void append(const std::string& str) { 
      append(str.c_str()); 
    }

    void append(std::shared_ptr<Buffer> buf) {
      append(buf->begin(), buf->size());
    }

    // encode protobuf message into output buffer
    void append(std::shared_ptr<Message> message) {
      ProtoBuf::encodeIntoBuffer(message, shared_from_this());
    }

    void append_uint32(uint32_t num) {
      num = ::htonl(num);     //  from host byte order to network byte order
      append(reinterpret_cast<const char*>(&num), sizeof num);
    }

    size_t fetch(char* dest, size_t len) {
      len = std::min({size(), len});
      std::copy(begin(), begin() + len, dest); 
      consume(len);
      return len;
    }

    uint32_t fetch_uint32(char* parse_pos = nullptr) {
      parse_pos = parse_pos ? parse_pos : begin();
      uint32_t num = 0;
      std::copy(parse_pos, parse_pos + (sizeof num), reinterpret_cast<char*>(&num));
      // consume(sizeof num);
      return ::ntohl(num);     // from network byte order to host byte order
    }

    // decode protobuf message from input buffer
    std::shared_ptr<Message> fetchMessage(ParseResult& parseResult) {
      std::shared_ptr<Message> message = ProtoBuf::decodeFromBuffer(shared_from_this());
      parseResult = ProtoBuf::getParseResult();
      return message;
    }


};

}