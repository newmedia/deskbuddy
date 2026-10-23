#pragma once
// Buddies in the same room (ESP-NOW on the device, UDP in the web emulator; see docs/multi-buddy.md):
// groups, signed hello/event messages, the list of buddies nearby and of groups nearby. The transport
// belongs to the platform; this class only builds and checks the frames.
//
// A group is a random 128-bit key plus a name. The invite "<key as 32 hex>.<name>" is all a buddy
// needs to join; it never goes over the air. Frames of the group are small JSON, signed with
// HMAC-SHA256 over the key, truncated to 8 bytes:
//   {"v":1,"t":"hello","from":"deskbuddy-7840","b":<boot id>,"n":<seq>,"look":"Robot","h":"<16 hex>"}
// A visible group also sends an unsigned beacon with its name and public id (derived from the key),
// so buddies that scan can list it ("groups nearby"):
//   {"v":1,"t":"group","from":"deskbuddy-7840","id":"a1b2c3d4","name":"Team Dev"}
// Without a group nothing is sent and signed frames are ignored; beacons are only heard while scanning.

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

// A group heard while scanning: name, public id and the buddies that announced it
struct MeshGroup {
  static constexpr int kMaxMembers = 8;
  char id[9] = "";
  char name[21] = "";
  uint32_t member[kMaxMembers] = {0};   // hash of the sender's name
  uint32_t memberSeen[kMaxMembers] = {0};
  uint32_t seenMs = 0;
  bool used = false;
};

class Mesh {
 public:
  static constexpr int kMaxPeers = 8, kMaxGroups = 8;
  static constexpr uint32_t kHelloMs = 3000;    // hello interval
  static constexpr uint32_t kBeaconMs = 5000;   // name of a visible group
  static constexpr uint32_t kExpireMs = 10000;  // gone after 3 missed hellos
  static constexpr uint32_t kGroupExpireMs = 15000;
  static constexpr size_t kMaxFrame = 250;      // ESP-NOW v1 size: little airtime, always delivered whole
  static constexpr size_t kKeyLen = 16, kMaxName = 20;
  static constexpr size_t kInviteLen = kKeyLen * 2 + 1 + kMaxName;  // "<32 hex>.<name>"

  void begin(const char* name, uint32_t bootId);
  const char* name() const { return name_; }

  // Group from an invite; "" = leave the group. False for an invalid invite (nothing changes).
  bool join(const char* invite);
  // New group with a random key (from 4 random words) and a name (1-20 characters).
  bool create(const uint32_t rnd[4], const char* name);
  void invite(char* out, size_t cap) const;  // "<32 hex>.<name>", "" without a group
  bool enabled() const { return inGroup_; }
  const char* groupName() const { return groupName_; }
  const char* groupId() const { return groupId_; }
  void setVisible(bool v) { visible_ = v; }
  bool visible() const { return visible_; }

  // Scanning: hear the beacons of visible groups for a while (also without a group of our own)
  void listen(uint32_t nowMs, uint32_t forMs) { listenUntil_ = nowMs + forMs; }
  bool scanning(uint32_t nowMs) const { return (int32_t)(listenUntil_ - nowMs) > 0; }
  bool radioNeeded(uint32_t nowMs) const { return inGroup_ || scanning(nowMs); }  // transport on?

  // Every frame: a hello or (visible group) a beacon when one is due. Returns its length (0 = nothing).
  size_t tick(uint32_t nowMs, const char* look, char* out, size_t cap);
  // A signed event ("yawn", ...). Returns its length (0 = no group).
  size_t event(const char* what, char* out, size_t cap);
  // Incoming frame. Signed frames of our group update the buddies list (for an event `what` gets its
  // name, otherwise ""); beacons update the groups list while scanning. False for frames we ignore.
  bool receive(const uint8_t* data, size_t len, int rssi, uint32_t nowMs, char* what, size_t whatCap);

  int count(uint32_t nowMs) const;
  // {"group":..,"name":..,"id":..,"visible":..,"me":..,"tx":..,"rx":..,"bad":..,"buddies":[...]}
  void json(uint32_t nowMs, char* out, size_t len) const;
  // {"scanning":..,"groups":[{"id":..,"name":..,"buddies":N,"mine":..}]}
  void groupsJson(uint32_t nowMs, char* out, size_t len) const;

 private:
  size_t sign(char* out, size_t cap, int bodyLen);
  void mac(const char* body, size_t len, char hex[17]) const;
  void setKey(const uint8_t key[kKeyLen], const char* name);
  bool beaconIn(const char* buf, uint32_t nowMs);

  char name_[24] = "deskbuddy";
  bool inGroup_ = false, visible_ = true;
  uint8_t key_[kKeyLen] = {0};
  char groupName_[kMaxName + 1] = "";
  char groupId_[9] = "";
  uint32_t boot_ = 0, seq_ = 0, nextHello_ = 0, nextBeacon_ = 0, listenUntil_ = 0;
  uint32_t tx_ = 0, rx_ = 0, bad_ = 0;  // sent, accepted, rejected (other group, tampered, replay)
  MeshPeer peers_[kMaxPeers];
  MeshGroup groups_[kMaxGroups];
};

}  // namespace buddy
