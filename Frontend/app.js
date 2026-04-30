(function () {
  const POLL_MS = 5000;

  const el = {
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
    intervalInput: document.getElementById("intervalInput"),
    readNowButton: document.getElementById("readNowButton"),
    startButton: document.getElementById("startButton"),
    stopButton: document.getElementById("stopButton"),
    downloadLatestButton: document.getElementById("downloadLatestButton"),
    clearButton: document.getElementById("clearButton"),
    captureDryButton: document.getElementById("captureDryButton"),
    captureWetButton: document.getElementById("captureWetButton"),
    clearCalibrationButton: document.getElementById("clearCalibrationButton"),
    message: document.getElementById("message"),
    dryValue: document.getElementById("dryValue"),
    wetValue: document.getElementById("wetValue"),
    calibrationMessage: document.getElementById("calibrationMessage"),
    logsCount: document.getElementById("logsCount"),
    logsBody: document.getElementById("logsBody")
  };

  let lastStatus = null;

  function setMessage(target, text, isError) {
    target.textContent = text || "";
    target.classList.toggle("error", !!isError);
  }

  function formatNumber(value, digits) {
    if (typeof value !== "number" || !Number.isFinite(value)) {
      return "--";
    }
    return value.toFixed(digits);
  }

  function formatBytes(value) {
    if (!value) {
      return "0 B";
    }
    if (value < 1024) {
      return value + " B";
    }
    return (value / 1024).toFixed(1) + " KB";
  }

  function setButtonState(status) {
    const logging = !!status.logging;
    const canDownload = !logging && !!status.download_available;

    el.intervalInput.disabled = logging;
    el.startButton.disabled = logging;
    el.stopButton.disabled = !logging;
    el.downloadLatestButton.disabled = !canDownload;
    el.clearButton.disabled = logging;
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
    cell.colSpan = 4;
    cell.className = "empty-cell";
    cell.textContent = message;
    row.appendChild(cell);
    el.logsBody.appendChild(row);
  }

  function renderLogs(logs) {
    el.logsCount.textContent = logs.length + (logs.length === 1 ? " file" : " files");
    el.logsBody.textContent = "";

    if (!logs.length) {
      clearLogsTable("No completed logs yet.");
      return;
    }

    logs.forEach(function (log) {
      const row = document.createElement("tr");
      const fileCell = document.createElement("td");
      const slotCell = document.createElement("td");
      const sizeCell = document.createElement("td");
      const actionCell = document.createElement("td");
      const link = document.createElement("a");

      fileCell.textContent = log.filename || ("log_" + log.slot + ".csv");
      if (log.is_latest) {
        const tag = document.createElement("span");
        tag.className = "latest-tag";
        tag.textContent = "latest";
        fileCell.appendChild(tag);
      }

      slotCell.textContent = String(log.slot);
      sizeCell.textContent = formatBytes(log.size_bytes || 0);
      link.className = "download-link";
      link.href = "/download.csv?slot=" + encodeURIComponent(log.slot);
      link.textContent = "Download";
      if (lastStatus && lastStatus.logging) {
        link.setAttribute("aria-disabled", "true");
        link.addEventListener("click", function (event) {
          event.preventDefault();
        });
      }
      actionCell.appendChild(link);

      row.appendChild(fileCell);
      row.appendChild(slotCell);
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
      renderLogs(payload.logs || []);
    } catch (error) {
      el.logsCount.textContent = "Unavailable";
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

  el.readNowButton.addEventListener("click", function () {
    postCommand("/read-now", "Reading requested");
  });

  el.startButton.addEventListener("click", function () {
    let seconds = parseInt(el.intervalInput.value, 10);
    if (!Number.isFinite(seconds) || seconds < 1) {
      seconds = 1;
      el.intervalInput.value = "1";
    }
    postCommand("/start?interval=" + encodeURIComponent(seconds), "Logging started");
  });

  el.stopButton.addEventListener("click", function () {
    postCommand("/stop", "Logging stopped");
  });

  el.downloadLatestButton.addEventListener("click", function () {
    window.location.href = "/download.csv";
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
})();
