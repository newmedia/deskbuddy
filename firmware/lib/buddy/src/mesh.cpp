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
// Names and looks go into JSON strings: no quotes or backslashes
void clean(char* s) {
  for (; *s; s++)
    if (*s == '"' || *s == '\\' || (unsigned char)*s < 0x20) *s = '\'';
}
}  // namespace

void Mesh::begin(const char* name, uint32_t bootId) {
  snprintf(name_, sizeof(name_), "%s", name);
  clean(name_);
  boot_ = bootId;
}

bool Mesh::setGroup(const char* code) {
  size_t n = strlen(code);
  if (n && (n < kMinCode || n > kMaxCode)) return false;
  snprintf(code_, sizeof(code_), "%s", code);
  char seed[48];
  int len = snprintf(seed, sizeof(seed), "deskbuddy-group:%s", code_);
  sha256(seed, (size_t)len, key_);
  for (MeshPeer& p : peers_) p = MeshPeer();  // other group, other buddies
  nextHello_ = 0;
  return true;
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
  if (!enabled() || (int32_t)(nowMs - nextHello_) < 0) return 0;
  nextHello_ = nowMs + kHelloMs;
  char lk[16];
  snprintf(lk, sizeof(lk), "%s", look);
  clean(lk);
  int n = snprintf(out, cap, "{\"v\":1,\"t\":\"hello\",\"from\":\"%s\",\"b\":%lu,\"n\":%lu,\"look\":\"%s\"", name_,
                   (unsigned long)boot_, (unsigned long)++seq_, lk);
  return sign(out, cap, n);
}

size_t Mesh::event(const char* what, char* out, size_t cap) {
  if (!enabled()) return 0;
  int n = snprintf(out, cap, "{\"v\":1,\"t\":\"event\",\"from\":\"%s\",\"b\":%lu,\"n\":%lu,\"what\":\"%s\"", name_,
                   (unsigned long)boot_, (unsigned long)++seq_, what);
  return sign(out, cap, n);
}

bool Mesh::receive(const uint8_t* data, size_t len, int rssi, uint32_t nowMs, char* what, size_t whatCap) {
  if (whatCap) what[0] = 0;
  if (!enabled()) return false;
  if (len < 24 || len > kMaxFrame) {
    bad_++;
    return false;
  }
  char buf[kMaxFrame + 1];
  memcpy(buf, data, len);
  buf[len] = 0;
  // Signature: the frame ends in ,"h":"<16 hex>"} and the HMAC covers everything before it
  const char* tail = len >= 24 ? buf + len - 24 : nullptr;
  char hex[17];
  if (!tail || strncmp(tail, ",\"h\":\"", 6) || strcmp(tail + 22, "\"}")) {
    bad_++;
    return false;
  }
  mac(buf, (size_t)(tail - buf), hex);
  if (strncmp(tail + 6, hex, 16)) {  // other group or tampered with
    bad_++;
    return false;
  }

  uint32_t v = 0, boot = 0, seq = 0;
  char type[8], from[24];
  if (!getU32(buf, "v", v) || v != 1 || !getStr(buf, "t", type, sizeof(type)) || !getStr(buf, "from", from, sizeof(from)) ||
      !getU32(buf, "b", boot) || !getU32(buf, "n", seq) || !strcmp(from, name_)) {
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
  int n = snprintf(out, len, "{\"group\":%s,\"me\":\"%s\",\"tx\":%lu,\"rx\":%lu,\"bad\":%lu,\"buddies\":[",
                   enabled() ? "true" : "false", name_, (unsigned long)tx_, (unsigned long)rx_, (unsigned long)bad_);
  bool first = true;
  for (const MeshPeer& p : peers_) {
    if (!p.used || nowMs - p.seenMs >= kExpireMs || n < 0 || (size_t)n >= len) continue;
    n += snprintf(out + n, len - (size_t)n, "%s{\"name\":\"%s\",\"look\":\"%s\",\"age\":%lu,\"rssi\":%d}", first ? "" : ",",
                  p.name, p.look, (unsigned long)((nowMs - p.seenMs) / 1000), p.rssi);
    first = false;
  }
  if (n >= 0 && (size_t)n < len) snprintf(out + n, len - (size_t)n, "]}");
}

}  // namespace buddy
