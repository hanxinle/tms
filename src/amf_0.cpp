#include "amf_0.h"

#include <iostream>

#include "bit_buffer.h"
#include "common_define.h"
#include "io_buffer.h"

int Amf0::Decode(std::string& data, AmfCommand& result) {
  BitBuffer bit_buffer(data);

  return Decode(bit_buffer, result);
}

int Amf0::Encode(const std::vector<any::Any*>& input, IoBuffer& output) {
  int ret = -1;
  std::cout << "input.size():" << input.size() << std::endl;
  for (const auto& any : input) {
    ret = Encode(any, output);

    if (ret != 0) {
      std::cout << LMSG << "when encode " << any->TypeStr() << "," << any
                << " failed" << std::endl;
      return ret;
    }

    std::cout << LMSG << "when encode " << any->TypeStr() << " success!!!"
              << std::endl;
  }

  return 0;
}

int Amf0::GetType(BitBuffer& bit_buffer, int& type) {
  if (!bit_buffer.MoreThanBytes(1)) {
    std::cout << LMSG << "bit_buffer no more than 1 bytes" << std::endl;
    return -1;
  }

  type = kUnknown;
  bit_buffer.GetBytes(1, type);

  return 0;
}

int Amf0::PeekType(BitBuffer& bit_buffer) {
  if (!bit_buffer.MoreThanBytes(1)) {
    std::cout << LMSG << "bit_buffer no more than 1 bytes" << std::endl;
    return -1;
  }

  uint64_t type = kUnknown;
  bit_buffer.PeekBytes(1, type);

  return type;
}

int Amf0::Decode(BitBuffer& bit_buffer, AmfCommand& result) {
  int type = kUnknown;
  int ret = 0;
  while (bit_buffer.GetBytes(1, type) == 0 && type != kUnknown) {
    std::cout << LMSG << "type:" << type << "->" << Amf0MarkerToStr(type)
              << ",bit_buffer.size():" << bit_buffer.BytesLeft() << std::endl;

    any::Any* any = NULL;
    if (Decode(type, bit_buffer, any) == 0) {
      result.PushBack(any);
    } else {
      ret = -1;
    }
  }

  return ret;
}

int Amf0::Decode(const int& type, BitBuffer& bit_buffer, any::Any*& result) {
  int ret = -1;
  switch (type) {
    case kNumber: {
      ret = DecodeNumber(bit_buffer, result);
      break;
    }

    case kBoolean: {
      ret = DecodeBoolean(bit_buffer, result);
      break;
    }

    case kString: {
      ret = DecodeString(bit_buffer, result);
      break;
    }

    case kObject: {
      ret = DecodeObject(bit_buffer, result);
      break;
    }

    case kEcmaArray: {
      ret = DecodeEcmaArray(bit_buffer, result);
      break;
    }

    case kNull: {
      ret = DecodeNull(bit_buffer, result);
      break;
    }

    default: {
      break;
    }
  }

  return ret;
}

int Amf0::DecodeNumber(BitBuffer& bit_buffer, any::Any*& result) {
  if (!bit_buffer.MoreThanBytes(8)) {
    std::cout << LMSG << "bit_buffer no more than 8 bytes" << std::endl;
    return -1;
  }

  uint64_t n;
  bit_buffer.GetBytes(8, n);

  double num = *(double*)&n;

  std::cout << LMSG << "num:[" << num << "]" << std::endl;

  result = new any::Double(num);

  return 0;
}

int Amf0::DecodeBoolean(BitBuffer& bit_buffer, any::Any*& result) {
  if (!bit_buffer.MoreThanBytes(1)) {
    std::cout << LMSG << "bit_buffer no more than 1 bytes" << std::endl;
    return -1;
  }

  uint8_t n;
  bit_buffer.GetBytes(1, n);

  result = new any::Int(n);

  return 0;
}

int Amf0::DecodeString(BitBuffer& bit_buffer, any::Any*& result) {
  if (!bit_buffer.MoreThanBytes(2)) {
    std::cout << LMSG << "bit_buffer no more than 2 bytes" << std::endl;
    return -1;
  }

  uint16_t len = 0;
  bit_buffer.GetBytes(2, len);

  if (!bit_buffer.MoreThanBytes(len)) {
    std::cout << LMSG << "bit_buffer no more than " << len << " bytes"
              << std::endl;
    return -1;
  }

  std::string str;
  bit_buffer.GetString(len, str);

  std::cout << LMSG << "len:" << len << "[" << str << "]" << std::endl;

  result = new any::String(str);

  return 0;
}

