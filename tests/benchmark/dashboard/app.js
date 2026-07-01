"use strict";

// Shape-pair benchmark dashboard.
//
// The data is a 6-dimensional cube — shape1, size1, shape2, size2, method,
// number-type — recorded per commit and per machine. Each dimension has a
// multi-select chip filter. The two highest-priority dimensions that still have
// more than one value selected become the table's columns and rows; any further
// multi-valued dimensions facet into a grid of small tables. Dimensions narrowed
// to a single value are shown in the caption. A second, separate section renders
// the whole-algorithm ("extra") benchmarks as classic per-suite tables.

let DB = null;
let chart = null;
let pop = null; // hover preview bubble

const DIMS = ["shape1", "size1", "shape2", "size2", "method", "type"];
const DIM_LABEL = {
  shape1: "Shape A", size1: "Size A", shape2: "Shape B", size2: "Size B",
  method: "Method", type: "Number",
};
const DIM_SHORT = {
  shape1: "A", size1: "A size", shape2: "B", size2: "B size",
  method: "method", type: "number",
};
// Axis priority: among multi-valued dimensions, the first becomes columns and
// the second becomes rows (number types first — the comparison that matters most).
const AXIS_PRIORITY = ["type", "method", "shape1", "shape2", "size1", "size2"];

// A bare Point has no extent, so it carries this sentinel size in the data. It
// is never offered as a size filter chip and never labelled.
const NO_SIZE = "n/a";
const SIZE_DIMS = new Set(["size1", "size2"]);
const SHAPE_OF = { size1: "shape1", size2: "shape2" };

const selected = {}; // dim -> Set of selected values
let swapAxes = false;

// Selectable values of a dimension — the "n/a" point-size is never offered.
const dimValues = (d) =>
  (DB.dimensions[d] || []).filter((v) => !(SIZE_DIMS.has(d) && v === NO_SIZE));

// "large Segment", "small Triangle", or just "Point" (points have no size).
function variantName(shape, size) {
  if (!shape) return "";
  if (shape === "Point" || !size || size === NO_SIZE) return shape;
  return `${size} ${shape}`;
}

// Number of points defining a random shape, mirroring randomshapes.hpp.
const _BISHAPES = new Set(["Segment", "OrientedSegment", "Line", "OrientedLine", "Rectangle"]);
const _TRISHAPES = new Set(["Triangle", "Disk"]);
function nDefiningPoints(shape) {
  if (_BISHAPES.has(shape)) return 2;
  if (_TRISHAPES.has(shape)) return 3;
  return 1000; // Convex hull sample size (m in run_shapepairs.py)
}

// Plain-language description of how a random shape of this kind/size is drawn,
// used as a tooltip. Mirrors the generators in tests/benchmark/randomshapes.hpp
// (small offset radius 500, full radius 5000).
function distributionTip(shape, size) {
  if (shape === "Point")
    return "Random integer points in a disk of radius 5000";
  const n = nDefiningPoints(shape);
  if (size === "large")
    return `Random ${shape} (large): ${n} random integer defining points in a disk of radius 5000`;
  return `Random ${shape} (small): ${n} random integer defining points in a disk of radius 500 translated by a random integer vector of length ≤ 5000`;
}

