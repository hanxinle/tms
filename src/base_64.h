#ifndef __BASE_64_H__
#define __BASE_64_H__

#include <string>

class Base64 {
 public:
  Base64();
  ~Base64();

  static int Encode(const uint8_t* input, const uint32_t& len,
                    std::string& output);
  static int Encode(const std::string& input, std::string& output);
  static int Decode(const uint8_t* input, const uint32_t& len,
                    std::string& output);
  static int Decode(const std::string& input, std::string& output);

 private:
};

#endif  // __BASE_64_H__
