/*************************************************************************
 * EduGrid UI script (instant feedback + drag-friendly)
 *************************************************************************/

/* ---- GET helper ---- */
function sendUpdate(id, state, state2) {
  var url = "/updatevalues?ID=" + encodeURIComponent(id) + "&STATE=" + encodeURIComponent(state || "");
  if (state2 !== undefined) url += "&STATE2=" + encodeURIComponent(state2);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.send();
}

/* Keep compatibility with inline HTML handlers */
function toggleSwitch(element) { sendUpdate(element.id, 1); }

/* Throttle slider sends to ~40ms to avoid flooding */
let _sliderTimer = null;
let _sliderPending = null;
function toggleSlider(element) {
  _sliderPending = element.value;
  if (_sliderTimer) return;
  _sliderTimer = setTimeout(function () {
    sendUpdate(element.id, _sliderPending);
    _sliderTimer = null;
  }, 40);
}

/* Click handler for labels (MODE is special) */
function labelHit(element) {
  if (!element) return;
  if (element.id === 'mode_label') {
    // Deterministic toggle: ask firmware for the OTHER regular mode explicitly
    const cur = (element.textContent || '').trim().toUpperCase();
    const target = (cur === 'AUTO') ? 'MANUAL' : 'AUTO';
    sendUpdate('mode_label', target);
  } else {
    // Default behavior for other clickable labels
    const val = (element.textContent || element.value || '').toString();
    sendUpdate(element.id, val);
  }
}

/*************************************************************************
 * WebSocket: dynamic URL (works in AP or STA)
 *************************************************************************/
let socket = null;
let reconnectTimer = null;

/* Pause overwriting slider while user drags it */
let isDraggingSlider = false;

function connectWS() {
  const wsScheme = (location.protocol === 'https:') ? 'wss://' : 'ws://';
  const wsUrl = wsScheme + location.hostname + ':81/';
  socket = new WebSocket(wsUrl);

  socket.onopen = function () {
    if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
  };
  socket.onclose = function () {
    if (!reconnectTimer) reconnectTimer = setTimeout(connectWS, 2000);
  };
  socket.onerror = function () { try { socket.close(); } catch (e) {} };

  socket.onmessage = function (event) {
    try {
      const j = JSON.parse(event.data);

      // ---------- helpers ----------
      const el = (id) => document.getElementById(id);
      const setText = (id, val) => { const e = el(id); if (e) e.textContent = val; };
      const fmtV = v => Number.isFinite(v) ? v.toFixed(2) + " V" : "--";
      const fmtA = a => Number.isFinite(a) ? a.toFixed(3) + " A" : "--";
      const fmtW = w => Number.isFinite(w) ? w.toFixed(2) + " W" : "--";
      const fmtPct = x => Number.isFinite(x) ? x.toFixed(1) + " %" : "--";

      // ---------- PWM / Frequency ----------
      const pwmValue = Number(j.pwm);
      if (el("pwm_label")) {
        setText("pwm_label", Number.isFinite(pwmValue) ? pwmValue.toFixed(0) + " %" : "--");
      }

      const freqHz  = Number(j.freq_hz);
      if (el("freq_label")) {
        if (Number.isFinite(freqHz)) {
          const freqDisplay = freqHz >= 100 ? freqHz.toFixed(0) : freqHz.toFixed(1);
          setText("freq_label", freqDisplay + " Hz");
        } else {
          setText("freq_label", "--");
        }
      }

      const slider = el("4");
      if (slider) {
        if (j.pwm_min !== undefined) slider.min = j.pwm_min;
        if (j.pwm_max !== undefined) slider.max = j.pwm_max;
        if (!isDraggingSlider && Number.isFinite(pwmValue)) slider.value = pwmValue;
      }

      // ---------- MODE label ----------
      // Server now sends "AUTO" / "MANUAL" / "IV_SWEEP" (strings).
      const modeLabel = el("mode_label");
      if (modeLabel) {
        if (typeof j.mode === "string") {
          // Normalize MANUALLY -> MANUAL just in case
          const m = j.mode.toUpperCase().replace("MANUALLY", "MANUAL");
          modeLabel.textContent = m;
        } else if (j.mode === 1) {
          modeLabel.textContent = "AUTO";
        } else if (j.mode === 0) {
          modeLabel.textContent = "MANUAL";
        } else {
          modeLabel.textContent = String(j.mode ?? "--");
        }
      }

      // ---------- Measurements ----------
      const vin  = Number(j.vin),  iin  = Number(j.iin);
      const vout = Number(j.vout), iout = Number(j.iout);
      const pin  = Number(j.pin),  pout = Number(j.pout);
      const eff  = Number(j.eff) * 100; // convert to %

      setText("voltage_in_label",  fmtV(vin));
      setText("current_in_label",  fmtA(iin));
      setText("voltage_out_label", fmtV(vout));
      setText("current_out_label", fmtA(iout));
      setText("power_in_label",    fmtW(pin));
      setText("power_out_label",   fmtW(pout));
      setText("efficiency_label",  fmtPct(eff));

      updateLiveOperatingPoints(vin, iin);

      // ---------- Misc ----------
      setText("logging_label",    j.logging ?? "--");

    } catch (e) {
      // console.error("[WS] JSON parse error:", e);
    }
  };
}

