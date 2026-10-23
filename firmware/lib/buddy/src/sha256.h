#pragma once
// SHA-256 (FIPS 180-4) and HMAC-SHA256 (RFC 2104), small and without dependencies, so the device,
// the simulator and the web emulator sign buddy messages the same way (mesh.h).

#include <stddef.h>
#include <stdint.h>

namespace buddy {

class Sha256 {
 public:
  Sha256();
  void update(const void* data, size_t len);
  void finish(uint8_t out[32]);

 private:
  void block(const uint8_t* p);
  uint32_t h_[8];
  uint8_t buf_[64];
  uint64_t bits_ = 0;
  size_t used_ = 0;
};

void sha256(const void* data, size_t len, uint8_t out[32]);
void hmacSha256(const uint8_t* key, size_t keyLen, const void* msg, size_t msgLen, uint8_t out[32]);

}  // namespace buddy
