#include "Buffer.h"
#include "ProtoBuf.h"

#include <google/protobuf/descriptor.h>

using namespace wnet;

thread_local ParseResult ProtoBuf::parseResult = ParseResult::PARSING;

using Descriptor = ::google::protobuf::Descriptor;
using DescriptorPool = ::google::protobuf::DescriptorPool;
using MessageFactory = ::google::protobuf::MessageFactory;

std::shared_ptr<Message> ProtoBuf::getMessageViaName(const std::string& typeName) {
  std::shared_ptr<Message> message = nullptr;
  const google::protobuf::Descriptor* descriptor = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
  if(descriptor) {
    const Message* prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
    if(prototype) {
      message = std::shared_ptr<Message>(prototype->New());
    }
  }
  return message;
}

void ProtoBuf::encodeIntoBuffer(std::shared_ptr<Message> message, std::shared_ptr<Buffer> buffer) {
  const std::string& typeName = message->GetDescriptor()->full_name();
  // write type name length to buffer
  buffer->append_uint32(static_cast<uint32_t>(typeName.size()));
  // write type name to buffer
  buffer->append(typeName);
  // write message length to buffer
  buffer->append_uint32(static_cast<uint32_t>(message->ByteSize()));
  // serialize message to buffer
  message->SerializeToArray(buffer->occupy(message->ByteSize()), message->ByteSize());
}

std::shared_ptr<Message> ProtoBuf::decodeFromBuffer(std::shared_ptr<Buffer> buffer) {
  parseResult = ParseResult::PARSING;

  auto bufSize = buffer->size();
  auto curr_pos = buffer->begin();
  size_t sizeofSizeField = sizeof(uint32_t);

  auto decodedSize = sizeofSizeField * 2;
  if (bufSize < decodedSize) {  
    parseResult = ParseResult::MESSAGE_INCOMPLETED;
    return nullptr;
  }

  auto typeNameSize = buffer->fetch_uint32();
  decodedSize += typeNameSize;
  if (bufSize < decodedSize) {      
    parseResult = ParseResult::MESSAGE_INCOMPLETED;
    return nullptr;
  }

  auto messageSizePos = curr_pos + sizeofSizeField + typeNameSize;
  auto messageSize = buffer->fetch_uint32(messageSizePos);
  decodedSize += messageSize;
  if (bufSize < decodedSize) {    
    parseResult = ParseResult::MESSAGE_INCOMPLETED;  
    return nullptr;
  }
  
  // buffer size larger than or equal to message size, start parse message
  buffer->consume(decodedSize);
  auto message = getMessageViaName(std::string(curr_pos + sizeofSizeField, typeNameSize));
  if(!message) {    
    parseResult = ParseResult::UNKNOWN_MESSAGE_TYPE;
    return nullptr;
  }

  if(!message->ParseFromArray(messageSizePos + sizeofSizeField, messageSize)) {
    parseResult = ParseResult::PARSE_ERROR;
    return nullptr;
  }
  parseResult = ParseResult::PARSE_SUCCESS;
  return message;
}

ParseResult ProtoBuf::getParseResult() {
  return parseResult;
}
