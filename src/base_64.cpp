#include <stdint.h>

#include <iostream>
#include <map>

#include "base_64.h"

char kEncodeBase64Dict[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
};

std::map<char, uint8_t> kDecodeBase64Map = {
    {'A', 0},  {'B', 1},  {'C', 2},  {'D', 3},  {'E', 4},  {'F', 5},  {'G', 6},
    {'H', 7},  {'I', 8},  {'J', 9},  {'K', 10}, {'L', 11}, {'M', 12}, {'N', 13},
    {'O', 14}, {'P', 15}, {'Q', 16}, {'R', 17}, {'S', 18}, {'T', 19}, {'U', 20},
    {'V', 21}, {'W', 22}, {'X', 23}, {'Y', 24}, {'Z', 25},

    {'a', 26}, {'b', 27}, {'c', 28}, {'d', 29}, {'e', 30}, {'f', 31}, {'g', 32},
    {'h', 33}, {'i', 34}, {'j', 35}, {'k', 36}, {'l', 37}, {'m', 38}, {'n', 39},
    {'o', 40}, {'p', 41}, {'q', 42}, {'r', 43}, {'s', 44}, {'t', 45}, {'u', 46},
    {'v', 47}, {'w', 48}, {'x', 49}, {'y', 50}, {'z', 51},

    {'0', 52}, {'1', 53}, {'2', 54}, {'3', 55}, {'4', 56}, {'5', 57}, {'6', 58},
    {'7', 59}, {'8', 60}, {'9', 61},

    {'+', 62}, {'/', 63},
};

int Base64::Encode(const uint8_t* input, const uint32_t& len,
                   std::string& output) {
  uint32_t three_times_len = len - len % 3;
  uint32_t left_len = len % 3;

  std::cout << "three_times_len:" << three_times_len
            << ", left_len:" << left_len << std::endl;

  for (uint32_t i = 0; i < three_times_len; i += 3) {
    uint32_t tmp = (input[i] << 16) | (input[i + 1] << 8) | (input[i + 2]);

    uint8_t index_1 = (tmp & 0x00FC0000) >> 18;
    uint8_t index_2 = (tmp & 0x0003F000) >> 12;
    uint8_t index_3 = (tmp & 0x00000FC0) >> 6;
    uint8_t index_4 = (tmp & 0x0000003F);

    output.append(1, kEncodeBase64Dict[index_1]);
    output.append(1, kEncodeBase64Dict[index_2]);
    output.append(1, kEncodeBase64Dict[index_3]);
    output.append(1, kEncodeBase64Dict[index_4]);
  }

  if (left_len == 1) {
    uint8_t index_1 = ((input[len - 1]) & 0xFC) >> 2;
    uint8_t index_2 = ((input[len - 1]) & 0x03) << 4;

    output.append(1, kEncodeBase64Dict[index_1]);
    output.append(1, kEncodeBase64Dict[index_2]);
    output.append(1, '=');
    output.append(1, '=');
  } else if (left_len == 2) {
    uint16_t tmp = (input[len - 2] << 8) | (input[len - 1]);
    uint8_t index_1 = (tmp & 0xFC00) >> 10;
    uint8_t index_2 = (tmp & 0x03F0) >> 4;
    uint8_t index_3 = (tmp & 0x000F) << 2;

    output.append(1, kEncodeBase64Dict[index_1]);
    output.append(1, kEncodeBase64Dict[index_2]);
    output.append(1, kEncodeBase64Dict[index_3]);
    output.append(1, '=');
  }

  return 0;
}

int Base64::Encode(const std::string& input, std::string& output) {
  return Encode((const uint8_t*)input.data(), input.size(), output);
}

int Base64::Decode(const uint8_t* input, const uint32_t& len,
                   std::string& output) {
  uint32_t four_times_len = len - len % 4;
  uint32_t len_left = len % 4;

  for (uint32_t i = 0; i < four_times_len; i += 4) {
    uint32_t tmp = (kDecodeBase64Map[input[i]] << 18) |
                   (kDecodeBase64Map[input[i + 1]] << 12) |
                   (kDecodeBase64Map[input[i + 2]] << 6) |
                   (kDecodeBase64Map[input[i + 3]]);

    if (i != four_times_len - 4) {
      uint8_t byte_1 = (tmp & 0x00FF0000) >> 16;
      uint8_t byte_2 = (tmp & 0x0000FF00) >> 8;
      uint8_t byte_3 = (tmp & 0x000000FF);

      output.append(1, byte_1);
      output.append(1, byte_2);
      output.append(1, byte_3);
    } else {
      if (input[i + 2] == '=' && input[i + 3] == '=') {
        uint8_t byte_1 = (tmp & 0x00FF0000) >> 16;

        output.append(1, byte_1);
      } else if (input[i + 3] == '=') {
        uint8_t byte_1 = (tmp & 0x00FF0000) >> 16;
        uint8_t byte_2 = (tmp & 0x0000FF00) >> 8;

        output.append(1, byte_1);
        output.append(1, byte_2);
      } else {
        uint8_t byte_1 = (tmp & 0x00FF0000) >> 16;
        uint8_t byte_2 = (tmp & 0x0000FF00) >> 8;
        uint8_t byte_3 = (tmp & 0x000000FF);

        output.append(1, byte_1);
        output.append(1, byte_2);
        output.append(1, byte_3);
      }
    }
  }

  if (len_left == 1) {
  } else if (len_left == 2) {
  } else if (len_left == 3) {
  }

  return 0;
}

int Base64::Decode(const std::string& input, std::string& output) {
  return Decode((const uint8_t*)input.data(), input.size(), output);
}
