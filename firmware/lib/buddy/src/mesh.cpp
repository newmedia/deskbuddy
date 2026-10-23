#include "mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sha256.h"

namespace buddy {

namespace {
// Value of "key":"..." or "key":123 in a flat JSON object (our own frames: no nesting, no escapes).
const char* find(const char* js, const char* key) {
  char pat[24];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(js, pat);
  return p ? p + strlen(pat) : nullptr;
}
bool getStr(const char* js, const char* key, char* out, size_t cap) {
  const char* p = find(js, key);
  if (!p || *p != '"') return false;
  p++;
  size_t n = 0;
  while (*p && *p != '"' && n + 1 < cap) out[n++] = *p++;
  out[n] = 0;
  return *p == '"';
}
bool getU32(const char* js, const char* key, uint32_t& out) {
  const char* p = find(js, key);
  if (!p || *p < '0' || *p > '9') return false;
  out = (uint32_t)strtoul(p, nullptr, 10);
  return true;
}
// Names go into JSON strings and invites: no quotes, backslashes, control characters or dots
void clean(char* s, bool dots = true) {
  for (; *s; s++)
    if (*s == '"' || *s == '\\' || (unsigned char)*s < 0x20 || (!dots && *s == '.')) *s = '\'';
}
int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
  return h ? h : 1;
}
}  // namespace

void Mesh::begin(const char* name, uint32_t bootId) {
  snprintf(name_, sizeof(name_), "%s", name);
  clean(name_);
  boot_ = bootId;
}

void Mesh::setKey(const uint8_t key[kKeyLen], const char* name) {
  memcpy(key_, key, kKeyLen);
  snprintf(groupName_, sizeof(groupName_), "%s", name);
  clean(groupName_);
  // Public group id: the first 4 bytes of a hash of the key (says nothing about the key itself)
  uint8_t h[32];
  Sha256 s;
  s.update("deskbuddy-group-id:", 19);
  s.update(key_, kKeyLen);
  s.finish(h);
  for (int i = 0; i < 4; i++) snprintf(groupId_ + i * 2, 3, "%02x", h[i]);
  inGroup_ = true;
  for (MeshPeer& p : peers_) p = MeshPeer();  // other group, other buddies
  nextHello_ = nextBeacon_ = 0;
}

