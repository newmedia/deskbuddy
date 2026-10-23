#!/usr/bin/env bash
# Regression test on the web emulator: builds env webemu, starts it on a free port with its own, clean
# state (your normal emulator settings are left alone), runs tools/webemu_test.py and stops it again.
#   tools/webemu-test.sh         -> summary; exit code 0 = all passed
#   tools/webemu-test.sh -v      -> also show the passed checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="$ROOT/.venv/bin/python3"

cd "$ROOT/firmware"
"$ROOT/.venv/bin/pio" run -e webemu -s

PORT="$("$PY" -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')"
STATE="$(mktemp -d "${TMPDIR:-/tmp}/webemu-test.XXXXXX")"
LOG="$STATE.log"
WEBEMU_STATE="$STATE" setsid "$ROOT/firmware/.pio/build/webemu/program" "$ROOT" "$PORT" >"$LOG" 2>&1 &
PID=$!
cleanup() {
  kill "$PID" 2>/dev/null || true
  wait "$PID" 2>/dev/null || true
  rm -rf "$STATE"
}
trap cleanup EXIT

set +e
"$PY" "$ROOT/tools/webemu_test.py" "http://127.0.0.1:$PORT" "$@"
RC=$?
set -e
if [ $RC -ne 0 ]; then
  echo "--- emulator log ($LOG) ---"
  tail -n 20 "$LOG"
else
  rm -f "$LOG"
fi
exit $RC