/* Add drag guards so WS doesn’t fight the user */
function attachDragGuards() {
  const slider = document.getElementById("4");
  if (!slider) return;
  const start = () => { isDraggingSlider = true;  };
  const stop  = () => { isDraggingSlider = false; };
  // mouse
  slider.addEventListener('mousedown', start);
  slider.addEventListener('mouseup',   stop);
  slider.addEventListener('mouseleave',stop);
  // touch
  slider.addEventListener('touchstart', start, {passive:true});
  slider.addEventListener('touchend',   stop,  {passive:true});
  slider.addEventListener('touchcancel',stop,  {passive:true});
}

/* Optional: only attaches the click; we do NOT change the text locally */
function attachInstantModeToggle() {
  const modeLabel = document.getElementById("mode_label");
  if (!modeLabel) return;
  modeLabel.style.cursor = "pointer";
  modeLabel.addEventListener('click', function () {
    labelHit(modeLabel); // send explicit AUTO/MANUAL target; WS will update text
  });
}

window.addEventListener('load', function() {
  attachDragGuards();
  attachInstantModeToggle();
  connectWS();
});

/* === IV CHART === */
let ivChart = null;
let liveOperatingPoints = [];

function applyLivePointsToChart() {
  if (!ivChart || !ivChart.data || !ivChart.data.datasets) return;
  const ds = ivChart.data.datasets[3];
  if (!ds) return;
  ds.data = liveOperatingPoints.map(pt => ({ x: pt.x, y: pt.y }));
}

function updateLiveOperatingPoints(voltage, current) {
  if (!Number.isFinite(voltage) || !Number.isFinite(current)) return;
  liveOperatingPoints.push({ x: voltage, y: current });
  if (liveOperatingPoints.length > 10) {
    liveOperatingPoints.splice(0, liveOperatingPoints.length - 10);
  }
  applyLivePointsToChart();
  if (ivChart) ivChart.update('none');
}

