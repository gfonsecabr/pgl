"use strict";

let DB = null;
let chart = null;
let pop = null; // hover preview bubble

async function load() {
  const res = await fetch("data.json", { cache: "no-store" });
  DB = await res.json();

  const sel = document.getElementById("machine");
  if (!DB.machines.length) {
    document.getElementById("suites").innerHTML =
      '<p class="empty-state">No benchmark data recorded yet. Run ' +
      "<code>bash benchmark/record.sh</code> and commit " +
      "<code>benchmark/history/</code>.</p>";
    return;
  }
  for (const m of DB.machines) {
    const o = document.createElement("option");
    o.value = o.textContent = m;
    sel.appendChild(o);
  }
  sel.addEventListener("change", render);
  document.getElementById("chart-close")
    .addEventListener("click", () => document.getElementById("chart-dialog").close());

  pop = document.getElementById("spark-pop");

  const suiteCount = Object.keys(DB.suites).length;
  let seriesCount = 0;
  for (const s of Object.values(DB.suites)) {
    seriesCount += s.functions.length * s.types.length;
  }
  const g = DB.generated ? new Date(DB.generated) : null;
  document.getElementById("summary").innerHTML =
    `<span><b>${suiteCount}</b> suites</span>` +
    `<span><b>${seriesCount}</b> series</span>` +
    `<span><b>${DB.machines.length}</b> machine${DB.machines.length > 1 ? "s" : ""}</span>` +
    (g ? `<span>updated <b>${g.toLocaleDateString()}</b></span>` : "");

  render();
}

function fmt(t) {
  if (t === null || t === undefined) return "—";
  if (t >= 100) return t.toFixed(0);
  if (t >= 10) return t.toFixed(1);
  if (t >= 1) return t.toFixed(2);
  return t.toFixed(3);
}

const latest = (pts) => (pts && pts.length ? pts[pts.length - 1] : null);
const best = (pts) => Math.min(...pts.map((p) => p.time));
const worst = (pts) => Math.max(...pts.map((p) => p.time));

// Friendlier column headers for the longer type keys.
const TYPE_LABEL = {
  rationalbigint: "rational big",
  rationalbigint60: "rational big/60",
};
const typeLabel = (t) => TYPE_LABEL[t] || t;

// Status colour on an absolute scale relative to the best (lo): green within
// `margin` of the best, ramping through amber to full red once the value is
// `redAt` above the best — independent of the worst ever seen.
function statusColor(cur, lo, hi, alpha) {
  const margin = 0.10; // within 10% of best => still "best" (green)
  const redAt = 1.0;   // 100% above best (2x) => full red
  let frac = 0;
  if (lo > 0) {
    const excess = (cur - lo) / lo;
    if (excess > margin) {
      frac = (excess - margin) / (redAt - margin);
      frac = Math.max(0, Math.min(1, frac));
    }
  }
  const hue = 130 * (1 - frac); // 130 green -> 65 amber -> 0 red
  return `hsl(${hue.toFixed(0)} 68% ${alpha ? "45% / " + alpha : "38%"})`;
}

function th(text, cls) {
  const e = document.createElement("th");
  e.textContent = text;
  if (cls) e.className = cls;
  return e;
}

// Tiny inline SVG sparkline with faint best/worst guides — built on hover.
function sparkline(points, w, h) {
  const times = points.map((p) => p.time);
  const lo = Math.min(...times), hi = Math.max(...times);
  const span = hi - lo || 1;
  const n = points.length;
  const x = (i) => (n === 1 ? w / 2 : (i / (n - 1)) * (w - 8) + 4);
  const y = (v) => h - 4 - ((v - lo) / span) * (h - 8);
  const poly = points.map((p, i) => `${x(i).toFixed(1)},${y(p.time).toFixed(1)}`).join(" ");
  const lp = points[n - 1];
  const cx = x(n - 1).toFixed(1), cy = y(lp.time).toFixed(1);
  return (
    `<svg width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">` +
    `<line x1="0" y1="${y(lo).toFixed(1)}" x2="${w}" y2="${y(lo).toFixed(1)}" stroke="#1a7f37" stroke-width="1" stroke-dasharray="2 2" opacity=".5"/>` +
    `<line x1="0" y1="${y(hi).toFixed(1)}" x2="${w}" y2="${y(hi).toFixed(1)}" stroke="#cf222e" stroke-width="1" stroke-dasharray="2 2" opacity=".5"/>` +
    `<polyline fill="none" stroke="#0a429e" stroke-width="1.5" points="${poly}"/>` +
    `<circle cx="${cx}" cy="${cy}" r="2.8" fill="${statusColor(lp.time, lo, hi)}"/>` +
    `</svg>`
  );
}