bool Mesh::join(const char* invite) {
  if (!*invite) {  // leave
    inGroup_ = false;
    memset(key_, 0, sizeof(key_));
    groupName_[0] = groupId_[0] = 0;
    for (MeshPeer& p : peers_) p = MeshPeer();
    return true;
  }
  uint8_t key[kKeyLen];
  for (size_t i = 0; i < kKeyLen; i++) {
    int hi = hexVal(invite[i * 2]), lo = hi < 0 ? -1 : hexVal(invite[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    key[i] = (uint8_t)(hi << 4 | lo);
  }
  const char* name = invite + kKeyLen * 2;
  if (*name != '.' || !name[1] || strlen(name + 1) > kMaxName) return false;
  setKey(key, name + 1);
  return true;
}

bool Mesh::create(const uint32_t rnd[4], const char* name) {
  size_t n = strlen(name);
  if (!n || n > kMaxName) return false;
  uint8_t key[kKeyLen];
  for (size_t i = 0; i < kKeyLen; i++) key[i] = (uint8_t)(rnd[i / 4] >> (8 * (i % 4)));
  char nm[kMaxName + 1];
  snprintf(nm, sizeof(nm), "%s", name);
  clean(nm, false);
  setKey(key, nm);
  return true;
}

void Mesh::invite(char* out, size_t cap) const {
  if (!cap) return;
  out[0] = 0;
  if (!inGroup_ || cap < kInviteLen + 1) return;
  for (size_t i = 0; i < kKeyLen; i++) snprintf(out + i * 2, 3, "%02x", key_[i]);
  snprintf(out + kKeyLen * 2, cap - kKeyLen * 2, ".%s", groupName_);
}

void Mesh::mac(const char* body, size_t len, char hex[17]) const {
  uint8_t h[32];
  hmacSha256(key_, sizeof(key_), body, len, h);
  for (int i = 0; i < 8; i++) snprintf(hex + i * 2, 3, "%02x", h[i]);
}

size_t Mesh::sign(char* out, size_t cap, int bodyLen) {
  if (bodyLen <= 0 || (size_t)bodyLen + 24 > cap || (size_t)bodyLen + 24 > kMaxFrame) return 0;
  tx_++;
  char hex[17];
  mac(out, (size_t)bodyLen, hex);
  return (size_t)bodyLen + (size_t)snprintf(out + bodyLen, cap - (size_t)bodyLen, ",\"h\":\"%s\"}", hex);
}

size_t Mesh::tick(uint32_t nowMs, const char* look, char* out, size_t cap) {
  if (!inGroup_) return 0;
  if ((int32_t)(nowMs - nextHello_) >= 0) {
    nextHello_ = nowMs + kHelloMs;
    char lk[16];
    snprintf(lk, sizeof(lk), "%s", look);
    clean(lk);
    int n = snprintf(out, cap, "{\"v\":1,\"t\":\"hello\",\"from\":\"%s\",\"b\":%lu,\"n\":%lu,\"look\":\"%s\"", name_,
                     (unsigned long)boot_, (unsigned long)++seq_, lk);
    return sign(out, cap, n);
  }
  if (visible_ && (int32_t)(nowMs - nextBeacon_) >= 0) {  // name of the group, for buddies that scan
    nextBeacon_ = nowMs + kBeaconMs;
    int n = snprintf(out, cap, "{\"v\":1,\"t\":\"group\",\"from\":\"%s\",\"id\":\"%s\",\"name\":\"%s\"}", name_, groupId_,
                     groupName_);
    if (n > 0 && (size_t)n < cap) {
      tx_++;
      return (size_t)n;
    }
  }
  return 0;
}

size_t Mesh::event(const char* what, char* out, size_t cap) {
  if (!inGroup_) return 0;
  int n = snprintf(out, cap, "{\"v\":1,\"t\":\"event\",\"from\":\"%s\",\"b\":%lu,\"n\":%lu,\"what\":\"%s\"", name_,
                   (unsigned long)boot_, (unsigned long)++seq_, what);
  return sign(out, cap, n);
}

// Unsigned beacon of a visible group: remember name, id and which buddies announce it
bool Mesh::beaconIn(const char* buf, uint32_t nowMs) {
  char id[9], name[kMaxName + 1], from[24];
  if (!getStr(buf, "id", id, sizeof(id)) || strlen(id) != 8 || !getStr(buf, "name", name, sizeof(name)) || !name[0] ||
      !getStr(buf, "from", from, sizeof(from)) || !strcmp(from, name_))
    return false;
  MeshGroup* g = nullptr;
  MeshGroup* slot = nullptr;
  for (MeshGroup& q : groups_) {
    if (q.used && !strcmp(q.id, id)) g = &q;
    if (!q.used) {
      if (!slot || slot->used) slot = &q;
    } else if (!slot || (slot->used && q.seenMs < slot->seenMs)) {
      slot = &q;
    }
  }
  if (!g) {
    g = slot;
    *g = MeshGroup();
    g->used = true;
    snprintf(g->id, sizeof(g->id), "%s", id);
  }
  snprintf(g->name, sizeof(g->name), "%s", name);  // anyone could send a beacon: it is only a label
  g->seenMs = nowMs;
  uint32_t h = fnv(from);
  int m = 0;
  for (int i = 0; i < MeshGroup::kMaxMembers; i++) {
    if (g->member[i] == h) {
      m = i;
      break;
    }
    if (nowMs - g->memberSeen[i] > nowMs - g->memberSeen[m] || !g->member[i]) m = i;  // free or oldest
  }
  g->member[m] = h;
  g->memberSeen[m] = nowMs;
  return true;
}

bool Mesh::receive(const uint8_t* data, size_t len, int rssi, uint32_t nowMs, char* what, size_t whatCap) {
  if (whatCap) what[0] = 0;
  if (!radioNeeded(nowMs)) return false;
  if (len < 24 || len > kMaxFrame) {
    bad_++;
    return false;
  }
  char buf[kMaxFrame + 1];
  memcpy(buf, data, len);
  buf[len] = 0;
  char type[8];
  uint32_t v = 0;
  if (!getU32(buf, "v", v) || v != 1 || !getStr(buf, "t", type, sizeof(type))) {
    bad_++;
    return false;
  }
  if (!strcmp(type, "group")) return scanning(nowMs) && beaconIn(buf, nowMs);
  if (!inGroup_) return false;

  // Signature: the frame ends in ,"h":"<16 hex>"} and the HMAC covers everything before it
  const char* tail = buf + len - 24;
  char hex[17];
  if (strncmp(tail, ",\"h\":\"", 6) || strcmp(tail + 22, "\"}")) {
    bad_++;
    return false;
  }
  mac(buf, (size_t)(tail - buf), hex);
  if (strncmp(tail + 6, hex, 16)) {  // other group or tampered with
    bad_++;
    return false;
  }
  uint32_t boot = 0, seq = 0;
  char from[24];
  if (!getStr(buf, "from", from, sizeof(from)) || !getU32(buf, "b", boot) || !getU32(buf, "n", seq) ||
      !strcmp(from, name_)) {
    bad_++;
    return false;
  }

  // Find the sender; otherwise a free place, or else the buddy not seen for the longest time
  MeshPeer* p = nullptr;
  MeshPeer* slot = nullptr;
  for (MeshPeer& q : peers_) {
    if (q.used && !strcmp(q.name, from)) p = &q;
    if (!q.used) {
      if (!slot || slot->used) slot = &q;
    } else if (!slot || (slot->used && q.seenMs < slot->seenMs)) {
      slot = &q;
    }
  }
  if (p && p->boot == boot && seq <= p->seq) {  // replay or duplicate
    bad_++;
    return false;
  }
  rx_++;
  if (!p) {
    p = slot;
    *p = MeshPeer();
    p->used = true;
    snprintf(p->name, sizeof(p->name), "%s", from);
  }
  p->boot = boot;
  p->seq = seq;
  p->seenMs = nowMs;
  p->rssi = rssi;
  if (!strcmp(type, "hello")) {
    getStr(buf, "look", p->look, sizeof(p->look));
  } else if (!strcmp(type, "event") && whatCap) {
    getStr(buf, "what", what, whatCap);
  }
  return true;
}

int Mesh::count(uint32_t nowMs) const {
  int n = 0;
  for (const MeshPeer& p : peers_) n += p.used && nowMs - p.seenMs < kExpireMs;
  return n;
}

void Mesh::json(uint32_t nowMs, char* out, size_t len) const {
  int n = snprintf(out, len,
                   "{\"group\":%s,\"name\":\"%s\",\"id\":\"%s\",\"visible\":%s,\"me\":\"%s\",\"tx\":%lu,\"rx\":%lu,"
                   "\"bad\":%lu,\"buddies\":[",
                   inGroup_ ? "true" : "false", groupName_, groupId_, visible_ ? "true" : "false", name_,
                   (unsigned long)tx_, (unsigned long)rx_, (unsigned long)bad_);
  bool first = true;
  for (const MeshPeer& p : peers_) {
    if (!p.used || nowMs - p.seenMs >= kExpireMs || n < 0 || (size_t)n >= len) continue;
    n += snprintf(out + n, len - (size_t)n, "%s{\"name\":\"%s\",\"look\":\"%s\",\"age\":%lu,\"rssi\":%d}", first ? "" : ",",
                  p.name, p.look, (unsigned long)((nowMs - p.seenMs) / 1000), p.rssi);
    first = false;
  }
  if (n >= 0 && (size_t)n < len) snprintf(out + n, len - (size_t)n, "]}");
}

void Mesh::groupsJson(uint32_t nowMs, char* out, size_t len) const {
  int n = snprintf(out, len, "{\"scanning\":%s,\"groups\":[", scanning(nowMs) ? "true" : "false");
  bool first = true;
  for (const MeshGroup& g : groups_) {
    if (!g.used || nowMs - g.seenMs >= kGroupExpireMs || n < 0 || (size_t)n >= len) continue;
    int members = 0;
    for (int i = 0; i < MeshGroup::kMaxMembers; i++)
      members += g.member[i] && nowMs - g.memberSeen[i] < kGroupExpireMs;
    n += snprintf(out + n, len - (size_t)n, "%s{\"id\":\"%s\",\"name\":\"%s\",\"buddies\":%d,\"mine\":%s}", first ? "" : ",",
                  g.id, g.name, members, inGroup_ && !strcmp(g.id, groupId_) ? "true" : "false");
    first = false;
  }
  if (n >= 0 && (size_t)n < len) snprintf(out + n, len - (size_t)n, "]}");
}

}  // namespace buddy
