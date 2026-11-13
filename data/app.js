// ======================================================
// ESP32-S3 Line Follower - WebSocket Client
// ======================================================

const $ = (id) => document.getElementById(id);

const els = {
    wsDot: $("wsDot"),
    wsText: $("wsText"),
    ipText: $("ipText"),
    rssiText: $("rssiText"),

    mErr: $("mErr"),
    mPid: $("mPid"),
    mL: $("mL"),
    mR: $("mR"),

    stateText: $("stateText"),
    noticeText: $("noticeText"),
    fullWarn: $("fullWarn"),
    logText: $("logText"),
    eventLog: $("eventLog"),

    kp: $("kp"), ki: $("ki"), kd: $("kd"), base: $("base"),
    vKp: $("vKp"), vKi: $("vKi"), vKd: $("vKd"), vBase: $("vBase"),

    chart: $("chart"),
    modeText: $("modeText"),

    destForm: $("destForm"),
    destId: $("destId"),
    destCode: $("destCode"),
    destBody: $("destBody"),
    destLog: $("destLog")
};

const sensors = [0, 1, 2, 3, 4].map((i) => $("s" + i));

let ws = null;
let reconnectTimer = null;
let applyingConfig = false;

const errHistory = [];
const MAX_POINTS = 150;

let tables = {};
let editingDestId = null;

// ======================================================
// TABS
// ======================================================

document.querySelectorAll(".tab").forEach((tab) => {
    tab.onclick = () => {
        document.querySelectorAll(".tab").forEach((t) => t.classList.remove("active"));
        document.querySelectorAll(".tabpane").forEach((p) => p.classList.remove("active"));
        tab.classList.add("active");
        $(tab.dataset.tab).classList.add("active");
    };
});

// ======================================================
// WEBSOCKET
// ======================================================

function connect() {
    const proto = location.protocol === "https:" ? "wss://" : "ws://";
    ws = new WebSocket(proto + location.host + "/ws");

    ws.onopen = () => {
        setOnline(true);
        log("WebSocket connected");
    };

    ws.onclose = () => {
        setOnline(false);
        log("Disconnected - reconnecting...");
        scheduleReconnect();
    };

    ws.onerror = () => {
        ws.close();
    };

    ws.onmessage = (ev) => {
        let msg;
        try {
            msg = JSON.parse(ev.data);
        } catch (e) {
            return;
        }

        if (msg.type === "config") updateConfig(msg);
        else if (msg.type === "telemetry") updateTelemetry(msg);
        else if (msg.type === "command_result" && msg.cmd === "goto") {
            destLog(msg.message || (msg.ok ? "Route request queued" : "Route request failed"));
            appendEventLog(msg.message || (msg.ok ? "Route request queued" : "Route request failed"));
        } else if (msg.type === "event") {
            appendEventLog(msg.message);
        }
    };
}

function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        connect();
    }, 1500);
}

function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(obj));
        return true;
    }
    return false;
}

function setOnline(online) {
    els.wsDot.className = "dot " + (online ? "online" : "offline");
    els.wsText.textContent = online ? "Connected" : "Disconnected";
}

function log(t) {
    els.logText.textContent = t;
}

function appendEventLog(message) {
    if (typeof message !== "string" || message.length === 0) return;

    const empty = els.eventLog.querySelector(".event-log-empty");
    if (empty) empty.remove();

    const entry = document.createElement("div");
    entry.className = "event-log-entry";

    const time = document.createElement("span");
    time.className = "event-log-time";
    time.textContent = new Date().toLocaleTimeString() + " ";

    entry.append(time, document.createTextNode(message));
    els.eventLog.appendChild(entry);

    while (els.eventLog.children.length > 60) {
        els.eventLog.firstElementChild.remove();
    }

    els.eventLog.scrollTop = els.eventLog.scrollHeight;
}

// ======================================================
// UPDATE UI
// ======================================================