async function load() {
  const res = await fetch("data.json", { cache: "no-store" });
  DB = await res.json();
  pop = document.getElementById("spark-pop");

  const sel = document.getElementById("machine");
  if (!DB.machines || !DB.machines.length) {
    document.getElementById("suites").innerHTML =
      '<p class="empty-state">No benchmark data recorded yet. Run ' +
      "<code>bash tests/benchmark/record.sh</code> and commit " +
      "<code>tests/benchmark/history/</code>.</p>";
    document.getElementById("extra-section").style.display = "none";
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

  for (const d of DIMS) selected[d] = new Set(dimValues(d));

  buildFilterBar();
  render();
}

// ── shared formatting / colour helpers (kept from the original dashboard) ─────

function fmt(t) {
  if (t === null || t === undefined) return "—";
  if (t >= 100) return t.toFixed(0);
  if (t >= 10) return t.toFixed(1);
  if (t >= 1) return t.toFixed(2);
  return t.toFixed(3);
}

const latest = (pts) => (pts && pts.length ? pts[pts.length - 1] : null);
const bestOf = (pts) => Math.min(...pts.map((p) => p.time));
const worstOf = (pts) => Math.max(...pts.map((p) => p.time));

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
  const lo = Math.min(...times);
  const hi = Math.max(...times, lo * 2);
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

// ── filter bar ────────────────────────────────────────────────────────────────

function buildFilterBar() {
  const root = document.getElementById("filters");
  root.innerHTML = "";

  for (const d of DIMS) {
    const values = dimValues(d);
    if (values.length === 0) continue;

    const group = document.createElement("div");
    group.className = "filter-group";

    const label = document.createElement("button");
    label.type = "button";
    label.className = "filter-label";
    label.textContent = DIM_LABEL[d];
    label.title = "Toggle all";
    label.addEventListener("click", () => {
      const all = selected[d].size === values.length;
      selected[d] = new Set(all ? [] : values);
      buildFilterBar();
      render();
    });
    group.appendChild(label);

    const chips = document.createElement("div");
    chips.className = "chips";
    for (const v of values) {
      const chip = document.createElement("button");
      chip.type = "button";
      chip.className = "chip" + (selected[d].has(v) ? " on" : "");
      chip.textContent = v;
      chip.addEventListener("click", () => {
        if (selected[d].has(v)) selected[d].delete(v);
        else selected[d].add(v);
        buildFilterBar();
        render();
      });
      chips.appendChild(chip);
    }
    group.appendChild(chips);
    root.appendChild(group);
  }

  const swap = document.createElement("button");
  swap.type = "button";
  swap.className = "swap-btn";
  swap.textContent = "⇄ swap rows / columns";
  swap.addEventListener("click", () => { swapAxes = !swapAxes; render(); });
  root.appendChild(swap);
}

// Selected values of a dimension, in canonical display order.
const sel = (d) => dimValues(d).filter((v) => selected[d].has(v));

// ── main render (shape-pair cube) ─────────────────────────────────────────────

function render() {
  const machine = document.getElementById("machine").value;
  const data = (DB.pairs && DB.pairs[machine]) || {};
  const root = document.getElementById("suites");
  root.innerHTML = "";

  // Dimensions with >1 selected value drive the pivot; those with exactly 1 are
  // fixed; those with 0 mean "nothing selected" -> no data.
  const empty = DIMS.filter((d) => sel(d).length === 0);
  if (empty.length) {
    root.innerHTML =
      `<p class="empty-state">No values selected for: ${empty.map((d) => DIM_LABEL[d]).join(", ")}.</p>`;
    updateSummary([], [], []);
    renderExtra();
    return;
  }

  let active = AXIS_PRIORITY.filter((d) => sel(d).length > 1);
  let colDim = active[0] || null;
  let rowDim = active[1] || null;
  if (swapAxes) { const t = colDim; colDim = rowDim; rowDim = t; }
  const facetDims = active.filter((d) => d !== colDim && d !== rowDim);
  const fixedDims = DIMS.filter((d) => sel(d).length === 1);

  updateSummary(fixedDims, [colDim, rowDim].filter(Boolean), facetDims);

  // One facet table per combination of the facet dimensions' selected values.
  let facetCombos = product(facetDims.map((d) => sel(d).map((v) => [d, v])));
  // A Point has no size, so collapse the size2 facet when the facet's shape2 is
  // Point — otherwise "Point + small" and "Point + large" would be duplicates.
  if (facetDims.includes("size2")) {
    const shape2Fixed = fixedDims.includes("shape2") ? sel("shape2")[0] : null;
    const shape2Facet = facetDims.includes("shape2");
    const seen = new Set();
    facetCombos = facetCombos.filter((combo) => {
      const s2 = shape2Facet ? (combo.find(([d]) => d === "shape2") || [])[1] : shape2Fixed;
      const k = combo
        .map(([d, v]) => (s2 === "Point" && d === "size2") ? `${d}=*` : `${d}=${v}`)
        .join(";");
      if (seen.has(k)) return false;
      seen.add(k);
      return true;
    });
  }

  let anyData = false;
  for (const combo of facetCombos) {
    const base = {};
    for (const d of fixedDims) base[d] = sel(d)[0];
    for (const [d, v] of combo) base[d] = v;

    // Displayable points for a cell: present and matching the baseline.
    const dispPts = (r, c) => {
      const coord = { ...base };
      if (rowDim) coord[rowDim] = r;
      if (colDim) coord[colDim] = c;
      const pts = data[keyOf(coord)] || null;
      const lp = latest(pts);
      return (lp && lp.match !== false) ? pts : null;
    };
    const allCols = colDim ? sel(colDim) : [null];
    const allRows = rowDim ? sel(rowDim) : [null];
    // Drop empty columns and rows, and skip the whole table if nothing remains.
    const cols = allCols.filter((c) => allRows.some((r) => dispPts(r, c)));
    const rows = allRows.filter((r) => cols.some((c) => dispPts(r, c)));
    if (!rows.length || !cols.length) continue;
    anyData = true;

    const section = document.createElement("section");
    section.className = "suite";

    // Facet title: merge each operand's shape and size ("large Segment"), with a
    // tooltip describing how those random shapes are drawn. A Point shows no size.
    const titleParts = [];
    const addOperand = (prefix, shapeDim, sizeDim) => {
      if (!facetDims.includes(shapeDim) && !facetDims.includes(sizeDim)) return;
      const shape = base[shapeDim], size = base[sizeDim];
      if (shape)
        titleParts.push({ text: `${prefix}: ${variantName(shape, size)}`, tip: distributionTip(shape, size) });
      else if (size !== undefined)
        titleParts.push({ text: `${prefix} size: ${size}`, tip: "" });
    };
    addOperand("A", "shape1", "size1");
    addOperand("B", "shape2", "size2");
    for (const d of facetDims)
      if (!SIZE_DIMS.has(d) && d !== "shape1" && d !== "shape2")
        titleParts.push({ text: `${DIM_SHORT[d]}: ${base[d]}`, tip: "" });

    if (titleParts.length) {
      const h = document.createElement("h2");
      h.className = "facet-title";
      titleParts.forEach((p, i) => {
        if (i) h.appendChild(document.createTextNode("  ·  "));
        const span = document.createElement("span");
        span.textContent = p.text;
        if (p.tip) { span.title = p.tip; span.className = "facet-part"; }
        h.appendChild(span);
      });
      section.appendChild(h);
    }

    const table = document.createElement("table");
    const thead = document.createElement("thead");
    const hr = document.createElement("tr");
    hr.appendChild(th(rowDim ? DIM_SHORT[rowDim] : ""));
    for (const c of cols) hr.appendChild(th(c === null ? colDim || "value" : c, "num"));
    thead.appendChild(hr);
    table.appendChild(thead);

    const tbody = document.createElement("tbody");
    for (const r of rows) {
      const tr = document.createElement("tr");
      const fn = document.createElement("td");
      fn.className = "fn";
      fn.textContent = r === null ? "" : r;
      tr.appendChild(fn);

      const rowPts = cols.map((c) => {
        const coord = { ...base };
        if (rowDim) coord[rowDim] = r;
        if (colDim) coord[colDim] = c;
        return data[keyOf(coord)] || null;
      });

      cols.forEach((c, ci) => {
        const td = document.createElement("td");
        td.className = "val";
        const pts = rowPts[ci];
        const lp = latest(pts);
        // A measurement whose result disagrees with the ERational baseline is
        // treated as missing — its timing is meaningless, so show a dash.
        if (lp && lp.match !== false) {
          anyData = true;
          const coord = { ...base };
          if (rowDim) coord[rowDim] = r;
          if (colDim) coord[colDim] = c;
          // Colour relative to this cell's own history (best..worst over time).
          const lo = bestOf(pts), hi = worstOf(pts);
          const spark = pts.length > 1 ? cellSpark(pts, 52, 16) : "";
          td.innerHTML =
            `<span class="cell">${spark}` +
            `<span class="num" style="color:${statusColor(lp.time, lo, hi)}">${fmt(lp.time)}</span>` +
            `</span>`;
          td.classList.add("clickable");
          td.addEventListener("click", () => showChart(describe(coord) + " — " + machine, pts, "ns"));
          td.addEventListener("mouseenter", (e) => showPop(e, describe(coord), pts, "ns"));
          td.addEventListener("mouseleave", hidePop);
        } else {
          td.textContent = "—";
          td.classList.add("empty");
        }
        tr.appendChild(td);
      });
      tbody.appendChild(tr);
    }
    table.appendChild(tbody);

    const wrap = document.createElement("div");
    wrap.className = "table-wrap";
    wrap.appendChild(table);
    section.appendChild(wrap);

    const note = document.createElement("div");
    note.className = "unit-note";
    note.textContent = colDim
      ? `columns: ${DIM_LABEL[colDim]} · time in ns · colour vs. own history · hover for trend, click for chart`
      : "time in ns · colour vs. own history · hover for trend, click for chart";
    section.appendChild(note);

    root.appendChild(section);
  }

  if (!anyData) {
    root.innerHTML = '<p class="empty-state">No data for this machine and filter.</p>';
  }

  renderExtra();
}

// Build the cube key in the canonical "s1|sz1|s2|sz2|method|type" order. A Point
// carries the size-agnostic sentinel regardless of any size axis/facet value.
function keyOf(c) {
  const size1 = c.shape1 === "Point" ? NO_SIZE : c.size1;
  const size2 = c.shape2 === "Point" ? NO_SIZE : c.size2;
  return [c.shape1, size1, c.shape2, size2, c.method, c.type].join("|");
}

// Human-readable label for a full coordinate ("large Segment × Point · …").
function describe(c) {
  return `${variantName(c.shape1, c.size1)} × ${variantName(c.shape2, c.size2)}`
    + ` · ${c.method} · ${c.type}`;
}

// Cartesian product of a list of [dim, value] option-lists. Empty -> [[]].
function product(lists) {
  return lists.reduce(
    (acc, list) => acc.flatMap((pre) => list.map((item) => [...pre, item])),
    [[]],
  );
}

function updateSummary(fixedDims, axisDims, facetDims) {
  const parts = [];
  if (axisDims.length) {
    parts.push(`<span><b>${axisDims.length}</b> axis dim${axisDims.length > 1 ? "s" : ""}: ${axisDims.map((d) => DIM_LABEL[d]).join(", ")}</span>`);
  }

  const fixedSet = new Set(fixedDims);
  const facetSet = new Set(facetDims || []);
  const shown = new Set();
  // Fixed operands: merge shape and size into "large Segment" (or just "Point").
  for (const [label, shapeDim, sizeDim] of [["Shape A", "shape1", "size1"],
                                            ["Shape B", "shape2", "size2"]]) {
    if (!fixedSet.has(shapeDim)) continue;
    const shape = sel(shapeDim)[0];
    const size = fixedSet.has(sizeDim) ? sel(sizeDim)[0] : null;
    const tip = (size || shape === "Point") ? ` title="${distributionTip(shape, size)}"` : "";
    const cls = tip ? ' class="facet-part"' : "";
    parts.push(`<span${cls}${tip}>${label}: <b>${variantName(shape, size)}</b></span>`);
    shown.add(shapeDim);
    if (size) shown.add(sizeDim);
  }
  for (const d of fixedDims) {
    if (shown.has(d)) continue;
    // A fixed size whose shape varies is already shown, merged, in the facet titles.
    if (SIZE_DIMS.has(d) && (facetSet.has(SHAPE_OF[d]) || !fixedSet.has(SHAPE_OF[d]))) continue;
    parts.push(`<span>${DIM_LABEL[d]} <b>${sel(d)[0]}</b></span>`);
  }

  const g = DB.generated ? new Date(DB.generated) : null;
  if (g) parts.push(`<span>updated <b>${g.toLocaleDateString()}</b></span>`);
  document.getElementById("summary").innerHTML = parts.join("");
}

// ── extra (whole-algorithm) benchmarks: classic per-suite tables ──────────────

function renderExtra() {
  const machine = document.getElementById("machine").value;
  const root = document.getElementById("extra");
  const section = document.getElementById("extra-section");
  root.innerHTML = "";

  const names = Object.keys(DB.extra || {}).sort();
  if (!names.length) { section.style.display = "none"; return; }
  let rendered = 0;
  for (const name of names) {
    const s = DB.extra[name];
    const machineData = (s.data && s.data[machine]) || {};

    // Drop empty type columns / operation rows, and skip empty suites.
    const types = s.types.filter((t) => s.functions.some((op) => machineData[op + "|" + t]));
    const ops = s.functions.filter((op) => types.some((t) => machineData[op + "|" + t]));
    if (!types.length || !ops.length) continue;
    rendered++;

    const card = document.createElement("section");
    card.className = "suite";

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
    card.appendChild(h);

    const table = document.createElement("table");
    const thead = document.createElement("thead");
    const hr = document.createElement("tr");
    hr.appendChild(th("Operation"));
    for (const t of types) hr.appendChild(th(t, "num"));
    thead.appendChild(hr);
    table.appendChild(thead);

    const tbody = document.createElement("tbody");
    for (const op of ops) {
      const tr = document.createElement("tr");
      const fn = document.createElement("td");
      fn.className = "fn";
      fn.textContent = op;
      tr.appendChild(fn);

      const rowPts = types.map((t) => machineData[op + "|" + t] || null);

      types.forEach((t, ti) => {
        const td = document.createElement("td");
        td.className = "val";
        const pts = rowPts[ti];
        const lp = latest(pts);
        if (lp) {
          // Colour relative to this cell's own history (best..worst over time).
          const lo = bestOf(pts), hi = worstOf(pts);
          const spark = pts.length > 1 ? cellSpark(pts, 52, 16) : "";
          td.innerHTML =
            `<span class="cell">${spark}` +
            `<span class="num" style="color:${statusColor(lp.time, lo, hi)}">${fmt(lp.time)}</span>` +
            `</span>`;
          td.classList.add("clickable");
          const title = `${name} · ${op} · ${t} — ${machine}`;
          td.addEventListener("click", () => showChart(title, pts, s.unit));
          td.addEventListener("mouseenter", (e) => showPop(e, `${name} · ${op} · ${t}`, pts, s.unit));
          td.addEventListener("mouseleave", hidePop);
        } else {
          td.textContent = "—";
          td.classList.add("empty");
        }
        tr.appendChild(td);
      });
      tbody.appendChild(tr);
    }
    table.appendChild(tbody);
    const wrap = document.createElement("div");
    wrap.className = "table-wrap";
    wrap.appendChild(table);
    card.appendChild(wrap);

    const note = document.createElement("div");
    note.className = "unit-note";
    note.textContent =
      "time in " + s.unit + " · colour vs. own history · hover for trend, click for chart";
    card.appendChild(note);

    root.appendChild(card);
  }
  section.style.display = rendered ? "" : "none";
}

// ── hover bubble + full chart (kept from the original dashboard) ──────────────

function showPop(event, title, points, unit) {
  const lp = latest(points);
  const lo = bestOf(points), hi = worstOf(points), cur = lp.time;
  const delta = lo > 0 ? ((cur - lo) / lo) * 100 : 0;
  // Per-run spread of the latest measurement (min..max across repetitions).
  const spread = (lp.min !== undefined && lp.max !== undefined && lp.max > lp.min)
    ? `<span class="muted">run ${fmt(lp.min)}–${fmt(lp.max)}</span>` : "";
  pop.innerHTML =
    `<div class="pop-title">${title}</div>` +
    sparkline(points, 168, 46) +
    `<div class="pop-stats">` +
    `<span style="color:${statusColor(cur, lo, hi)}">now ${fmt(cur)}` +
    (delta > 0.05 ? ` (+${delta.toFixed(1)}%)` : "") + `</span>` +
    `<span class="muted">best ${fmt(lo)} · ${points.length} pts</span>` +
    spread +
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

function showChart(title, points, unit) {
  hidePop();
  const dialog = document.getElementById("chart-dialog");
  document.getElementById("chart-title").textContent = title;

  const n = points.length;
  const lo = bestOf(points), hi = worstOf(points);
  const labels = points.map((p) => (p.date ? p.date.slice(0, 10) : "") + "\n" + p.commit);
  const values = points.map((p) => p.time);

  const pointColors = values.map((v, i) =>
    i === n - 1 ? "#0a429e" : statusColor(v, lo, hi));
  const pointRadii = values.map((v, i) =>
    i === n - 1 ? 6 : v === lo || v === hi ? 5 : 3);

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
      type: "number", easing: "linear", duration: stepDuration, from: NaN,
      delay(ctx) {
        if (ctx.type !== "data" || ctx.xStarted) return 0;
        ctx.xStarted = true;
        return ctx.index * stepDuration;
      },
    },
    y: {
      type: "number", easing: "linear", duration: stepDuration, from: previousY,
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
          tension: 0,
          fill: true,
          order: 1,
        },
        {
          label: "best " + fmt(lo),
          data: values.map(() => lo),
          borderColor: "#1a7f37", borderDash: [6, 4], borderWidth: 1.5,
          pointRadius: 0, fill: false, order: 2,
        },
        {
          label: "worst " + fmt(hi),
          data: values.map(() => hi),
          borderColor: "#cf222e", borderDash: [6, 4], borderWidth: 1.5,
          pointRadius: 0, fill: false, order: 3,
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
        y: { title: { display: true, text: "time (" + unit + ")" }, grace: "8%" },
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