// Compact inline sparkline shown inside every cell — muted line, status dot.
function cellSpark(points, w, h) {
  const times = points.map((p) => p.time);
  const lo = Math.min(...times), hi = Math.max(...times);
  const span = hi - lo || 1;
  const n = points.length;
  const x = (i) => (n === 1 ? w / 2 : (i / (n - 1)) * (w - 4) + 2);
  const y = (v) => h - 2 - ((v - lo) / span) * (h - 4);
  const poly = points.map((p, i) => `${x(i).toFixed(1)},${y(p.time).toFixed(1)}`).join(" ");
  const lp = points[n - 1];
  return (
    `<svg class="cell-spark" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">` +
    `<polyline fill="none" stroke="#c2ccd9" stroke-width="1.25" points="${poly}"/>` +
    `<circle cx="${x(n - 1).toFixed(1)}" cy="${y(lp.time).toFixed(1)}" r="1.9" fill="${statusColor(lp.time, lo, hi)}"/>` +
    `</svg>`
  );
}

function render() {
  const machine = document.getElementById("machine").value;
  const root = document.getElementById("suites");
  root.innerHTML = "";

  for (const name of Object.keys(DB.suites).sort()) {
    const s = DB.suites[name];
    const machineData = s.data[machine] || {};

    const section = document.createElement("section");
    section.className = "suite";

    const h = document.createElement("h2");
    h.innerHTML = s.source_url
      ? `<a class="suite-link" href="${s.source_url}" target="_blank" rel="noopener">${name}</a>`
      : name;
    if (s.description) {
      const desc = document.createElement("span");
      desc.className = "suite-desc";
      desc.textContent = s.description;
      h.appendChild(desc);
    }
    section.appendChild(h);

    const table = document.createElement("table");
    const thead = document.createElement("thead");
    const hr = document.createElement("tr");
    hr.appendChild(th("Function"));
    for (const t of s.types) hr.appendChild(th(typeLabel(t), "num"));
    thead.appendChild(hr);
    table.appendChild(thead);

    const tbody = document.createElement("tbody");
    let any = false;
    for (const op of s.functions) {
      const tr = document.createElement("tr");
      const fn = document.createElement("td");
      fn.className = "fn";
      fn.textContent = op;
      tr.appendChild(fn);

      for (const t of s.types) {
        const td = document.createElement("td");
        td.className = "val";
        const pts = machineData[op + "|" + t];
        const lp = latest(pts);
        if (lp) {
          any = true;
          const lo = best(pts), hi = worst(pts);
          const heavy = hi > lo && (lp.time - lo) / lo > 1.0 ? " heavy" : "";
          const spark = pts.length > 1 ? cellSpark(pts, 52, 16) : "";
          td.innerHTML =
            `<span class="cell">${spark}` +
            `<span class="num${heavy}" style="color:${statusColor(lp.time, lo, hi)}">${fmt(lp.time)}</span>` +
            `</span>`;
          td.classList.add("clickable");
          td.addEventListener("click", () => showChart(name, op, t, machine, pts, s.unit));
          td.addEventListener("mouseenter", (e) => showPop(e, op, t, pts, s.unit));
          td.addEventListener("mouseleave", hidePop);
        } else {
          td.textContent = "—";
          td.classList.add("empty");
        }
        tr.appendChild(td);
      }
      tbody.appendChild(tr);
    }
    table.appendChild(tbody);
    const tableWrap = document.createElement("div");
    tableWrap.className = "table-wrap";
    tableWrap.appendChild(table);
    section.appendChild(tableWrap);

    const note = document.createElement("div");
    note.className = "unit-note";
    note.textContent = any
      ? "time in " + s.unit + " · hover for trend, click for full chart"
      : "no data for this machine";
    section.appendChild(note);

    root.appendChild(section);
  }
}

function showPop(event, op, type, points, unit) {
  const lo = best(points), hi = worst(points), cur = latest(points).time;
  const delta = lo > 0 ? ((cur - lo) / lo) * 100 : 0;
  pop.innerHTML =
    `<div class="pop-title">${op} · ${typeLabel(type)}</div>` +
    sparkline(points, 168, 46) +
    `<div class="pop-stats">` +
    `<span style="color:${statusColor(cur, lo, hi)}">now ${fmt(cur)}` +
    (delta > 0.05 ? ` (+${delta.toFixed(1)}%)` : "") + `</span>` +
    `<span class="muted">best ${fmt(lo)} · ${points.length} pts</span>` +
    `</div>`;
  pop.style.display = "block";
  const r = event.currentTarget.getBoundingClientRect();
  const px = Math.min(r.left + window.scrollX, window.scrollX + window.innerWidth - 196);
  pop.style.left = px + "px";
  pop.style.top = r.bottom + window.scrollY + 6 + "px";
}