function updateConfig(cfg) {
    applyingConfig = true;

    els.kp.value = cfg.kp;
    els.ki.value = cfg.ki;
    els.kd.value = cfg.kd;
    els.base.value = cfg.base;

    renderSliderLabels();
    applyingConfig = false;

    setState(cfg.on, cfg.mode);
}

function updateTelemetry(t) {
    for (let i = 0; i < 5; i++) {
        sensors[i].classList.toggle("active", t.s[i] === 1);
    }

    els.mErr.textContent = Number(t.err).toFixed(2);
    els.mPid.textContent = Number(t.pid).toFixed(2);
    els.mL.textContent = t.l;
    els.mR.textContent = t.r;

    els.ipText.textContent = "IP: " + t.ip;
    els.rssiText.textContent = "WiFi: " + t.rssi + " dBm";

    els.fullWarn.classList.toggle("hidden", !t.full);

    setState(t.on, t.mode);

    if (t.mode === "path") {
        els.noticeText.textContent =
            "DEST " + t.dest + " · STEP " + t.step + " · " + (t.notice || "");
    } else {
        els.noticeText.textContent = t.notice || "--";
    }

    pushChart(Number(t.err));
}

function setState(on, mode) {
    mode = mode || (on ? "auto" : "stop");

    const label = mode === "auto" ? "LINE FOLLOWING (AUTO)"
        : mode === "manual" ? "MANUAL DRIVE"
        : mode === "path" ? "ROUTE MODE (PATH)" : "STOPPED";

    const color = mode === "auto" || mode === "path" ? "var(--green)"
        : mode === "manual" ? "var(--amber)" : "var(--muted)";

    els.stateText.textContent = "Status: " + label;
    els.stateText.style.color = color;
    els.modeText.textContent = mode.toUpperCase();
    els.modeText.style.color = color;
}

function renderSliderLabels() {
    els.vKp.textContent = Number(els.kp.value).toFixed(1);
    els.vKi.textContent = Number(els.ki.value).toFixed(1);
    els.vKd.textContent = Number(els.kd.value).toFixed(1);
    els.vBase.textContent = els.base.value;
}

// ======================================================
// CHART
// ======================================================

function pushChart(v) {
    errHistory.push(v);
    if (errHistory.length > MAX_POINTS) errHistory.shift();
    drawChart();
}

function drawChart() {
    const c = els.chart;
    const ctx = c.getContext("2d");
    const W = c.width;
    const H = c.height;

    ctx.clearRect(0, 0, W, H);

    ctx.strokeStyle = "#2a3240";
    ctx.lineWidth = 1;

    for (let i = 0; i <= 4; i++) {
        const y = (H / 4) * i;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(W, y);
        ctx.stroke();
    }

    if (errHistory.length < 2) return;

    const max = Math.max(2, ...errHistory.map(Math.abs));

    ctx.strokeStyle = "#4ea1ff";
    ctx.lineWidth = 2;
    ctx.beginPath();

    errHistory.forEach((v, i) => {
        const x = (i / (MAX_POINTS - 1)) * W;
        const y = H / 2 - (v / max) * (H / 2 - 6);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
    });

    ctx.stroke();
}

// ======================================================
// CONTROL EVENTS
// ======================================================

$("btnStart").onclick = () => {
    send({ cmd: "start" });
    log("START command sent");
};

$("btnStop").onclick = () => {
    send({ cmd: "stop" });
    log("STOP command sent");
};

$("btnEstop").onclick = () => {
    send({ cmd: "estop" });
    log("E-STOP command sent");
};

$("btnPlay").onclick = () => {
    if (send({ cmd: "play", file: "welcome.mp3" })) {
        log("Play command sent for welcome.mp3");
    } else {
        log("WebSocket is not connected");
    }
};

let cfgTimer = null;

