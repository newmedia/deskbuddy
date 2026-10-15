# PlatformIO pre-script: comprimeert web/index.html (gzip) naar src/net/web_index.h,
# zodat de webinterface in de firmware zit en zonder bestandssysteem werkt.
import gzip
import os

Import("env")  # noqa: F821 (PlatformIO)

root = env.subst("$PROJECT_DIR")  # noqa: F821
src = os.path.join(root, "web", "index.html")
dst = os.path.join(root, "src", "net", "web_index.h")

data = gzip.compress(open(src, "rb").read(), compresslevel=9, mtime=0)
lines = [", ".join(f"0x{b:02x}" for b in data[i : i + 20]) for i in range(0, len(data), 20)]
out = (
    "// GEGENEREERD door scripts/embed_web.py uit web/index.html - niet met de hand aanpassen\n"
    "#pragma once\n#include <stddef.h>\n#include <stdint.h>\n\n"
    f"static const uint8_t kIndexGz[] = {{\n  " + ",\n  ".join(lines) + "\n};\n"
    f"static const size_t kIndexGzLen = {len(data)};\n"
)
if not os.path.exists(dst) or open(dst).read() != out:
    open(dst, "w").write(out)
    print(f"embed_web: {len(data)} bytes gzip -> src/net/web_index.h")
