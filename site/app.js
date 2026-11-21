const REPO = "AtaCanYmc/RubberOrca";

// 1. Subtle cursor glow
const glow = document.getElementById("cursorGlow");
window.addEventListener("pointermove", (e) => {
  glow.style.left = `${e.clientX}px`;
  glow.style.top = `${e.clientY}px`;
});

// 2. Dual-board flasher manifest selector (Super Mini / N16R8)
const flashButton = document.getElementById("flashButton");
const boardSpec = document.getElementById("boardSpec");
document.getElementById("boardSelector").addEventListener("click", (e) => {
  const btn = e.target.closest(".board-btn");
  if (!btn) return;
  document.querySelectorAll(".board-btn").forEach((b) => b.classList.remove("active"));
  btn.classList.add("active");
  flashButton.setAttribute("manifest", btn.dataset.manifest);
  boardSpec.textContent = btn.dataset.spec;
});

// 3. Fetch latest GitHub Release tag & date
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

// 4. Interactive PCB 3D tilt & hotspot inspector
const pcbCard = document.getElementById("pcbCard");
const hotspotStatus = document.getElementById("hotspotStatus");
const hotspotDetail = document.getElementById("hotspotDetail");

pcbCard.addEventListener("mousemove", (e) => {
  const rect = pcbCard.getBoundingClientRect();
  const x = (e.clientX - rect.left) / rect.width - 0.5;
  const y = (e.clientY - rect.top) / rect.height - 0.5;
  pcbCard.style.transform = `perspective(900px) rotateY(${x * 8}deg) rotateX(${-y * 8}deg)`;
});

pcbCard.addEventListener("mouseleave", () => {
  pcbCard.style.transform = "perspective(900px) rotateY(0deg) rotateX(0deg)";
});

document.querySelectorAll(".hotspot").forEach((el) => {
  el.addEventListener("mouseenter", () => {
    hotspotStatus.textContent = el.dataset.name;
    hotspotStatus.style.color = "var(--accent)";
    hotspotDetail.textContent = el.dataset.detail;
  });
  el.addEventListener("mouseleave", () => {
    hotspotStatus.textContent = "HOVER MODULE";
    hotspotStatus.style.color = "var(--muted)";
    hotspotDetail.textContent = "Select or hover a hardware block to inspect signal path.";
  });
});

// 5. Interactive HID Macro Simulator (RubberOrca wire commands)
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
      { delay: 360, type: "host", text: "On branch main — working tree clean" },
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
  document.querySelectorAll(".preset-btn").forEach((b) => {
    b.classList.toggle("active", b.dataset.preset === key);
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
    await new Promise((r) => setTimeout(r, evt.delay));
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