function onSlider() {
    renderSliderLabels();

    if (applyingConfig) return;

    clearTimeout(cfgTimer);
    cfgTimer = setTimeout(() => {
        send({
            cmd: "config",
            kp: parseFloat(els.kp.value),
            ki: parseFloat(els.ki.value),
            kd: parseFloat(els.kd.value),
            base: parseInt(els.base.value, 10)
        });
        log("PID / speed settings updated");
    }, 150);
}

[els.kp, els.ki, els.kd, els.base].forEach((el) => {
    el.addEventListener("input", onSlider);
});

// ======================================================
// MANUAL DRIVE (D-PAD + ARROW KEYS)
// ======================================================

const pressed = new Set();
let lastDriveDir = "stop";
let driveHeartbeat = null;

function dirFromPressed() {
    const up = pressed.has("forward");
    const down = pressed.has("back");
    const left = pressed.has("left");
    const right = pressed.has("right");

    if (up && left) return "forward_left";
    if (up && right) return "forward_right";
    if (down && left) return "back_left";
    if (down && right) return "back_right";
    if (up) return "forward";
    if (down) return "back";
    if (left) return "left";
    if (right) return "right";
    return "stop";
}

function updateDrive() {
    const dir = dirFromPressed();

    document.querySelectorAll(".pad").forEach((b) => {
        b.classList.toggle("held", pressed.has(b.dataset.dir));
    });

    if (dir !== lastDriveDir) {
        lastDriveDir = dir;
        send({ cmd: "drive", dir: dir });
        log(dir === "stop" ? "Manual drive stopped" : "Manual drive: " + dir);
    }

    if (dir !== "stop" && !driveHeartbeat) {
        driveHeartbeat = setInterval(() => {
            send({ cmd: "drive", dir: lastDriveDir });
        }, 250);
    } else if (dir === "stop" && driveHeartbeat) {
        clearInterval(driveHeartbeat);
        driveHeartbeat = null;
    }
}

document.querySelectorAll(".pad").forEach((b) => {
    const dir = b.dataset.dir;

    b.addEventListener("pointerdown", (e) => {
        e.preventDefault();
        pressed.add(dir);
        updateDrive();
    });

    const release = (e) => {
        e.preventDefault();
        pressed.delete(dir);
        updateDrive();
    };

    b.addEventListener("pointerup", release);
    b.addEventListener("pointercancel", release);
    b.addEventListener("pointerleave", release);
    b.addEventListener("contextmenu", (e) => e.preventDefault());
});

const keyMap = {
    ArrowUp: "forward",
    ArrowDown: "back",
    ArrowLeft: "left",
    ArrowRight: "right"
};

window.addEventListener("keydown", (e) => {
    const dir = keyMap[e.key];
    if (!dir || e.repeat) return;
    e.preventDefault();
    pressed.add(dir);
    updateDrive();
});

window.addEventListener("keyup", (e) => {
    const dir = keyMap[e.key];
    if (!dir) return;
    pressed.delete(dir);
    updateDrive();
});

// ======================================================
// DESTINATION TABLE
// ======================================================

function destLog(t) {
    els.destLog.textContent = t;
}

function updateDestFormMode() {
    const submitButton = els.destForm.querySelector('button[type="submit"]');
    submitButton.textContent = editingDestId === null ? "Add" : "Update";
}

function resetDestForm() {
    editingDestId = null;
    els.destForm.reset();
    updateDestFormMode();
}

function validCode(code) {
    return /^[LRS]*0$/.test(code) && code.length <= 90;
}

