#!/usr/bin/env bash
# Regression test on the web emulator: for each board builds env webemu (Original) and/or webemu-xl (XL),
# starts it on a free port with its own, clean state (your normal emulator settings are left alone), runs
# tools/webemu_test.py and stops it again.
#   tools/webemu-test.sh                  -> both boards; exit code 0 = all passed
#   tools/webemu-test.sh --board xl       -> only the XL (or: original)
#   tools/webemu-test.sh -v               -> also show the passed checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="$ROOT/.venv/bin/python3"

BOARDS="original xl"
ARGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --board) BOARDS="${2:?--board needs original, xl or all}"; [ "$BOARDS" = all ] && BOARDS="original xl"; shift 2 ;;
    *) ARGS+=("$1"); shift ;;
  esac
done

PID=""
STATE=""
cleanup() {
  [ -n "$PID" ] && { kill "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true; }
  [ -n "$STATE" ] && rm -rf "$STATE"
  PID=""; STATE=""
}
trap cleanup EXIT

RC=0
for BOARD in $BOARDS; do
  case "$BOARD" in
    original) ENV=webemu ;;
    xl) ENV=webemu-xl ;;
    *) echo "Unknown board: $BOARD (original, xl or all)" >&2; exit 2 ;;
  esac
  echo "== $BOARD ($ENV)"
  (cd "$ROOT/firmware" && "$ROOT/.venv/bin/pio" run -e "$ENV" -s)
  PORT="$("$PY" -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')"
  STATE="$(mktemp -d "${TMPDIR:-/tmp}/webemu-test.XXXXXX")"
  LOG="$STATE.log"
  WEBEMU_STATE="$STATE" setsid "$ROOT/firmware/.pio/build/$ENV/program" "$ROOT" "$PORT" >"$LOG" 2>&1 &
  PID=$!

  set +e
  "$PY" "$ROOT/tools/webemu_test.py" "http://127.0.0.1:$PORT" --board "$BOARD" ${ARGS[@]+"${ARGS[@]}"}
  R=$?
  set -e
  if [ $R -ne 0 ]; then
    RC=1
    echo "--- emulator log ($LOG) ---"
    tail -n 20 "$LOG"
  else
    rm -f "$LOG"
  fi
  cleanup
  echo
done
exit $RC
