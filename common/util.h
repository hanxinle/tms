#ifndef __UTIL_H__
#define __UTIL_H__

#include <map>
#include <sstream>
#include <string>
#include <vector>

class Util {
 public:
  static void Daemon();
  static std::map<std::string, std::string> ParseArgs(int argc, char* argv[]);
  static std::string Bin2Hex(const uint8_t* buf, const size_t& len,
                             const size_t& char_per_line = 32,
                             const bool& printf_ascii = true,
                             const std::string& prefix = "");
  static std::string Bin2Hex(const std::string& str,
                             const size_t& char_per_line = 32,
                             const bool& printf_ascii = true,
                             const std::string& prefix = "");

  static uint64_t GetNowMs();
  static uint64_t GetNow();
  static uint64_t GetNowUs();
  static std::string GetNowStr();
  static std::string GetNowUTCStr();
  static std::string SecondToUTCStr(const uint64_t& second);
  static std::string GetNowStrHttpFormat();  // RFC 2822
  static std::string GetNowMsStr();

  static std::string ReadFile(const std::string& file_name);

  template <typename T>
  static std::string Num2Str(const T& t) {
    std::ostringstream os;
    os << t;

    return os.str();
  }

  template <typename T>
  static T Str2Num(const std::string& str) {
    T ret;
    std::istringstream is(str);

    is >> ret;

    return ret;
  }

  static std::vector<std::string> SepStr(const std::string& input,
                                         const std::string& sep);
  static void Replace(std::string& input, const std::string& from,
                      const std::string& to);
  static std::string GenRandom(const size_t& len);
  static std::string GenRandomNum(const size_t& len);
};

#endif  // __UTIL_H__