function renderTables() {
    const body = els.destBody;
    body.innerHTML = "";

    const ids = Object.keys(tables).map(Number).sort((a, b) => a - b);

    if (ids.length === 0) {
        const tr = document.createElement("tr");
        const td = document.createElement("td");
        td.colSpan = 4;
        td.className = "empty";
        td.textContent = "No destinations available";
        tr.appendChild(td);
        body.appendChild(tr);
        return;
    }

    ids.forEach((id) => {
        const code = tables[id];

        const tr = document.createElement("tr");

        const tdId = document.createElement("td");
        tdId.textContent = id;

        const tdCode = document.createElement("td");
        tdCode.className = "mono";
        tdCode.textContent = code;

        const tdGo = document.createElement("td");
        const btnGo = document.createElement("button");
        btnGo.className = "btn tiny start";
        btnGo.textContent = "Go";
        btnGo.onclick = () => {
            if (send({ cmd: "goto", id: id, code: code })) {
                destLog("Sending route " + code + " to robot...");
            } else {
                destLog("WebSocket is not connected");
            }
        };
        tdGo.appendChild(btnGo);

        const tdAct = document.createElement("td");
        tdAct.className = "rowact";

        const btnEdit = document.createElement("button");
        btnEdit.className = "btn tiny ghost";
        btnEdit.textContent = "Edit";
        btnEdit.onclick = () => {
            editingDestId = String(id);
            els.destId.value = id;
            els.destCode.value = code;
            els.destCode.focus();
            updateDestFormMode();
        };

        const btnDel = document.createElement("button");
        btnDel.className = "btn tiny estop";
        btnDel.textContent = "Delete";
        btnDel.onclick = () => {
            delete tables[id];
            if (editingDestId === String(id)) {
                resetDestForm();
            }
            renderTables();
            destLog("Destination " + id + " deleted locally (not saved to robot)");
        };

        tdAct.appendChild(btnEdit);
        tdAct.appendChild(btnDel);

        tr.appendChild(tdId);
        tr.appendChild(tdCode);
        tr.appendChild(tdGo);
        tr.appendChild(tdAct);
        body.appendChild(tr);
    });
}

async function loadTables() {
    try {
        const res = await fetch("/api/tables", { cache: "no-store" });
        if (!res.ok) throw new Error("HTTP " + res.status);
        const loadedTables = await res.json();
        if (!loadedTables || Array.isArray(loadedTables) || typeof loadedTables !== "object") {
            throw new Error("Invalid destination table data");
        }
        tables = loadedTables;
        resetDestForm();
        renderTables();
        destLog("Loaded " + Object.keys(tables).length + " destination(s)");
    } catch (e) {
        destLog("Failed to load destination table: " + e.message);
    }
}

async function saveTables() {
    try {
        const res = await fetch("/api/tables", {
            method: "PUT",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(tables)
        });

        const data = await res.json();

        if (!res.ok || !data.ok) {
            throw new Error(data.error || ("HTTP " + res.status));
        }

        destLog(Object.keys(tables).length === 0
            ? "All destinations deleted from the robot"
            : "Destination table saved to robot");
    } catch (e) {
        destLog("Failed to save destination table: " + e.message);
    }
}

els.destForm.addEventListener("submit", (e) => {
    e.preventDefault();

    const id = Number(els.destId.value);
    const idKey = String(id);
    const code = els.destCode.value.trim().toUpperCase();

    if (!Number.isSafeInteger(id) || id < 1 || id > 2147483647) {
        destLog("ID must be an integer from 1 to 2147483647");
        return;
    }

    if (!validCode(code)) {
        destLog("Invalid code: use only L, R, S and end with 0");
        return;
    }

    if (editingDestId !== null &&
        idKey !== editingDestId &&
        Object.prototype.hasOwnProperty.call(tables, idKey)) {
        destLog("ID " + id + " already exists; choose another ID");
        return;
    }

    const wasEditing = editingDestId !== null;
    if (wasEditing) {
        delete tables[editingDestId];
    }
    tables[idKey] = code;
    resetDestForm();
    renderTables();
    destLog((wasEditing ? "Updated destination " : "Added/updated destination ") +
        id + " = " + code + " (not saved to robot)");
});

$("btnReload").onclick = loadTables;
$("btnSave").onclick = saveTables;

// ======================================================
// START
// ======================================================

renderSliderLabels();
drawChart();
renderTables();
updateDestFormMode();
connect();
loadTables();
