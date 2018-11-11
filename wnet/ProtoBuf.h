#pragma once

#include <memory>
#include <string>

#include <google/protobuf/message.h>

namespace wnet {

class Buffer;

using Message = ::google::protobuf::Message;

enum class ParseResult { 
  PARSING = 1,
  PARSE_SUCCESS,
  MESSAGE_INCOMPLETED,
  UNKNOWN_MESSAGE_TYPE,
  PARSE_ERROR,
  CONNECTION_CLOSED_BY_PEER,
  TIMEOUT
};

class ProtoBuf {
  private:
    static thread_local ParseResult parseResult;
    
  public:
    static std::shared_ptr<Message> getMessageViaName(const std::string& typeName);
    
    static void encodeIntoBuffer(std::shared_ptr<Message> message, std::shared_ptr<Buffer> buffer);

    static std::shared_ptr<Message> decodeFromBuffer(std::shared_ptr<Buffer> buffer);

    static ParseResult getParseResult();

};

}