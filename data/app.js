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
        log("Da ket noi websocket");
    };

    ws.onclose = () => {
        setOnline(false);
        log("Mat ket noi - dang thu lai...");
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
    els.wsText.textContent = online ? "Da ket noi" : "Mat ket noi";
}

function log(t) {
    els.logText.textContent = t;
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
            "DEST " + t.dest + " · buoc " + t.step + " · " + (t.notice || "");
    } else {
        els.noticeText.textContent = t.notice || "--";
    }

    pushChart(Number(t.err));
}

function setState(on, mode) {
    mode = mode || (on ? "auto" : "stop");

    const label = mode === "auto" ? "DO LINE (AUTO)"
        : mode === "manual" ? "LAI TAY (MANUAL)"
        : mode === "path" ? "DI THEO MA (PATH)" : "DUNG";

    const color = mode === "auto" || mode === "path" ? "var(--green)"
        : mode === "manual" ? "var(--amber)" : "var(--muted)";

    els.stateText.textContent = "Trang thai: " + label;
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
    log("Gui lenh START");
};

$("btnStop").onclick = () => {
    send({ cmd: "stop" });
    log("Gui lenh STOP");
};

$("btnEstop").onclick = () => {
    send({ cmd: "estop" });
    log("Gui lenh E-STOP");
};

$("btnPlay").onclick = () => {
    if (send({ cmd: "play", file: "welcome.mp3" })) {
        log("Gui lenh phat welcome.mp3");
    } else {
        log("Chua ket noi websocket");
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
        log("Da cap nhat PID / speed");
    }, 150);
}

[els.kp, els.ki, els.kd, els.base].forEach((el) => {
    el.addEventListener("input", onSlider);
});

// ======================================================
// MANUAL DRIVE (D-PAD + PHIM MUI TEN)
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
        log(dir === "stop" ? "Dung lai tay" : "Lai tay: " + dir);
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
    submitButton.textContent = editingDestId === null ? "Them" : "Cap nhat";
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
        td.textContent = "Chua co du lieu";
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
        btnGo.textContent = "Di";
        btnGo.onclick = () => {
            if (send({ cmd: "goto", id: id })) {
                destLog("Da gui lenh di toi diem " + id);
            } else {
                destLog("Chua ket noi websocket");
            }
        };
        tdGo.appendChild(btnGo);

        const tdAct = document.createElement("td");
        tdAct.className = "rowact";

        const btnEdit = document.createElement("button");
        btnEdit.className = "btn tiny ghost";
        btnEdit.textContent = "Sua";
        btnEdit.onclick = () => {
            editingDestId = String(id);
            els.destId.value = id;
            els.destCode.value = code;
            els.destCode.focus();
            updateDestFormMode();
        };

        const btnDel = document.createElement("button");
        btnDel.className = "btn tiny estop";
        btnDel.textContent = "Xoa";
        btnDel.onclick = () => {
            delete tables[id];
            if (editingDestId === String(id)) {
                resetDestForm();
            }
            renderTables();
            destLog("Da xoa diem " + id + " (chua luu len robot)");
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
            throw new Error("Du lieu bang diem den khong hop le");
        }
        tables = loadedTables;
        resetDestForm();
        renderTables();
        destLog("Da tai " + Object.keys(tables).length + " diem den");
    } catch (e) {
        destLog("Tai bang that bai: " + e.message);
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
            ? "Da xoa toan bo diem den tren robot"
            : "Da luu len robot");
    } catch (e) {
        destLog("Luu that bai: " + e.message);
    }
}

els.destForm.addEventListener("submit", (e) => {
    e.preventDefault();

    const id = Number(els.destId.value);
    const idKey = String(id);
    const code = els.destCode.value.trim().toUpperCase();

    if (!Number.isSafeInteger(id) || id < 1 || id > 2147483647) {
        destLog("ID phai la so nguyen tu 1 den 2147483647");
        return;
    }

    if (!validCode(code)) {
        destLog("Ma sai: chi dung L R S va phai ket thuc bang 0");
        return;
    }

    if (editingDestId !== null &&
        idKey !== editingDestId &&
        Object.prototype.hasOwnProperty.call(tables, idKey)) {
        destLog("ID " + id + " da ton tai; hay chon ID khac");
        return;
    }

    const wasEditing = editingDestId !== null;
    if (wasEditing) {
        delete tables[editingDestId];
    }
    tables[idKey] = code;
    resetDestForm();
    renderTables();
    destLog((wasEditing ? "Da sua diem " : "Da them/cap nhat diem ") +
        id + " = " + code + " (chua luu len robot)");
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
