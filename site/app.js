const REPO = "AtaCanYmc/RubberOrca";
const motionQuery = window.matchMedia("(prefers-reduced-motion: no-preference)");

// 1. Dual-board flasher manifest selector (Super Mini / N16R8)
const flashButton = document.getElementById("flashButton");
const boardSpec = document.getElementById("boardSpec");
document.getElementById("boardSelector").addEventListener("click", (e) => {
  const btn = e.target.closest(".board-btn");
  if (!btn) return;
  document.querySelectorAll(".board-btn").forEach((b) => {
    const active = b === btn;
    b.classList.toggle("active", active);
    b.setAttribute("aria-pressed", String(active));
  });
  flashButton.setAttribute("manifest", btn.dataset.manifest);
  boardSpec.textContent = btn.dataset.spec;
});

// 2. Fetch latest GitHub Release tag & date
async function loadLatestRelease() {
  const tagEl = document.getElementById("releaseTag");
  const metaEl = document.getElementById("releaseMeta");

  try {
    const res = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`);
    if (!res.ok) throw new Error("Release fetch failed");
    const data = await res.json();
    const date = new Date(data.published_at).toISOString().slice(0, 10);

    tagEl.textContent = data.tag_name;
    metaEl.textContent = `${data.tag_name} (${date})`;
  } catch {
    metaEl.textContent = "Latest on GitHub";
  }
}
loadLatestRelease();

// 3. Interactive PCB 3D tilt (reduced-motion safe) & keyboard-accessible hotspots
const pcbCard = document.getElementById("pcbCard");
const hotspotStatus = document.getElementById("hotspotStatus");
const hotspotDetail = document.getElementById("hotspotDetail");
const DEFAULT_HOTSPOT_DETAIL = "Hover or focus a hardware block to inspect signal path.";

pcbCard.addEventListener("mousemove", (e) => {
  if (!motionQuery.matches) return;
  const rect = pcbCard.getBoundingClientRect();
  const x = (e.clientX - rect.left) / rect.width - 0.5;
  const y = (e.clientY - rect.top) / rect.height - 0.5;
  pcbCard.style.transform = `perspective(900px) rotateY(${x * 6}deg) rotateX(${-y * 6}deg)`;
});

pcbCard.addEventListener("mouseleave", () => {
  pcbCard.style.transform = "";
});

function showHotspot(el) {
  hotspotStatus.textContent = el.dataset.name;
  hotspotStatus.style.color = "var(--accent)";
  hotspotDetail.textContent = el.dataset.detail;
}

function resetHotspot() {
  hotspotStatus.textContent = "INSPECT MODULE";
  hotspotStatus.style.color = "var(--muted)";
  hotspotDetail.textContent = DEFAULT_HOTSPOT_DETAIL;
}

document.querySelectorAll(".hotspot").forEach((el) => {
  el.addEventListener("mouseenter", () => showHotspot(el));
  el.addEventListener("mouseleave", resetHotspot);
  el.addEventListener("focus", () => showHotspot(el));
  el.addEventListener("blur", resetHotspot);
});

// 4. Interactive HID Macro Simulator (RubberOrca wire commands)
const PRESETS = {
  chain: {
    script: `press gui 50 && delay 250 && type "terminal" && delay 150 && enter\ndelay 350 && type "echo \\"RubberOrca HID active\\"" && enter`,
    events: [
      { delay: 260, type: "hid", text: "[BLE 0xFFE1] RX STX VER=0x01 SEQ=0x01 XOR=OK" },
      { delay: 320, type: "hid", text: "[HID] PRESS: LEFT_GUI (50ms)" },
      { delay: 350, type: "hid", text: "[HID] TYPE: terminal + KEY_RETURN" },
      { delay: 420, type: "host", text: 'zsh % echo "RubberOrca HID active"' },
      { delay: 280, type: "host", text: "RubberOrca HID active" },
      { delay: 200, type: "hid", text: "[BLE 0xFFE1] TX ACK seq=1 ok=1 code=0" }
    ]
  },
  trq: {
    script: `layout tr_q && type "Merhaba İstanbul @orca"\nenter`,
    events: [
      { delay: 260, type: "hid", text: "[NVS] kbd_layout = 1 (TR_Q)" },
      { delay: 340, type: "hid", text: "[HID RAW] İ -> mod=0x02 usage=0x34 | @ -> mod=0x40 usage=0x14" },
      { delay: 380, type: "host", text: "Merhaba İstanbul @orca" },
      { delay: 200, type: "hid", text: "[BLE 0xFFE1] TX ACK seq=2 ok=1 code=0" }
    ]
  },
  macro: {
    script: `macro define m0 { type "git status" && enter }\nmacro run m0\njiggler toggle`,
    events: [
      { delay: 260, type: "hid", text: "[NVS] Saved slot m0 (26 bytes)" },
      { delay: 320, type: "hid", text: "[HID] TYPE: git status + KEY_RETURN" },
      { delay: 360, type: "host", text: "On branch main. Working tree clean." },
      { delay: 300, type: "hid", text: "[LED] RGB -> AMBER (16, 10, 0) // JIGGLER ON" },
      { delay: 200, type: "hid", text: "[BLE 0xFFE1] TX ACK seq=3 ok=1 code=0" }
    ]
  }
};

let activePreset = "chain";
let simRunning = false;

const simCode = document.getElementById("simCode");
const termOutput = document.getElementById("termOutput");
const simState = document.getElementById("simState");
const runSimBtn = document.getElementById("runSimBtn");

function renderPreset(key) {
  activePreset = key;
  simCode.textContent = PRESETS[key].script;
  document.querySelectorAll("#simPresets .preset-btn").forEach((b) => {
    const active = b.dataset.preset === key;
    b.classList.toggle("active", active);
    b.setAttribute("aria-selected", String(active));
  });
}

document.getElementById("simPresets").addEventListener("click", (e) => {
  if (e.target.matches(".preset-btn") && !simRunning) {
    renderPreset(e.target.dataset.preset);
  }
});

runSimBtn.addEventListener("click", async () => {
  if (simRunning) return;
  simRunning = true;
  simState.textContent = "INJECTING";
  termOutput.innerHTML = `<div class="dim">// USB-HID endpoint claimed (ESP32-S3 TinyUSB)...</div>`;

  for (const evt of PRESETS[activePreset].events) {
    const wait = motionQuery.matches ? evt.delay : 40;
    await new Promise((r) => setTimeout(r, wait));
    const line = document.createElement("div");
    line.className = evt.type === "hid" ? "hid-evt" : "";
    line.textContent = evt.text;
    termOutput.appendChild(line);
    termOutput.scrollTop = termOutput.scrollHeight;
  }

  const done = document.createElement("div");
  done.className = "dim";
  done.textContent = "// Payload completed.";
  termOutput.appendChild(done);

  simState.textContent = "COMPLETE";
  simRunning = false;
});