int Amf0::DecodeObject(BitBuffer& bit_buffer, any::Any*& result) {
  uint16_t key_len = 0;

  bool error = false;

  result = new any::Map(true);

  while (bit_buffer.GetBytes(2, key_len) == 0) {
    if (!bit_buffer.MoreThanBytes(key_len)) {
      std::cout << LMSG << "bit_buffer no more than " << key_len << " bytes"
                << std::endl;
      error = true;
      break;
    }

    std::string key;

    bit_buffer.GetString(key_len, key);

    std::cout << LMSG << "key:" << key << ",key_len:" << key_len
              << ",bit_buffer.size:" << bit_buffer.BytesLeft() << std::endl;

    int type = kUnknown;
    if (GetType(bit_buffer, type) != 0) {
      error = true;
      break;
    }

    if (type == kUnknown) {
      error = true;
      break;
    }

    if (type == kObjectEnd) {
      break;
    }

    any::Any* val;
    if (Decode(type, bit_buffer, val) != 0) {
      std::cout << LMSG << "decode key" << key << " failed" << std::endl;
      error = true;
      break;
    }

    result->ToMap().Insert(key, val);
  }

  if (error) {
    delete result;
    return -1;
  }

  return 0;
}

int Amf0::DecodeEcmaArray(BitBuffer& bit_buffer, any::Any*& result) {
  uint32_t element_count = 0;

  if (!bit_buffer.MoreThanBytes(4)) {
    std::cout << LMSG << "bit_buffer no more than 4 bytes" << std::endl;
    return -1;
  }

  bool error = false;

  bit_buffer.GetBytes(4, element_count);

  std::cout << LMSG << "element_count:" << element_count << std::endl;

  result = new any::Map(true);

  for (uint32_t c = 0; c != element_count; ++c) {
    uint16_t key_len = 0;

    if (bit_buffer.GetBytes(2, key_len) == 0) {
      if (!bit_buffer.MoreThanBytes(key_len)) {
        std::cout << LMSG << "bit_buffer no more than " << key_len << " bytes"
                  << std::endl;
        error = true;
        break;
      }

      std::string key;

      bit_buffer.GetString(key_len, key);

      std::cout << LMSG << "index:" << c << ",key:[" << key
                << "],key_len:" << key_len
                << ",bit_buffer.size:" << bit_buffer.BytesLeft() << std::endl;

      int type = kUnknown;
      if (GetType(bit_buffer, type) != 0) {
        error = true;
        break;
      }

      if (type == kUnknown) {
        error = true;
        break;
      }

      if (type == kObjectEnd) {
        break;
      }

      any::Any* val = NULL;
      if (Decode(type, bit_buffer, val) != 0) {
        std::cout << LMSG << "decode key" << key << " failed" << std::endl;
        error = true;
        break;
      }

      result->ToMap().Insert(key, val);
    }
  }

  if (error) {
    delete result;
    return -1;
  }

  return 0;
}

int Amf0::DecodeNull(BitBuffer& bit_buffer, any::Any*& result) {
  UNUSED(bit_buffer);
  UNUSED(result);

  result = NULL;

  return 0;
}

int Amf0::Encode(const any::Any* any, IoBuffer& output) {
  int ret = -1;
  if (any->IsInt()) {
    ret = EncodeType(kNumber, output);
    ret = EncodeNumber(double(any->ToInt().GetVal()), output);
  } else if (any->IsDouble()) {
    ret = EncodeType(kNumber, output);
    ret = EncodeNumber(any->ToDouble().GetVal(), output);
  } else if (any->IsString()) {
    ret = EncodeType(kString, output);
    ret = EncodeString(any->ToString().GetVal(), output);
  } else if (any->IsMap()) {
    ret = EncodeType(kObject, output);
    ret = EncodeObject(any->ToMap().GetVal(), output);
    ret = EncodeType(kObjectEnd, output);
  } else if (any->IsNull()) {
    ret = EncodeType(kNull, output);
  } else {
    std::cout << "unknown any, type:" << (uint16_t)any->GetType() << std::endl;
  }

  return ret;
}

int Amf0::EncodeType(const uint8_t& type, IoBuffer& output) {
  int ret = output.WriteU8(type);
  if (ret != sizeof(uint8_t)) {
    return -1;
  }

  return 0;
}

int Amf0::EncodeNumber(const double& val, IoBuffer& output) {
  uint64_t u64 = *((uint64_t*)&val);

  int ret = output.WriteU64(u64);

  if (ret != sizeof(uint64_t)) {
    return -1;
  }

  return 0;
}

int Amf0::EncodeBoolean(const uint8_t& val, IoBuffer& output) {
  int ret = output.WriteU8(val);

  if (ret != sizeof(uint8_t)) {
    return -1;
  }

  return 0;
}

int Amf0::EncodeString(const std::string& val, IoBuffer& output) {
  int ret = output.WriteU16(val.size());
  if (ret != 2) {
    return -1;
  }

  ret = output.Write(val);

  if (ret != (int)val.size()) {
    return -1;
  }

  return 0;
}

int Amf0::EncodeObject(const std::map<std::string, any::Any*>& val,
                       IoBuffer& output) {
  int ret = 0;
  for (const auto& kv : val) {
    const std::string& key = kv.first;
    const any::Any* val = kv.second;

    uint16_t key_len = key.length();

    ret = output.WriteU16(key_len);

    if (ret != sizeof(uint16_t)) {
      return -1;
    }

    ret = output.Write(key);
    if (ret != (int)key.size()) {
      return -1;
    }

    ret = Encode(val, output);
    if (ret != 0) {
      return -1;
    }
  }

  ret = output.WriteU16(0);
  if (ret != sizeof(uint16_t)) {
    return -1;
  }

  return 0;
}
