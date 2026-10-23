// Faked Web Bluetooth for the web emulator. The PWA code (ble-transport in index.html) stays
// unchanged: it talks to this object as if it were a real deskbuddy with the Nordic UART Service.
// Replies arrive in chunks of 20 bytes (like with the default MTU) and end with "\n".
(() => {
  const NUS = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  const post = (a, body = "", q = "") => fetch(`/emu/ble?a=${a}${q}`, {method: "POST", body});
  // ?auto: no prompts; the pairing code is read from the (faked) screen. Handy for tests.
  const AUTO = new URLSearchParams(location.search).has("auto");

  function makeDevice() {
    const listeners = {};
    const device = {
      name: "deskbuddy-A45C (emulator)",
      addEventListener(t, f) { (listeners[t] ||= []).push(f); },
      gatt: {
        connected: false,
        async connect() { device.gatt.connected = true; return server; },
        disconnect() {
          device.gatt.connected = false;
          post("disconnect");
          (listeners.gattserverdisconnected || []).forEach(f => f({target: device}));
        },
      },
    };
    const txListeners = [];
    const tx = {
      addEventListener(t, f) { if (t === "characteristicvaluechanged") txListeners.push(f); },
      async startNotifications() {
        // This is where a real phone asks for the pairing code from the screen the first time.
        const r = await post("connect");
        const t = await r.text();
        if (!r.ok) throw new DOMException(t, "NetworkError");
        if (t === "pair") {
          const code = AUTO ? String((await (await fetch("/emu/state")).json()).pairCode)
            : prompt("Pairing request (emulator)\nType the pairing code shown on the deskbuddy screen:");
          const p = await post("pair", "", `&code=${encodeURIComponent(code || "")}`);
          if (!p.ok) { device.gatt.connected = false; throw new DOMException(await p.text(), "SecurityError"); }
        }
      },
      emit(text) {
        const bytes = new TextEncoder().encode(text);
        for (let i = 0; i < bytes.length; i += 20) {
          const value = new DataView(bytes.slice(i, i + 20).buffer);
          txListeners.forEach(f => f({target: {value}}));
        }
      },
    };
    const rx = {
      async writeValueWithResponse(data) {
        const line = new TextDecoder().decode(data);
        const r = await post("line", line);
        const t = await r.text();
        if (!r.ok) {
          device.gatt.connected = false;
          (listeners.gattserverdisconnected || []).forEach(f => f({target: device}));
          throw new DOMException(t, "NetworkError");
        }
        setTimeout(() => tx.emit(t), 5);  // the notify arrives separately from the write
      },
    };
    const server = {
      async getPrimaryService(uuid) {
        if (uuid !== NUS) throw new DOMException("service not found", "NotFoundError");
        return {async getCharacteristic(u) { return u.startsWith("6e400002") ? rx : tx; }};
      },
    };
    return device;
  }

  Object.defineProperty(navigator, "bluetooth", {
    configurable: true,
    value: {
      async requestDevice() {
        // Real Chrome shows a device picker here; the emulator has only one.
        if (!AUTO && !confirm("Bluetooth (emulator)\nConnect to deskbuddy-A45C?")) throw new DOMException("no device selected", "NotFoundError");
        return makeDevice();
      },
    },
  });
  // Show that this is the emulator
  addEventListener("DOMContentLoaded", () => {
    const s = document.getElementById("sub");
    if (s) s.dataset.emu = "1";
  });
})();