renderPreset(activePreset);

// 5. CLI / MCP / Python SDK Usage Tabs + Invoker Command Copy Button
const USAGE_SNIPPETS = {
  cli: {
    code: `# Install unified package (SDK + CLI + MCP)
pip install rubberorca

# Discover BLE 0xFFE0 devices and USB serial ports
rubberorca scan

# Send commands over BLE (default) or USB CDC (--port)
rubberorca type "hello from RubberOrca"
rubberorca key enter
rubberorca layout tr_q
rubberorca ble-passkey 482913
rubberorca macro define m0 'type "git status" && enter'
rubberorca macro run m0
rubberorca jiggler toggle`,
    meta: "Use --port /dev/cu.usbmodem1101 or --address <BLE_UUID> to target a specific board."
  },
  mcp: {
    code: `// Add to Claude Desktop, Cursor, or Antigravity mcp_config.json
{
  "mcpServers": {
    "rubberorca": {
      "command": "uvx",
      "args": ["--from", "rubberorca", "rubberorca-mcp"]
    }
  }
}

// Exposed stdio JSON-RPC 2.0 tools:
// scan_devices, type_text, press_key, media_control,
// mouse_action, jiggler, macro_define, macro_run, send_command`,
    meta: "Pass --port or set RUBBERORCA_PORT / RUBBERORCA_ADDRESS env vars to pin a device."
  },
  sdk: {
    code: `from rubberorca import RubberOrca

# Auto-discovers BLE 0xFFE0 (or pass port="/dev/cu.usbmodem1101")
orca = RubberOrca(timeout=3.0)

orca.set_layout("tr_q")
orca.type("Merhaba İstanbul @orca")
orca.key("enter")
orca.media("play_pause")
orca.mouse_move(40, -20)
orca.jiggler("toggle")`,
    meta: "Zero extra framework dependencies. Built on bleak and pyserial."
  }
};

let activeUsage = "cli";
const usageCode = document.getElementById("usageCode");
const usageMeta = document.getElementById("usageMeta");

function renderUsage(key) {
  activeUsage = key;
  usageCode.textContent = USAGE_SNIPPETS[key].code;
  usageMeta.textContent = USAGE_SNIPPETS[key].meta;
  document.querySelectorAll("#usageTabs .preset-btn").forEach((b) => {
    const active = b.dataset.usage === key;
    b.classList.toggle("active", active);
    b.setAttribute("aria-selected", String(active));
  });
}

document.getElementById("usageTabs").addEventListener("click", (e) => {
  const btn = e.target.closest(".preset-btn");
  if (btn) renderUsage(btn.dataset.usage);
});

renderUsage(activeUsage);

// Copy button using Invoker Commands + progressive click fallback
const copyTimers = new WeakMap();
let lastAnnouncement = "";

function reportCopy(button, ok) {
  const tip = button.nextElementSibling;
  clearTimeout(copyTimers.get(button));
  button.removeAttribute("data-copied");
  button.removeAttribute("data-failed");
  let message = ok ? "Copied" : "Copy failed";
  if (message === lastAnnouncement) message += "\u00A0";
  lastAnnouncement = message;
  tip.textContent = "";
  void button.offsetWidth;
  button.setAttribute(ok ? "data-copied" : "data-failed", "");
  requestAnimationFrame(() => {
    tip.textContent = message;
  });
  copyTimers.set(
    button,
    setTimeout(() => {
      button.removeAttribute("data-copied");
      button.removeAttribute("data-failed");
      tip.textContent = "";
    }, 2000)
  );
}

async function copyTarget(button, target) {
  const text = target.innerText.trim();
  if (!text) return;
  try {
    await navigator.clipboard.writeText(text);
    reportCopy(button, true);
  } catch {
    reportCopy(button, false);
  }
}

document.addEventListener(
  "command",
  (event) => {
    if (event.command === "--copy") copyTarget(event.source, event.target);
  },
  true
);

if (!("CommandEvent" in window)) {
  document.addEventListener("click", (event) => {
    const button = event.target.closest('button[command="--copy"]');
    if (!button) return;
    const target = document.getElementById(button.getAttribute("commandfor"));
    if (target) copyTarget(button, target);
  });
}
