#ifndef __AMF_0_H__
#define __AMF_0_H__

#include <string>
#include <vector>

#include "any.h"

class BitBuffer;
class IoBuffer;

enum Amf0Marker {
  kUnknown = -1,
  kNumber = 0,
  kBoolean,
  kString,
  kObject,
  kMovieclip,
  kNull,
  kUndefined,
  kReference,
  kEcmaArray,
  kObjectEnd,
  kStrictArray,
  kDate,
  kLongString,
  kUnsupported,
  kRecordset,
  kXmlDocument,
  kTypedObject,
};

static std::string Amf0MarkerToStr(const int& marker) {
  switch (marker) {
    case kUnknown:
      return "kUnknown";
      break;
    case kNumber:
      return "kNumber";
      break;
    case kBoolean:
      return "kBoolean";
      break;
    case kString:
      return "kString";
      break;
    case kObject:
      return "kObject";
      break;
    case kMovieclip:
      return "kMovieclip";
      break;
    case kNull:
      return "kNull";
      break;
    case kUndefined:
      return "kUndefined";
      break;
    case kReference:
      return "kReference";
      break;
    case kEcmaArray:
      return "kEcmaArray";
      break;
    case kObjectEnd:
      return "kObjectEnd";
      break;
    case kStrictArray:
      return "kStrictArray";
      break;
    case kDate:
      return "kDate";
      break;
    case kLongString:
      return "kLongString";
      break;
    case kUnsupported:
      return "kUnsupported";
      break;
    case kRecordset:
      return "kRecordset";
      break;
    case kXmlDocument:
      return "kXmlDocument";
      break;
    case kTypedObject:
      return "kTypedObject";
      break;
    default:
      return "kUnknown";
      break;
  }

  return "kUnknown";
}

class AmfCommand {
 public:
  AmfCommand(const bool& delete_when_destruct = true)
      : delete_when_destruct_(delete_when_destruct) {}

  void PushBack(any::Any* any) { vec_any_.push_back(any); }

  ~AmfCommand() {
    if (delete_when_destruct_) {
      for (const auto& any : vec_any_) {
        if (any != NULL) {
          delete any;
        }
      }
    }
  }

  any::Any* operator[](const size_t index) {
    if (vec_any_.size() < index + 1) {
      return NULL;
    }

    return vec_any_[index];
  }

  size_t size() { return vec_any_.size(); }

 private:
  std::vector<any::Any*> vec_any_;
  bool delete_when_destruct_;
};

class Amf0 {
 public:
  static int Decode(std::string& data, AmfCommand& result);
  static int Encode(const std::vector<any::Any*>& input, IoBuffer& output);

 private:
  static int GetType(BitBuffer& bit_bufferk, int& type);
  static int PeekType(BitBuffer& bit_buffer);

  static int Decode(BitBuffer& bit_buffer, AmfCommand& result);
  static int Decode(const int& type, BitBuffer& bit_buffer, any::Any*& result);
  static int DecodeNumber(BitBuffer& bit_buffer, any::Any*& result);
  static int DecodeBoolean(BitBuffer& bit_buffer, any::Any*& result);
  static int DecodeString(BitBuffer& bit_buffer, any::Any*& result);
  static int DecodeObject(BitBuffer& bit_buffer, any::Any*& result);
  static int DecodeEcmaArray(BitBuffer& bit_buffer, any::Any*& result);
  static int DecodeNull(BitBuffer& bit_buffer, any::Any*& result);

  static int Encode(const any::Any* any, IoBuffer& output);
  static int EncodeType(const uint8_t& type, IoBuffer& output);
  static int EncodeNumber(const double& val, IoBuffer& output);
  static int EncodeBoolean(const uint8_t& val, IoBuffer& output);
  static int EncodeString(const std::string& val, IoBuffer& output);
  static int EncodeObject(const std::map<std::string, any::Any*>& val,
                          IoBuffer& output);
};

#endif  // __AMF_0_H__
