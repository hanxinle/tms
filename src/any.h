#ifndef __ANY_H__
#define __ANY_H__

#include <map>
#include <string>
#include <vector>

namespace any {

const uint8_t kInt = 0;
const uint8_t kDouble = 1;
const uint8_t kString = 2;
const uint8_t kVector = 3;
const uint8_t kMap = 4;
const uint8_t kNull = 5;

static std::string AnyTypeToStr(const uint8_t& type) {
  switch (type) {
    case kInt:
      return "kInt";
      break;
    case kDouble:
      return "kDouble";
      break;
    case kString:
      return "kString";
      break;
    case kVector:
      return "kVector";
      break;
    case kMap:
      return "kMap";
      break;
    case kNull:
      return "kNull";
      break;
    default:
      return "kUnknown";
      break;
  }
}

class Int;
class Double;
class String;
class Vector;
class Map;

class Any {
 public:
  explicit Any(const uint8_t& type) : type_(type) {}

  virtual ~Any() {}

  std::string TypeStr() { return AnyTypeToStr(type_); }

  uint8_t GetType() const { return type_; }

  bool IsInt() const { return type_ == kInt; }

  bool IsDouble() const { return type_ == kDouble; }

  bool IsString() const { return type_ == kString; }

  bool IsVector() const { return type_ == kVector; }

  bool IsMap() const { return type_ == kMap; }

  bool IsNull() const { return type_ == kNull; }

  explicit operator Int*();
  explicit operator Double*();
  explicit operator String*();
  explicit operator Vector*();
  explicit operator Map*();

  Int& ToInt() const;
  Double& ToDouble() const;
  String& ToString() const;
  Vector& ToVector() const;
  Map& ToMap() const;

  bool GetInt(int64_t& val);
  bool GetDouble(double& val);
  bool GetString(std::string& val);
  bool GetVector(std::vector<Any*>& val);
  bool GetMap(std::map<std::string, Any*>& val);

  Any* operator[](const size_t& index);
  Any* operator[](const std::string& key);

 private:
  uint8_t type_;
};

class Int : public Any {
  friend class Any;

 public:
  Int(const int64_t i) : Any(kInt), val_(i) {}

  int64_t GetVal() { return val_; }

 private:
  int64_t val_;
};

class Double : public Any {
  friend class Any;

 public:
  Double(const double& d) : Any(kDouble), val_(d) {}

  double GetVal() { return val_; }

 private:
  double val_;
};

class String : public Any {
  friend class Any;

 public:
  String(const std::string& str) : Any(kString), val_(str) {}

  std::string GetVal() { return val_; }

 private:
  std::string val_;
};

class Vector : public Any {
  friend class Any;

 public:
  Vector(const std::vector<Any*>& v, const bool& delete_when_destruct = false)
      : Any(kVector), val_(v), delete_when_destruct_(delete_when_destruct) {}

  ~Vector() {
    if (delete_when_destruct_) {
      for (auto& any : val_) {
        delete any;
      }
    }
  }

  Any* operator[](const size_t& index) {
    if (val_.empty() || val_.size() < index + 1) {
      return NULL;
    }

    return val_[index];
  }

  std::vector<Any*> GetVal() { return val_; }

 private:
  std::vector<Any*> val_;
  bool delete_when_destruct_;
};

class Map : public Any {
  friend class Any;

 public:
  Map(const std::map<std::string, Any*>& m,
      const bool& delete_when_destruct = false)
      : Any(kMap), val_(m), delete_when_destruct_(delete_when_destruct) {}

  Map(const bool& delete_when_destruct = false)
      : Any(kMap), delete_when_destruct_(delete_when_destruct) {}

  ~Map() {
    if (delete_when_destruct_) {
      for (auto& kv : val_) {
        delete kv.second;
      }
    }
  }

  Any* operator[](const std::string& key) {
    if (!val_.count(key)) {
      return NULL;
    }

    return val_[key];
  }

  bool Insert(const std::string& key, Any* val) {
    return (val_.insert(make_pair(key, val))).second;
  }

  std::map<std::string, Any*> GetVal() { return val_; }

 private:
  std::map<std::string, Any*> val_;
  bool delete_when_destruct_;
};

class Null : public Any {
  friend class Any;

 public:
  Null() : Any(kNull) {}
};

}  // namespace any

#endif  // __ANY_H__
