// Nagebootste Web Bluetooth voor de web-emulator. De PWA-code (ble-transport in index.html) blijft
// ongewijzigd: hij praat met dit object alsof het een echte deskbuddy met de Nordic UART Service is.
// Antwoorden komen in stukjes van 20 bytes binnen (zoals bij de standaard-MTU) en eindigen op "\n".
(() => {
  const NUS = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  const post = (a, body = "", q = "") => fetch(`/emu/ble?a=${a}${q}`, {method: "POST", body});
  // ?auto: geen vragen; de koppelcode wordt van het (nagebootste) scherm gelezen. Handig voor tests.
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
        // Hier vraagt een echte telefoon bij de eerste keer om de koppelcode van het scherm.
        const r = await post("connect");
        const t = await r.text();
        if (!r.ok) throw new DOMException(t, "NetworkError");
        if (t === "pair") {
          const code = AUTO ? String((await (await fetch("/emu/state")).json()).pairCode)
            : prompt("Koppelverzoek (emulator)\nTyp de koppelcode die op het scherm van de deskbuddy staat:");
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
        setTimeout(() => tx.emit(t), 5);  // notify komt los van het schrijven binnen
      },
    };
    const server = {
      async getPrimaryService(uuid) {
        if (uuid !== NUS) throw new DOMException("service niet gevonden", "NotFoundError");
        return {async getCharacteristic(u) { return u.startsWith("6e400002") ? rx : tx; }};
      },
    };
    return device;
  }

  Object.defineProperty(navigator, "bluetooth", {
    configurable: true,
    value: {
      async requestDevice() {
        // Echte Chrome toont hier een apparaatkiezer; de emulator heeft er maar één.
        if (!AUTO && !confirm("Bluetooth (emulator)\nVerbinden met deskbuddy-A45C?")) throw new DOMException("geen apparaat gekozen", "NotFoundError");
        return makeDevice();
      },
    },
  });
  // Laat zien dat dit de emulator is
  addEventListener("DOMContentLoaded", () => {
    const s = document.getElementById("sub");
    if (s) s.dataset.emu = "1";
  });
})();
