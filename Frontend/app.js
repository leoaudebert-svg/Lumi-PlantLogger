(function () {
  const POLL_MS = 5000;
  const MAX_CHART_POINTS = 500;
  const NAME_PATTERN = /^[A-Za-z0-9 _.-]+$/;
  const isGraphPage = window.location.pathname.replace(/\/+$/, "") === "/graphs";

  const METRICS = [
    { key: "temp_c", label: "Temperature", unit: "C", color: "#a56026", digits: 1 },
    { key: "rh_pct", label: "Humidity", unit: "%", color: "#237579", digits: 1 },
    { key: "pressure_hpa", label: "Pressure", unit: "hPa", color: "#a84d37", digits: 1 },
    { key: "lux", label: "Light", unit: "lux", color: "#c98513", digits: 0 },
    { key: "soil_raw", label: "Soil raw", unit: "ADC", color: "#456d38", digits: 0 },
    { key: "soil_pct", label: "Soil moisture", unit: "%", color: "#294c2d", digits: 0 }
  ];

  const el = {
    dashboardView: document.getElementById("dashboardView"),
    graphView: document.getElementById("graphView"),
    loggingBadge: document.getElementById("loggingBadge"),
    deviceBadge: document.getElementById("deviceBadge"),
    lastSync: document.getElementById("lastSync"),
    soilValue: document.getElementById("soilValue"),
    soilUnit: document.getElementById("soilUnit"),
    soilState: document.getElementById("soilState"),
    soilHelper: document.getElementById("soilHelper"),
    tempValue: document.getElementById("tempValue"),
    tempHelper: document.getElementById("tempHelper"),
    humidityValue: document.getElementById("humidityValue"),
    humidityHelper: document.getElementById("humidityHelper"),
    lightValue: document.getElementById("lightValue"),
    lightHelper: document.getElementById("lightHelper"),
    pressureValue: document.getElementById("pressureValue"),
    pressureHelper: document.getElementById("pressureHelper"),
    overallBadge: document.getElementById("overallBadge"),
    soilSummaryTitle: document.getElementById("soilSummaryTitle"),
    soilSummaryDetail: document.getElementById("soilSummaryDetail"),
    lightSummaryTitle: document.getElementById("lightSummaryTitle"),
    lightSummaryDetail: document.getElementById("lightSummaryDetail"),
    climateSummaryTitle: document.getElementById("climateSummaryTitle"),
    climateSummaryDetail: document.getElementById("climateSummaryDetail"),
    logNameField: document.querySelector(".name-field"),
    logNameInput: document.getElementById("logNameInput"),
    logNameHint: document.getElementById("logNameHint"),
    intervalInput: document.getElementById("intervalInput"),
    readNowButton: document.getElementById("readNowButton"),
    startButton: document.getElementById("startButton"),
    stopButton: document.getElementById("stopButton"),
    clearButton: document.getElementById("clearButton"),
    captureDryButton: document.getElementById("captureDryButton"),
    captureWetButton: document.getElementById("captureWetButton"),
    clearCalibrationButton: document.getElementById("clearCalibrationButton"),
    message: document.getElementById("message"),
    dryValue: document.getElementById("dryValue"),
    wetValue: document.getElementById("wetValue"),
    calibrationMessage: document.getElementById("calibrationMessage"),
    logsCount: document.getElementById("logsCount"),
    logsBody: document.getElementById("logsBody"),
    storageUsedLabel: document.getElementById("storageUsedLabel"),
    storageFreeLabel: document.getElementById("storageFreeLabel"),
    storageLogLabel: document.getElementById("storageLogLabel"),
    storageBar: document.querySelector(".storage-bar"),
    storageBarFill: document.getElementById("storageBarFill"),
    graphTitle: document.getElementById("graphTitle"),
    graphMeta: document.getElementById("graphMeta"),
    graphMessage: document.getElementById("graphMessage"),
    chartsGrid: document.getElementById("chartsGrid"),
    pageLoader: document.getElementById("pageLoader")
  };

  let lastStatus = null;
  let chartState = [];
  let logNameSubmitAttempted = false;
  let logNameManualEditArmed = false;

  const isIOS = /iPad|iPhone|iPod/.test(navigator.platform) ||
    (navigator.platform === "MacIntel" && navigator.maxTouchPoints > 1);

  function setMessage(target, text, isError) {
    if (!target) {
      return;
    }
    target.textContent = text || "";
    target.classList.toggle("error", !!isError);
  }

  function showPageLoader() {
    if (!el.pageLoader) {
      return;
    }
    el.pageLoader.classList.add("visible");
    el.pageLoader.setAttribute("aria-hidden", "false");
  }

  function hidePageLoader() {
    if (!el.pageLoader) {
      return;
    }
    el.pageLoader.classList.remove("visible");
    el.pageLoader.setAttribute("aria-hidden", "true");
  }

  function formatNumber(value, digits) {
    if (typeof value !== "number" || !Number.isFinite(value)) {
      return "--";
    }
    return value.toFixed(digits);
  }

  function formatBytes(value) {
    if (!value || value < 0) {
      return "0 B";
    }
    if (value < 1024) {
      return value + " B";
    }
    if (value < 1024 * 1024) {
      return (value / 1024).toFixed(1) + " KB";
    }
    return (value / (1024 * 1024)).toFixed(1) + " MB";
  }

  function validateLogName(value) {
    const name = (value || "").trim();
    if (!name) {
      return { ok: false, name: "", error: "Log name is required." };
    }
    if (name.length > 32) {
      return { ok: false, name: name, error: "Use 32 characters or less." };
    }
    if (!NAME_PATTERN.test(name)) {
      return { ok: false, name: name, error: "Use letters, numbers, spaces, _, -, and ." };
    }
    return { ok: true, name: name, error: "" };
  }

  function setDisabled(target, disabled) {
    if (target && target.disabled !== disabled) {
      target.disabled = disabled;
    }
  }

  function lockLogNameInput() {
    if (!el.logNameInput) {
      return;
    }
    if (document.activeElement === el.logNameInput) {
      el.logNameInput.blur();
    }
    setDisabled(el.logNameInput, true);
    el.logNameInput.readOnly = true;
    el.logNameInput.style.pointerEvents = "none";
    el.logNameInput.setAttribute("tabindex", "-1");
  }

  function unlockLogNameInput() {
    if (!el.logNameInput || el.logNameInput.disabled) {
      if (!el.logNameInput) {
        return;
      }
      if (lastStatus && lastStatus.logging) {
        return;
      }
      setDisabled(el.logNameInput, false);
    }
    if (lastStatus && lastStatus.logging) {
      return;
    }
    if (el.logNameInput.readOnly) {
      el.logNameInput.readOnly = false;
    }
    el.logNameInput.style.pointerEvents = "";
    el.logNameInput.removeAttribute("tabindex");
  }

  function armLogNameManualEdit() {
    logNameManualEditArmed = true;
    unlockLogNameInput();
  }

  function resetLogNameManualEdit() {
    logNameManualEditArmed = false;
  }

  function updateLogNameHint(showRequired) {
    if (!el.logNameInput || !el.logNameHint) {
      return { ok: true, name: "" };
    }
    const result = validateLogName(el.logNameInput.value);
    const shouldShow = !result.ok && (result.name.length > 0 || !!showRequired);
    el.logNameHint.textContent = shouldShow ? result.error : "";
    el.logNameHint.classList.toggle("error", shouldShow);
    return result;
  }

  function requireLogName() {
    logNameSubmitAttempted = true;
    return updateLogNameHint(true);
  }

  function setButtonState(status) {
    const logging = !!status.logging;

    setDisabled(el.intervalInput, logging);
    setDisabled(el.startButton, logging);
    setDisabled(el.stopButton, !logging);
    setDisabled(el.clearButton, logging);

    if (logging) {
      resetLogNameManualEdit();
      lockLogNameInput();
      return;
    }
    if (isIOS) {
      if (logNameManualEditArmed) {
        unlockLogNameInput();
      } else {
        lockLogNameInput();
      }
      return;
    }
    unlockLogNameInput();
  }

  function updateBadges(status) {
    el.loggingBadge.classList.toggle("active", !!status.logging);
    el.loggingBadge.classList.toggle("warning", !status.logging);
    el.loggingBadge.innerHTML = '<span class="dot"></span>' + (status.logging ? "Logging on" : "Logging off");
    el.deviceBadge.innerHTML = '<span class="dot"></span>Device online';
    el.lastSync.textContent = "Last sync: just now";
  }

  function updateMetrics(status) {
    el.tempValue.textContent = formatNumber(status.temp_c, 1);
    el.humidityValue.textContent = formatNumber(status.rh_pct, 1);
    el.lightValue.textContent = status.lux || 0;

    if (status.pressure_hpa > 0) {
      el.pressureValue.textContent = formatNumber(status.pressure_hpa, 1);
      el.pressureHelper.textContent = "Atmospheric pressure available";
    } else {
      el.pressureValue.textContent = "--";
      el.pressureHelper.textContent = "Pressure sensor unavailable";
    }

    if (status.soil_calibrated) {
      el.soilValue.textContent = formatNumber(status.soil_pct, 0);
      el.soilUnit.textContent = "%";
      el.soilState.textContent = status.soil_pct < 25 ? "Low moisture" : status.soil_pct > 80 ? "Very wet" : "Healthy range";
      el.soilHelper.textContent = "Raw ADC " + status.soil_raw;
    } else {
      el.soilValue.textContent = status.soil_raw || 0;
      el.soilUnit.textContent = "raw";
      el.soilState.textContent = "Needs calibration";
      el.soilHelper.textContent = "Capture dry and wet points to show percent.";
    }

    el.tempHelper.textContent = status.temp_c === 0 ? "Sensor may be unavailable" : "Temperature sensor synced";
    el.humidityHelper.textContent = status.rh_pct === 0 ? "Sensor may be unavailable" : "Humidity sensor synced";
    el.lightHelper.textContent = status.lux === 0 ? "Light sensor may be unavailable" : "Light sensor synced";
    el.dryValue.textContent = "Dry: " + (status.cal_dry > 0 ? status.cal_dry : "--");
    el.wetValue.textContent = "Wet: " + (status.cal_wet > 0 ? status.cal_wet : "--");
  }

  function updateSummary(status) {
    if (!status.soil_calibrated) {
      el.soilSummaryTitle.textContent = "Soil needs calibration";
      el.soilSummaryDetail.textContent = "Raw value " + status.soil_raw + " is available, but percent needs dry and wet points.";
    } else if (status.soil_pct < 25) {
      el.soilSummaryTitle.textContent = "Soil moisture is low";
      el.soilSummaryDetail.textContent = "Water soon if this plant prefers steady moisture.";
    } else if (status.soil_pct > 80) {
      el.soilSummaryTitle.textContent = "Soil is very wet";
      el.soilSummaryDetail.textContent = "Give the soil time to drain before watering again.";
    } else {
      el.soilSummaryTitle.textContent = "Soil moisture is healthy";
      el.soilSummaryDetail.textContent = "The calibrated moisture is in a comfortable range.";
    }

    if (status.lux < 200) {
      el.lightSummaryTitle.textContent = "Light is low";
      el.lightSummaryDetail.textContent = "Move closer to a brighter window if growth looks slow.";
    } else if (status.lux > 12000) {
      el.lightSummaryTitle.textContent = "Light is strong";
      el.lightSummaryDetail.textContent = "Watch leaves for stress if this is direct afternoon sun.";
    } else {
      el.lightSummaryTitle.textContent = "Light looks usable";
      el.lightSummaryDetail.textContent = "Current lux is in a moderate indoor range.";
    }

    if (status.temp_c < 10 || status.temp_c > 35) {
      el.climateSummaryTitle.textContent = "Temperature needs attention";
      el.climateSummaryDetail.textContent = "The current reading is outside a common indoor comfort range.";
    } else {
      el.climateSummaryTitle.textContent = "Climate is stable";
      el.climateSummaryDetail.textContent = "Temperature and humidity are being reported normally.";
    }

    const soilGood = status.soil_calibrated && status.soil_pct >= 25 && status.soil_pct <= 80;
    const climateGood = status.temp_c >= 10 && status.temp_c <= 35;
    el.overallBadge.textContent = soilGood && climateGood ? "Overall: steady" : "Overall: check";
  }

  function clearLogsTable(message) {
    el.logsBody.textContent = "";
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 3;
    cell.className = "empty-cell";
    cell.textContent = message;
    row.appendChild(cell);
    el.logsBody.appendChild(row);
  }

  function renderStorage(storage) {
    const total = Number(storage && storage.total_bytes) || 0;
    const used = Number(storage && storage.used_bytes) || 0;
    const free = Number(storage && storage.free_bytes) || Math.max(total - used, 0);
    const logBytes = Number(storage && storage.log_bytes) || 0;
    const pct = total > 0 ? Math.min(100, Math.max(0, (used / total) * 100)) : 0;

    el.storageUsedLabel.textContent = formatBytes(used) + " of " + formatBytes(total) + " used";
    el.storageFreeLabel.textContent = formatBytes(free) + " free";
    el.storageLogLabel.textContent = "Logs: " + formatBytes(logBytes);
    el.storageBar.setAttribute("aria-valuenow", String(Math.round(pct)));
    el.storageBarFill.style.width = pct.toFixed(1) + "%";
  }

  function renderLogs(logs, storage) {
    renderStorage(storage || {});
    el.logsCount.textContent = logs.length + (logs.length === 1 ? " file" : " files");
    el.logsBody.textContent = "";

    if (!logs.length) {
      clearLogsTable("No completed logs yet.");
      return;
    }

    logs.forEach(function (log) {
      const row = document.createElement("tr");
      const nameCell = document.createElement("td");
      const sizeCell = document.createElement("td");
      const actionCell = document.createElement("td");
      const link = document.createElement("a");

      nameCell.textContent = log.name || log.filename || ("Log " + (Number(log.slot) + 1));
      if (log.is_latest) {
        const tag = document.createElement("span");
        tag.className = "latest-tag";
        tag.textContent = "latest";
        nameCell.appendChild(tag);
      }

      sizeCell.textContent = formatBytes(log.size_bytes || 0);
      link.className = "graph-link";
      link.href = "/graphs?slot=" + encodeURIComponent(log.slot);
      link.setAttribute("aria-label", "Open graphs for " + (log.name || log.filename || "this log"));
      link.title = "Open graphs";
      link.innerHTML = '<svg viewBox="0 0 24 24" focusable="false" aria-hidden="true"><path d="M4 19V5"></path><path d="M4 19h16"></path><path d="M8 16v-5"></path><path d="M12 16V8"></path><path d="M16 16v-7"></path><path d="M20 16v-3"></path></svg><span class="sr-only">Open graphs</span>';
      link.addEventListener("click", function (event) {
        if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) {
          return;
        }
        showPageLoader();
      });
      actionCell.appendChild(link);

      row.appendChild(nameCell);
      row.appendChild(sizeCell);
      row.appendChild(actionCell);
      el.logsBody.appendChild(row);
    });
  }

  async function fetchJson(url) {
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(await response.text());
    }
    return response.json();
  }

  async function fetchText(url) {
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(await response.text());
    }
    return response.text();
  }

  async function postCommand(url, successMessage, target) {
    const messageTarget = target || el.message;
    setMessage(messageTarget, "Working...", false);
    try {
      const response = await fetch(url, { method: "POST" });
      const text = await response.text();
      if (!response.ok) {
        throw new Error(text || "Request failed");
      }
      setMessage(messageTarget, text && text !== "OK" ? text : successMessage, false);
      await refreshAll();
    } catch (error) {
      setMessage(messageTarget, error.message || "Request failed", true);
    }
  }

  async function refreshStatus() {
    const status = await fetchJson("/api/status");
    lastStatus = status;
    updateBadges(status);
    updateMetrics(status);
    updateSummary(status);
    setButtonState(status);
  }

  async function refreshLogs() {
    try {
      const payload = await fetchJson("/api/logs");
      renderLogs(payload.logs || [], payload.storage || {});
    } catch (error) {
      el.logsCount.textContent = "Unavailable";
      renderStorage({});
      clearLogsTable("Log list is unavailable.");
    }
  }

  async function refreshAll() {
    try {
      await refreshStatus();
      await refreshLogs();
    } catch (error) {
      el.deviceBadge.innerHTML = '<span class="dot"></span>Device offline';
      el.lastSync.textContent = "Last sync failed";
      setMessage(el.message, error.message || "Unable to reach device", true);
    }
  }

  function parseCsv(text) {
    const lines = text.replace(/\r/g, "").split("\n").filter(function (line) {
      return line.trim().length > 0;
    });
    if (lines.length < 2) {
      return [];
    }

    const headers = lines[0].split(",").map(function (header) {
      return header.trim();
    });
    const indexes = {};
    headers.forEach(function (header, index) {
      indexes[header] = index;
    });
    if (indexes.timestamp_ms === undefined) {
      return [];
    }

    const rows = [];
    for (let i = 1; i < lines.length; ++i) {
      const values = lines[i].split(",");
      const timestamp = Number(values[indexes.timestamp_ms]);
      if (!Number.isFinite(timestamp)) {
        continue;
      }
      const row = { timestamp_ms: timestamp };
      METRICS.forEach(function (metric) {
        const index = indexes[metric.key];
        row[metric.key] = index === undefined ? NaN : Number(values[index]);
      });
      rows.push(row);
    }
    return rows;
  }

  function downsample(rows) {
    if (rows.length <= MAX_CHART_POINTS) {
      return rows;
    }
    const step = Math.ceil(rows.length / MAX_CHART_POINTS);
    const sampled = [];
    for (let i = 0; i < rows.length; i += step) {
      sampled.push(rows[i]);
    }
    if (sampled[sampled.length - 1] !== rows[rows.length - 1]) {
      sampled.push(rows[rows.length - 1]);
    }
    return sampled;
  }

  function formatDuration(ms) {
    const seconds = Math.max(0, ms / 1000);
    if (seconds >= 3600) {
      return (seconds / 3600).toFixed(1) + " h";
    }
    if (seconds >= 60) {
      return (seconds / 60).toFixed(1) + " min";
    }
    return seconds.toFixed(0) + " s";
  }

  function metricValue(value, metric) {
    return formatNumber(value, metric.digits) + " " + metric.unit;
  }

  function renderChartCards(rows) {
    chartState = [];
    el.chartsGrid.textContent = "";

    METRICS.forEach(function (metric) {
      const card = document.createElement("article");
      const top = document.createElement("div");
      const title = document.createElement("h3");
      const range = document.createElement("span");
      const canvas = document.createElement("canvas");
      const note = document.createElement("p");

      card.className = "chart-card";
      top.className = "chart-top";
      title.textContent = metric.label;
      range.className = "chart-range";
      canvas.className = "chart-canvas";
      note.className = "chart-note";

      top.appendChild(title);
      top.appendChild(range);
      card.appendChild(top);
      card.appendChild(canvas);
      card.appendChild(note);
      el.chartsGrid.appendChild(card);
      chartState.push({ canvas: canvas, metric: metric, range: range, note: note, rows: rows });
    });

    drawAllCharts();
  }

  function drawAllCharts() {
    chartState.forEach(drawChart);
  }

  function drawChart(state) {
    const canvas = state.canvas;
    const rect = canvas.getBoundingClientRect();
    const width = Math.max(280, Math.floor(rect.width));
    const height = Math.max(220, Math.floor(rect.height || 220));
    const dpr = window.devicePixelRatio || 1;
    const ctx = canvas.getContext("2d");
    const plotRows = downsample(state.rows).filter(function (row) {
      return Number.isFinite(row[state.metric.key]);
    });

    canvas.width = Math.floor(width * dpr);
    canvas.height = Math.floor(height * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, width, height);
    ctx.fillStyle = "#fffef9";
    ctx.fillRect(0, 0, width, height);

    const pad = { left: 52, right: 14, top: 16, bottom: 34 };
    const innerW = Math.max(1, width - pad.left - pad.right);
    const innerH = Math.max(1, height - pad.top - pad.bottom);

    ctx.strokeStyle = "#dce7d5";
    ctx.lineWidth = 1;
    ctx.font = "11px system-ui, sans-serif";
    ctx.fillStyle = "#667468";
    for (let i = 0; i <= 4; ++i) {
      const y = pad.top + (innerH * i) / 4;
      ctx.beginPath();
      ctx.moveTo(pad.left, y);
      ctx.lineTo(width - pad.right, y);
      ctx.stroke();
    }

    if (!plotRows.length) {
      state.range.textContent = "--";
      state.note.textContent = "No valid data";
      ctx.fillText("No valid data", pad.left, pad.top + 24);
      return;
    }

    const startMs = state.rows[0].timestamp_ms;
    const minX = 0;
    const maxX = Math.max(1, state.rows[state.rows.length - 1].timestamp_ms - startMs);
    let minY = Infinity;
    let maxY = -Infinity;
    plotRows.forEach(function (row) {
      const value = row[state.metric.key];
      minY = Math.min(minY, value);
      maxY = Math.max(maxY, value);
    });
    if (minY === maxY) {
      minY -= 1;
      maxY += 1;
    }
    const paddingY = (maxY - minY) * 0.08;
    minY -= paddingY;
    maxY += paddingY;

    function xFor(row) {
      return pad.left + (((row.timestamp_ms - startMs) - minX) / (maxX - minX)) * innerW;
    }

    function yFor(row) {
      return pad.top + (1 - ((row[state.metric.key] - minY) / (maxY - minY))) * innerH;
    }

    ctx.strokeStyle = state.metric.color;
    ctx.lineWidth = 2.4;
    ctx.beginPath();
    plotRows.forEach(function (row, index) {
      const x = xFor(row);
      const y = yFor(row);
      if (index === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    });
    ctx.stroke();

    if (plotRows.length <= 30) {
      ctx.fillStyle = state.metric.color;
      plotRows.forEach(function (row) {
        ctx.beginPath();
        ctx.arc(xFor(row), yFor(row), 3, 0, Math.PI * 2);
        ctx.fill();
      });
    }

    ctx.strokeStyle = "#9fb394";
    ctx.beginPath();
    ctx.moveTo(pad.left, pad.top);
    ctx.lineTo(pad.left, height - pad.bottom);
    ctx.lineTo(width - pad.right, height - pad.bottom);
    ctx.stroke();

    ctx.fillStyle = "#667468";
    ctx.fillText(metricValue(maxY - paddingY, state.metric), 4, pad.top + 4);
    ctx.fillText(metricValue(minY + paddingY, state.metric), 4, height - pad.bottom);
    ctx.fillText("0 s", pad.left, height - 10);
    ctx.fillText(formatDuration(maxX), width - pad.right - 54, height - 10);

    state.range.textContent = metricValue(minY + paddingY, state.metric) + " - " + metricValue(maxY - paddingY, state.metric);
    state.note.textContent = plotRows.length < state.rows.length ? plotRows.length + " points shown" : plotRows.length + " samples";
  }

  async function loadGraphPage() {
    const params = new URLSearchParams(window.location.search);
    const slot = Number(params.get("slot"));
    if (!Number.isInteger(slot) || slot < 0 || slot > 4) {
      throw new Error("Invalid log slot");
    }

    const payload = await fetchJson("/api/logs");
    const logs = payload.logs || [];
    const log = logs.find(function (item) {
      return Number(item.slot) === slot;
    });
    if (!log) {
      throw new Error("Log is not available");
    }

    el.graphTitle.textContent = log.name || "Log graphs";
    el.graphMeta.textContent = formatBytes(log.size_bytes || 0);
    setMessage(el.graphMessage, "Loading...", false);

    const csv = await fetchText("/api/log-data?slot=" + encodeURIComponent(slot));
    const rows = parseCsv(csv);
    if (!rows.length) {
      el.chartsGrid.textContent = "";
      setMessage(el.graphMessage, "This log has no chartable rows.", true);
      return;
    }

    el.graphMeta.textContent = formatBytes(log.size_bytes || 0) + " | " + rows.length + (rows.length === 1 ? " sample" : " samples");
    setMessage(el.graphMessage, "", false);
    renderChartCards(rows);
  }

  function initDashboard() {
    el.readNowButton.addEventListener("click", function () {
      postCommand("/read-now", "Reading requested");
    });

    if (isIOS) {
      lockLogNameInput();

      const activateNameField = function (event) {
        if (lastStatus && lastStatus.logging) {
          return;
        }
        if (!logNameManualEditArmed) {
          event.preventDefault();
          armLogNameManualEdit();
          try {
            el.logNameInput.focus({ preventScroll: true });
          } catch (focusError) {
            el.logNameInput.focus();
          }
        }
      };

      if (el.logNameField) {
        el.logNameField.addEventListener("touchstart", activateNameField, { passive: false });
        el.logNameField.addEventListener("pointerdown", activateNameField);
        el.logNameField.addEventListener("mousedown", activateNameField);
      }

      el.logNameInput.addEventListener("focus", function () {
        if (logNameManualEditArmed) {
          unlockLogNameInput();
          return;
        }
        window.setTimeout(function () {
          if (document.activeElement === el.logNameInput) {
            el.logNameInput.blur();
          }
          lockLogNameInput();
        }, 0);
      });

      el.logNameInput.addEventListener("blur", function () {
        resetLogNameManualEdit();
        lockLogNameInput();
      });
    } else {
      unlockLogNameInput();
    }

    el.logNameInput.addEventListener("input", function () {
      const nameState = updateLogNameHint(logNameSubmitAttempted);
      if (nameState.ok) {
        logNameSubmitAttempted = false;
      }
      if (lastStatus) {
        setButtonState(lastStatus);
      }
    });

    el.startButton.addEventListener("click", function () {
      const nameState = requireLogName();
      if (!nameState.ok) {
        setMessage(el.message, nameState.error, true);
        if (isIOS) {
          armLogNameManualEdit();
        }
        el.logNameInput.focus();
        return;
      }

      let seconds = parseInt(el.intervalInput.value, 10);
      if (!Number.isFinite(seconds) || seconds < 1) {
        seconds = 1;
        el.intervalInput.value = "1";
      }

      const url = "/start?interval=" + encodeURIComponent(seconds) + "&name=" + encodeURIComponent(nameState.name);
      postCommand(url, "Logging started");
    });

    el.stopButton.addEventListener("click", function () {
      postCommand("/stop", "Logging stopped");
    });

    el.clearButton.addEventListener("click", function () {
      postCommand("/clear", "Log cleared");
    });

    el.captureDryButton.addEventListener("click", function () {
      postCommand("/calibrate-dry", "Dry point captured", el.calibrationMessage);
    });

    el.captureWetButton.addEventListener("click", function () {
      postCommand("/calibrate-wet", "Wet point captured", el.calibrationMessage);
    });

    el.clearCalibrationButton.addEventListener("click", function () {
      postCommand("/calibrate-clear", "Calibration cleared", el.calibrationMessage);
    });

    refreshAll();
    setInterval(refreshAll, POLL_MS);
  }

  function init() {
    window.addEventListener("pageshow", hidePageLoader);
    document.body.classList.toggle("graph-page", isGraphPage);
    el.dashboardView.hidden = isGraphPage;
    el.graphView.hidden = !isGraphPage;

    if (isGraphPage) {
      loadGraphPage().catch(function (error) {
        el.deviceBadge.innerHTML = '<span class="dot"></span>Device offline';
        setMessage(el.graphMessage, error.message || "Unable to load graphs", true);
      });
      window.addEventListener("resize", function () {
        window.clearTimeout(drawAllCharts._timer);
        drawAllCharts._timer = window.setTimeout(drawAllCharts, 120);
      });
      return;
    }

    initDashboard();
  }

  init();
})();
