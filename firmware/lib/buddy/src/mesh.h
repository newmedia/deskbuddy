#pragma once
// Buddies in the same room (ESP-NOW on the device, UDP in the web emulator; see docs/multi-buddy.md):
// signed hello/event messages and the list of buddies nearby. The transport belongs to the platform;
// this class only builds and checks the frames.
//
// Frame: small JSON, signed with HMAC-SHA256 over the group key (SHA-256 of the group code), truncated
// to 8 bytes: {"v":1,"t":"hello","from":"deskbuddy-7840","b":<boot id>,"n":<seq>,"look":"Robot","h":"<16 hex>"}
// Without a group code everything is off: no sending, everything received is ignored.

#include <stddef.h>
#include <stdint.h>

namespace buddy {

struct MeshPeer {
  char name[24] = "";
  char look[16] = "";
  uint32_t boot = 0;    // boot id of the sender: a new id = restarted, sequence starts again
  uint32_t seq = 0;     // highest sequence number seen (against replays and duplicates)
  uint32_t seenMs = 0;  // last hello or event
  int rssi = 0;
  bool used = false;
};

class Mesh {
 public:
  static constexpr int kMaxPeers = 8;
  static constexpr uint32_t kHelloMs = 3000;   // hello interval
  static constexpr uint32_t kExpireMs = 10000; // gone after 3 missed hellos
  static constexpr size_t kMaxFrame = 250;     // ESP-NOW v1 size: little airtime, always delivered whole
  static constexpr size_t kMinCode = 3, kMaxCode = 16;

  void begin(const char* name, uint32_t bootId);
  bool setGroup(const char* code);  // "" = off; false when too short or too long
  bool enabled() const { return code_[0] != 0; }
  const char* name() const { return name_; }

  // Every frame: writes a hello into out when one is due. Returns its length (0 = nothing to send).
  size_t tick(uint32_t nowMs, const char* look, char* out, size_t cap);
  // A signed event ("yawn", ...). Returns its length (0 = mesh off).
  size_t event(const char* what, char* out, size_t cap);
  // Incoming frame. Verifies the signature and the sequence, updates the list; for an event `what`
  // gets its name (otherwise ""). Returns false for frames that are not valid for our group.
  bool receive(const uint8_t* data, size_t len, int rssi, uint32_t nowMs, char* what, size_t whatCap);

  int count(uint32_t nowMs) const;
  // {"group":..,"me":..,"tx":..,"rx":..,"bad":..,"buddies":[...]}; tx/rx/bad count frames since the start
  void json(uint32_t nowMs, char* out, size_t len) const;

 private:
  size_t sign(char* out, size_t cap, int bodyLen);
  void mac(const char* body, size_t len, char hex[17]) const;

  char name_[24] = "deskbuddy";
  char code_[kMaxCode + 1] = "";
  uint8_t key_[32] = {0};
  uint32_t boot_ = 0, seq_ = 0, nextHello_ = 0;
  uint32_t tx_ = 0, rx_ = 0, bad_ = 0;  // sent, accepted, rejected (other group, tampered, replay)
  MeshPeer peers_[kMaxPeers];
};

}  // namespace buddy