function hidePop() {
  if (pop) pop.style.display = "none";
}

function showChart(suite, op, type, machine, points, unit) {
  hidePop();
  const dialog = document.getElementById("chart-dialog");
  document.getElementById("chart-title").textContent =
    suite + " · " + op + " · " + typeLabel(type) + " — " + machine;

  const n = points.length;
  const lo = best(points), hi = worst(points);
  const labels = points.map((p) => (p.date ? p.date.slice(0, 10) : "") + "\n" + p.commit);
  const values = points.map((p) => p.time);

  const pointColors = values.map((v, i) =>
    i === n - 1 ? "#0a429e" : statusColor(v, lo, hi));
  const pointRadii = values.map((v, i) =>
    i === n - 1 ? 6 : v === lo || v === hi ? 5 : 3);

  // Progressive left-to-right draw: each point eases in from the previous
  // point's position, so the line traces itself instead of dropping in.
  const totalDuration = 900;
  const stepDuration = totalDuration / Math.max(n, 1);
  const previousY = (ctx) => {
    const meta = ctx.chart.getDatasetMeta(ctx.datasetIndex);
    const prev = meta.data[ctx.index === 0 ? 0 : ctx.index - 1];
    return prev
      ? prev.getProps(["y"], true).y
      : ctx.chart.scales.y.getPixelForValue(ctx.chart.scales.y.min);
  };
  const drawAnimation = {
    x: {
      type: "number",
      easing: "linear",
      duration: stepDuration,
      from: NaN,
      delay(ctx) {
        if (ctx.type !== "data" || ctx.xStarted) return 0;
        ctx.xStarted = true;
        return ctx.index * stepDuration;
      },
    },
    y: {
      type: "number",
      easing: "linear",
      duration: stepDuration,
      from: previousY,
      delay(ctx) {
        if (ctx.type !== "data" || ctx.yStarted) return 0;
        ctx.yStarted = true;
        return ctx.index * stepDuration;
      },
    },
  };

  if (chart) chart.destroy();
  chart = new Chart(document.getElementById("chart-canvas"), {
    type: "line",
    data: {
      labels,
      datasets: [
        {
          label: "time (" + unit + ")",
          data: values,
          borderColor: "#0a429e",
          backgroundColor: "rgba(10,66,158,.10)",
          pointBackgroundColor: pointColors,
          pointBorderColor: "#fff",
          pointBorderWidth: 1,
          pointRadius: pointRadii,
          pointHoverRadius: 7,
          tension: 0.25,
          fill: true,
          order: 1,
        },
        {
          label: "best " + fmt(lo),
          data: values.map(() => lo),
          borderColor: "#1a7f37",
          borderDash: [6, 4],
          borderWidth: 1.5,
          pointRadius: 0,
          fill: false,
          order: 2,
        },
        {
          label: "worst " + fmt(hi),
          data: values.map(() => hi),
          borderColor: "#cf222e",
          borderDash: [6, 4],
          borderWidth: 1.5,
          pointRadius: 0,
          fill: false,
          order: 3,
        },
      ],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: { duration: totalDuration },
      animations: drawAnimation,
      interaction: { mode: "index", intersect: false },
      scales: {
        y: {
          title: { display: true, text: "time (" + unit + ")" },
          grace: "8%",
        },
        x: { ticks: { maxRotation: 0, autoSkip: true, maxTicksLimit: 8 } },
      },
      plugins: {
        legend: { display: true, position: "top", labels: { boxWidth: 14, usePointStyle: true } },
        tooltip: {
          filter: (item) => item.datasetIndex === 0,
          callbacks: {
            title: (items) => {
              const p = points[items[0].dataIndex];
              return p.commit + (p.date ? "  ·  " + p.date.slice(0, 10) : "");
            },
            label: (item) => {
              const v = item.parsed.y;
              const d = lo > 0 ? ((v - lo) / lo) * 100 : 0;
              const tag = v === lo ? "  (best)" : v === hi ? "  (worst)" : "";
              return `${fmt(v)} ${unit}` + (d > 0.05 ? `  ·  +${d.toFixed(1)}% vs best` : "  ·  best") + tag;
            },
          },
        },
      },
    },
  });

  if (!dialog.open) dialog.showModal();
}

load();
