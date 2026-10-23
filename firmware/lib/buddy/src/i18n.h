#pragma once
// Language of the on-screen text: English (default) or Dutch. Every user-facing string goes through
// tr("English", "Nederlands"); serial/API replies and logs are English only.

#include <stdint.h>
#include <string.h>

namespace buddy {

enum class Lang : uint8_t { En = 0, Nl = 1 };

inline Lang gLang = Lang::En;  // set by App from the settings

inline const char* tr(const char* en, const char* nl) { return gLang == Lang::Nl ? nl : en; }

inline const char* langName(Lang l) { return l == Lang::Nl ? "nl" : "en"; }

inline bool langFromName(const char* n, Lang& out) {
  if (!strcmp(n, "en")) out = Lang::En;
  else if (!strcmp(n, "nl")) out = Lang::Nl;
  else return false;
  return true;
}

}  // namespace buddy