function initIvChart() {
  const el = document.getElementById('ivChart');
  if (!el) return;
  const ctx = el.getContext('2d');

  ivChart = new Chart(ctx, {
    type: 'scatter',
    data: {
      datasets: [
        // I-V curve (amps on left axis)
        {
          label: 'I-V',
          data: [],
          yAxisID: 'yI',
          showLine: true,
          cubicInterpolationMode: 'monotone',
          tension: 0.4,
          pointRadius: 2
        },
        // P-V curve (watts on right axis)
        {
          label: 'P-V',
          data: [],
          yAxisID: 'yP',
          showLine: true,
          cubicInterpolationMode: 'monotone',
          tension: 0.4,
          pointRadius: 0
        },
        // MPP marker on the I-V plot (single point)
        {
          label: 'MPP',
          data: [],
          yAxisID: 'yI',
          showLine: false,
          pointRadius: 10,   // bigger
          hoverRadius: 12,
          hitRadius: 14,
          borderWidth: 3,    // thicker outline
          pointStyle: 'cross',
          order: 999
        },
        // Live operating point history (last 10 samples)
        {
          label: 'Live PV',
          data: [],
          yAxisID: 'yI',
          showLine: false,
          pointRadius: ctx => (ctx.dataIndex === ctx.dataset.data.length - 1 ? 7 : 4),
          pointHoverRadius: 8,
          pointBorderWidth: 1,
          pointBorderColor: 'rgba(0, 200, 83, 1)',
          pointBackgroundColor: ctx => (ctx.dataIndex === ctx.dataset.data.length - 1
            ? 'rgba(0, 200, 83, 0.95)'
            : 'rgba(0, 200, 83, 0.35)'),
          hitRadius: 10,
          order: 998
        }
      ]
    },
    options: {
      animation: false,
      parsing: false,     // we supply {x,y} objects directly
      spanGaps: true,
      normalized: true,
      scales: {
        x:  { title: { display: true, text: 'Voltage [V]' } },
        yI: { type: 'linear', position: 'left',  title: { display: true, text: 'Current [A]' } },
        yP: { type: 'linear', position: 'right', title: { display: true, text: 'Power [W]' }, grid: { drawOnChartArea: false } }
      },
      plugins: { legend: { display: true } }
    }
  });

  applyLivePointsToChart();
}

async function pollIvData() {
  try {
    const res = await fetch('/ivsweep/data', { cache: 'no-cache' });
    const j = await res.json();

    const v = Array.isArray(j.v) ? j.v : [];
    const i = Array.isArray(j.i) ? j.i : [];
    const p = Array.isArray(j.p) ? j.p : [];

    // Build raw point arrays
    const rawIV = [];
    const rawPV = [];
    for (let k = 0; k < v.length; k++) {
      const vx = Number(v[k]);
      const iy = Number(i[k]);
      const py = Number(p[k]);
      if (Number.isFinite(vx) && Number.isFinite(iy)) rawIV.push({ x: vx, y: iy });
      if (Number.isFinite(vx) && Number.isFinite(py)) rawPV.push({ x: vx, y: py });
    }

    // Sort by voltage (ensures the line is monotone in X)
    rawIV.sort((a, b) => a.x - b.x);
    rawPV.sort((a, b) => a.x - b.x);

    // --- Find MPP (max of P array) ---
    let mppIdx = -1, mppP = -Infinity;
    for (let k = 0; k < p.length; k++) {
      const pk = Number(p[k]);
      if (Number.isFinite(pk) && pk > mppP) { mppP = pk; mppIdx = k; }
    }
    const mppPointIV = (mppIdx >= 0)
      ? [{ x: Number(v[mppIdx]) || 0, y: Number(i[mppIdx]) || 0 }]
      : [];

    // Update datasets
    if (ivChart) {
      ivChart.data.datasets[0].data = rawIV;      // I-V
      ivChart.data.datasets[1].data = rawPV;      // P-V
      ivChart.data.datasets[2].data = mppPointIV; // MPP marker (I-V axis)
      ivChart.update();
    }

    // Update textual MPP line (ASCII to avoid encoding issues)
    const info = document.getElementById('mppInfo');
    if (info) {
      if (mppIdx >= 0) {
        const vv = Number(v[mppIdx]) || 0;
        const ii = Number(i[mppIdx]) || 0;
        const pp = Number(p[mppIdx]) || 0;
        info.textContent = `MPP ~ ${vv.toFixed(2)} V, ${ii.toFixed(2)} A  ->  ${pp.toFixed(2)} W`;
      } else {
        info.textContent = '';
      }
    }

    // keep polling while sweep not finished
    if (!j.done) setTimeout(pollIvData, 120);
  } catch (e) {
    setTimeout(pollIvData, 250);
  }
}

async function startIvSweep() {
  try {
    await fetch('/ivsweep/start', { cache: 'no-cache' });
    pollIvData();
  } catch (e) {
    alert('Failed to start IV sweep');
  }
}

// hook up on page load
window.addEventListener('load', () => {
  initIvChart();
  const b = document.getElementById('ivStartBtn');
  if (b) b.addEventListener('click', startIvSweep);
});
