#ifndef __CRC32_H__
#define __CRC32_H__

#include <iostream>

#include "common_define.h"

enum CRC32Type {
  CRC32_HLS = 0,
  CRC32_STUN = 1,
  CRC32_SCTP = 2,
};

class CRC32 {
 public:
  CRC32(const int& type);
  uint32_t GetCrc32(const uint8_t* data, int len);

 private:
  void MakeHlsTable();
  void MakeSctpTable();
  void MakeStunTable();

 private:
  int type_;

  static uint32_t crc32_stun_table_[256];
  static uint32_t crc32_hls_table_[256];
  static uint32_t crc32_sctp_table_[256];
};

#endif  // __CRC32_H__
