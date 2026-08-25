"use strict";

// Shape-pair benchmark dashboard.
//
// The data is a 6-dimensional cube — shape1, size1, shape2, size2, method,
// number-type — recorded per commit and per machine. Each dimension has a
// multi-select chip filter. The two highest-priority dimensions that still have
// more than one value selected become the table's columns and rows; any further
// multi-valued dimensions facet into a grid of small tables. Dimensions narrowed
// to a single value are shown in the caption.
//
// The asymptotic benchmarks render on their own page, as one chart of time
// against input size per category. Their data is a smaller cube — dataset,
// problem, algorithm, number type — and the reader picks which single dimension
// is the multi-select "compare" axis; the other three act as radio groups. See
// renderAsymptotic below.

let DB = null;
let chart = null;
let pop = null; // hover preview bubble
const PAGE = document.body.dataset.page || "pairs";
let deferredPairsPromise = null;
let deferredPairsLoaded = false;

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
// the second becomes rows; any further ones split into separate tables.
const AXIS_PRIORITY = ["shape1", "shape2", "method", "type", "size1", "size2"];

// A bare Point has no extent, so it carries this sentinel size in the data. It
// is never offered as a size filter chip and never labelled.
const NO_SIZE = "n/a";
const SIZE_DIMS = new Set(["size1", "size2"]);
const SHAPE_OF = { size1: "shape1", size2: "shape2" };

const selected = {}; // dim -> Set of selected values
let swapAxes = false;

// Initial selection at page load: every shape and method, but a single size and
// number type so the first view is small. Dimensions not listed start fully
// selected; a listed value that isn't in the data falls back to "all".
const INITIAL_SELECTION = { size1: ["large"], size2: ["small"], type: ["int"] };

// Selectable values of a dimension — the "n/a" point-size is never offered.
const dimValues = (d) =>
  (DB.dimensions[d] || []).filter((v) => !(SIZE_DIMS.has(d) && v === NO_SIZE));

// "large Segment", "small Triangle", or just "Point" (points have no size).
function variantName(shape, size) {
  if (!shape) return "";
  if (shape === "Point" || !size || size === NO_SIZE) return shape;
  return `${size} ${shape}`;
}

// Shapes drawn with one shape's generator but stored as a more general type, so
// the cube exercises the storage type's code paths on that geometry.
const _AS_TYPE = {
  TriangleAsPolygon: { source: "Triangle", stored: "Polygon" },
  TriangleAsConvex:  { source: "Triangle", stored: "Convex" },
  ConvexAsPolygon:   { source: "Convex",   stored: "Polygon" },
  PolygonAsPWH:      { source: "Polygon",  stored: "hole-free PolygonWithHoles" },
  // Not a re-storage of the same vertices but a different structure over the same
  // region: the polygon's constrained Delaunay triangulation, built as setup so
  // only the queries against the mesh are timed.
  PolygonAsTriangulation: { source: "Polygon", stored: "Triangulation" },
  // Also a re-storage: the convex hull, adopted as the intersection of its own
  // edge half-planes, so the shape's rational-vertex paths run on that region.
  HalfplaneIntersection: { source: "Convex", stored: "HalfplaneIntersection of the hull's edge half-planes" },
};

// Shapes built from a sample of m points rather than a fixed number of defining
// points: how many are drawn, and what the constructor makes of them. Mirrors
// the randomSmall/LargeXxx generators in randomshapes.hpp.
const _SAMPLED = {
  Polygon:       { m: 32, build: "untangled into a simple polygon (at most 32 vertices)" },
  PolygonWithHoles: { m: 32, build: "untangled into a simple polygon (at most 32 vertices), " +
                                    "then punched with 6 holes of 6 points each, themselves " +
                                    "untangled (usually non-convex) and drawn from a fifth of " +
                                    "the polygon's span, taking about a tenth of its area away" },
  Polyline:      { m: 32, build: "linked in the order drawn and never untangled, so the chain may " +
                                 "cross itself (32 vertices)" },
  MonotoneChain: { m: 32, build: "sorted lexicographically with duplicates dropped (at most 32 vertices)" },
  Convex:        { m: 1000, build: "reduced to their convex hull (~34 vertices on average)" },
};

// Number of points defining a random shape, mirroring randomshapes.hpp.
const _BISHAPES = new Set(["Segment", "OrientedSegment", "Line", "OrientedLine", "Rectangle"]);
const _TRISHAPES = new Set(["Triangle", "Disk"]);
function nDefiningPoints(shape) {
  if (_AS_TYPE[shape]) return nDefiningPoints(_AS_TYPE[shape].source);
  if (_SAMPLED[shape]) return _SAMPLED[shape].m;
  if (_BISHAPES.has(shape)) return 2;
  if (_TRISHAPES.has(shape)) return 3;
  return 1000; // Unknown shape: fall back to the point-sample size in run_shapepairs.py
}

// A shape's own extent and the field it is scattered over are set independently:
// every generator draws an anchor point in the field and the rest of the shape's
// points relative to it. A small shape spans 1000 in a field of 10000, so a random
// pair usually misses; a large one spans 5000 in a field of 5000, so a random pair
// usually meets. Points are drawn from the disks inscribed in those squares, hence
// the radii below — smallRange / mediumRange / largeRange halved, as in
// tests/benchmark/randomshapes.hpp.
const _SIZE_SCALE = {
  small: { extent: 500,  field: 5000 },
  large: { extent: 2500, field: 2500 },
};
const sizeScale = (size) => _SIZE_SCALE[size] || _SIZE_SCALE.small;

// Where a random point sample of size n is drawn from, per size class.
const pointCloud = (n, size) => {
  const s = sizeScale(size);
  return `${n} random integer points in a disk of radius ${s.extent} translated by ` +
         `a random integer vector of length ≤ ${s.field}`;
};

// Plain-language description of how a random shape of this kind/size is drawn,
// used as a tooltip. Mirrors the generators in tests/benchmark/randomshapes.hpp.
function distributionTip(shape, size) {
  if (shape === "Point")
    return "Random integer points in a disk of radius 5000";
  const as = _AS_TYPE[shape];
  if (as)
    return `${distributionTip(as.source, size)}, then stored as a ${as.stored}`;
  // A set is drawn on a grid rather than from a point cloud: every 4-connected
  // group of filled cells is one component, so two components meet at corners at
  // most, and a group may close around a hole or nest another.
  if (shape === "PolygonSet") {
    const s = sizeScale(size);
    return `Random PolygonSet (${size}): a 6×6 grid of squares spanning ${2 * s.extent}, each cell ` +
           `filled with probability 45%, every 4-connected group of filled cells becoming one ` +
           `component (usually pinched, sometimes holed or nesting another), translated by ` +
           `a random integer vector of length ≤ ${s.field}`;
  }
  const sampled = _SAMPLED[shape];
  if (sampled)
    return `Random ${shape} (${size}): ${pointCloud(sampled.m, size)}, ${sampled.build}`;
  const n = nDefiningPoints(shape);
  return `Random ${shape} (${size}): ${pointCloud(n, size)} as defining points`;
}

async function load() {
  const dataFile = PAGE === "asymptotic" ? "asymptotic.json" : "pairs.json";
  const res = await fetch(dataFile, { cache: "no-cache" });
  DB = await res.json();
  pop = document.getElementById("spark-pop");

  const sel = document.getElementById("machine");
  if (!DB.machines || !DB.machines.length) {
    const root = document.getElementById(PAGE === "asymptotic" ? "asymptotic" : "suites");
    root.innerHTML =
      '<p class="empty-state">No benchmark data recorded yet. Run ' +
      "<code>bash tests/benchmark/record.sh</code>, which records into the " +
      "benchmark data repository.</p>";
    return;
  }
  for (const m of DB.machines) {
    const o = document.createElement("option");
    o.value = o.textContent = m;
    sel.appendChild(o);
  }
  sel.addEventListener("change", () => {
    render();
    requestDeferredPairs(render);
  });
  // The shared history-depth input limits both pages' charts. On pairs it also
  // limits the recent history used to colour the current measurement.
  const depth = document.getElementById("depth");
  if (depth) depth.addEventListener("change", () => { historyDepth(); render(); });
  const chartClose = document.getElementById("chart-close");
  if (chartClose) {
    chartClose.addEventListener(
      "click", () => document.getElementById("chart-dialog").close());
  }

  if (PAGE === "pairs") {
    for (const d of DIMS) {
      const vals = dimValues(d);
      const pref = (INITIAL_SELECTION[d] || []).filter((v) => vals.includes(v));
      selected[d] = new Set(pref.length ? pref : vals);
    }
    buildFilterBar();
  }

  render();
  if (PAGE === "pairs") scheduleDeferredPairs();
}

function loadDeferredPairs() {
  if (PAGE !== "pairs" || deferredPairsLoaded) return Promise.resolve();
  if (deferredPairsPromise) return deferredPairsPromise;

  deferredPairsPromise = fetch("pairs-deferred.json", { cache: "no-cache" })
    .then((res) => {
      if (!res.ok) throw new Error(`could not load deferred pair data (${res.status})`);
      return res.json();
    })
    .then((payload) => {
      for (const [machine, cells] of Object.entries(payload.pairs || {}))
        Object.assign(DB.pairs[machine] ||= {}, cells);
      deferredPairsLoaded = true;
    })
    .catch((error) => {
      deferredPairsPromise = null;
      console.warn("Deferred pair benchmark data was not loaded.", error);
      throw error;
    });
  return deferredPairsPromise;
}

// The opening view is complete in pairs.json. Fetch the rest only after it has
// had a chance to paint, but immediately finish the load if a reader changes a
// filter before then.
function scheduleDeferredPairs() {
  const prefetch = () => { loadDeferredPairs().catch(() => {}); };
  if ("requestIdleCallback" in window)
    window.requestIdleCallback(prefetch, { timeout: 3000 });
  else
    window.setTimeout(prefetch, 1500);
}

function requestDeferredPairs(onLoaded) {
  if (PAGE === "pairs" && !deferredPairsLoaded)
    loadDeferredPairs().then(onLoaded).catch(() => {});
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
const MIN_HISTORY_DEPTH = 2;
const MAX_HISTORY_DEPTH = 20;

function historyDepth() {
  const input = document.getElementById("depth");
  const requested = Number(input && input.value);
  const depth = Math.max(
    MIN_HISTORY_DEPTH,
    Math.min(MAX_HISTORY_DEPTH, Number.isFinite(requested) ? Math.trunc(requested) : 5));
  if (input) input.value = depth;
  return depth;
}

function recentHistory(points, depth = historyDepth()) {
  return points.slice(Math.max(0, points.length - depth));
}

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

function th(text, cls, tip) {
  const e = document.createElement("th");
  e.textContent = text;
  if (cls) e.className = cls;
  if (tip) { e.title = tip; e.classList.add("facet-part"); }
  return e;
}

// Distribution tooltip for a row/column header that names a shape. Returns "" —
// no tooltip — when the shape's size varies along the *other* axis, because then
// the header spans both small and large (two distributions, so it's ambiguous).
function axisTip(dim, value, base, otherDim) {
  if (value === null || (dim !== "shape1" && dim !== "shape2")) return "";
  if (value === "Point") return distributionTip("Point", null);
  const sizeDim = dim === "shape1" ? "size1" : "size2";
  if (sizeDim === otherDim) return "";        // size spans the other axis → ambiguous
  const size = base[sizeDim];                 // fixed or facet-fixed within this table
  return (size && size !== NO_SIZE) ? distributionTip(value, size) : "";
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
        requestDeferredPairs(render);
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
  if (PAGE === "asymptotic") {
    renderAsymptotic();
    return;
  }
  renderPairs();
}

function renderPairs() {
  const machine = document.getElementById("machine").value;
  const depth = historyDepth();
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
    for (const c of cols)
      hr.appendChild(th(c === null ? colDim || "value" : c, "num", axisTip(colDim, c, base, rowDim)));
    thead.appendChild(hr);
    table.appendChild(thead);

    const tbody = document.createElement("tbody");
    for (const r of rows) {
      const tr = document.createElement("tr");
      const fn = document.createElement("td");
      fn.className = "fn";
      fn.textContent = r === null ? "" : r;
      const rtip = axisTip(rowDim, r, base, colDim);
      if (rtip) { fn.title = rtip; fn.classList.add("facet-part"); }
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
          // Compare only the requested recent history, rather than letting an
          // old run permanently set this cell's green-to-red scale.
          const history = recentHistory(pts, depth);
          const lo = bestOf(history), hi = worstOf(history);
          const spark = history.length > 1 ? cellSpark(history, 52, 16) : "";
          td.innerHTML =
            `<span class="cell">${spark}` +
            `<span class="num" style="color:${statusColor(lp.time, lo, hi)}">${fmt(lp.time)}</span>` +
            `</span>`;
          td.classList.add("clickable");
          td.addEventListener("click", () => showChart(describe(coord) + " — " + machine, history, "ns"));
          td.addEventListener("mouseenter", (e) => showPop(e, describe(coord), history, "ns"));
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
      ? `columns: ${DIM_LABEL[colDim]} · time in ns · colour vs. last ${depth} runs · hover for trend, click for chart`
      : `time in ns · colour vs. last ${depth} runs · hover for trend, click for chart`;
    section.appendChild(note);

    root.appendChild(section);
  }

  if (!anyData) {
    root.innerHTML = '<p class="empty-state">No data for this machine and filter.</p>';
  }

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

// ── asymptotic benchmarks: one time-against-size chart per category ───────────
//
// Each category is a small cube — dataset, problem, algorithm, number type —
// measured over a fixed list of input sizes. Exactly one of those four
// dimensions is the multi-select "compare" axis at any moment: its selected
// values are the curves on the chart, and each one's chip wears its curve's
// colour. The other three behave as radio groups, naming the single slice of
// the cube being compared. Clicking a dimension's label makes that dimension
// the compare axis (shift- or ctrl-clicking one of its chips does the same and
// selects that value in one go), which demotes the previous compare axis back
// to a single value.
//
// The shared History control draws the last N recorded runs of each curve:
// recency modulates shade, the cube
// dimension controls hue, so an older run of a curve is a paler version of the
// same colour rather than a different-coloured line.

const ASYM_DIMS = ["dataset", "problem", "algorithm", "type"];
const ASYM_DIM_LABEL = {
  dataset: "Dataset", problem: "Problem", algorithm: "Algorithm", type: "Number",
};
// The opening view of a category is read straight off the bar, left to right:
// the first field that offers a choice becomes the compare axis with all of its
// values drawn, and every field after it stands on its first value. No ranking
// of which dimension is the interesting one — the bar's own order is the
// answer, and what the reader sees first is the widest comparison the leftmost
// choice can make.

// Categorical palette for the compare axis. Deliberately unlike the pairs
// page's green-to-red status heat: nothing here is better or worse for being a
// particular hue, it is just a different value of one dimension.
const CURVE_COLORS = [
  "#0a429e", "#cf222e", "#1a7f37", "#bf5b04",
  "#8250df", "#0f7d8c", "#a3325f", "#57606a",
];
// The CGAL reference, which is not one of the cube's values and should not look
// like one: neutral and dashed.
const BASELINE_COLOR = "#6e7781";
// One dash pattern per reference of the same curve, in the order the baseline
// snapshot recorded them.
const BASELINE_DASHES = [[6, 4], [2, 3], [10, 3, 2, 3]];

// Per-category UI state, built on first render.
const asymState = {};
const asymCharts = {};

const hexToRgba = (hex, alpha) => {
  const n = parseInt(hex.slice(1), 16);
  return `rgba(${(n >> 16) & 255},${(n >> 8) & 255},${n & 255},${alpha})`;
};

const asymKey = (coord) => ASYM_DIMS.map((d) => coord[d]).join("|");

// The recorded series for one value of the compare axis, or null.
//
// A category's cube is not always rectangular. Most are: Segment search
// measures every (dataset, problem, algorithm, type) it names. But several
// categories pair each value of one dimension with its own value of another —
// each problem in Point constructions has a single algorithm of its own, each
// dataset in Regularized union its own problem — and there the radio buttons
// cannot name one slice that all the curves live in, because no such slice
// exists.
//
// So the radios are honoured where the cube has the combination and given way
// where it does not: a lookup that finds nothing exact drops one radio
// dimension at a time, in the order below, until it finds a series. Whatever it
// had to drop comes back with the result so the curve's legend entry can say
// which values it actually stands for — a curve is never quietly something
// other than what the bar says. The number type is never dropped: it is the one
// dimension that is independent in every category, so a miss there is a real
// gap in the data rather than a corner of the cube that was never square.
const ASYM_RELAX_ORDER = ["algorithm", "problem", "dataset"];

function asymSeries(category, state, machineData, value) {
  const coord = {};
  for (const dim of ASYM_DIMS) {
    coord[dim] = dim === state.compare
      ? value
      : asymSelected(category, state, dim)[0];
  }
  const exact = machineData[asymKey(coord)];
  if (exact) return { runs: exact, relaxed: [] };

  // One dimension at a time, then pairs — enough for the couplings these
  // categories actually have, and it prefers the answer that overrides least.
  for (const first of ASYM_RELAX_ORDER) {
    if (first === state.compare) continue;
    for (const a of category.dimensions[first] || []) {
      if (a === coord[first]) continue;
      const once = machineData[asymKey({ ...coord, [first]: a })];
      if (once) return { runs: once, relaxed: [[first, a]] };
    }
  }
  for (const first of ASYM_RELAX_ORDER) {
    if (first === state.compare) continue;
    for (const second of ASYM_RELAX_ORDER) {
      if (second === first || second === state.compare) continue;
      for (const a of category.dimensions[first] || []) {
        for (const b of category.dimensions[second] || []) {
          const twice = machineData[asymKey({ ...coord, [first]: a, [second]: b })];
          if (twice) return { runs: twice, relaxed: [[first, a], [second, b]] };
        }
      }
    }
  }
  return null;
}

// Whether a radio dimension set to `value` leaves any curve on the chart, with
// the rest of the selection as it stands.
function asymRadioHasData(category, state, machineData, dim, value) {
  const probe = {
    ...state,
    selected: { ...state.selected, [dim]: new Set([value]) },
  };
  return asymSelected(category, probe, probe.compare)
    .some((v) => machineData[asymKey(Object.fromEntries(ASYM_DIMS.map((d) => [
      d, d === probe.compare ? v : asymSelected(category, probe, d)[0]])))]);
}

// Move every radio dimension (except `keep`) off a value that has no exact data
// onto the first that does, so that changing one dimension of a correlated pair
// carries the other along instead of leaving the bar describing a combination
// the category never measured.
function asymSnapRadios(category, state, machineData, keep) {
  for (const dim of ASYM_DIMS) {
    if (dim === state.compare || dim === keep) continue;
    const current = asymSelected(category, state, dim)[0];
    if (current !== undefined &&
        asymRadioHasData(category, state, machineData, dim, current)) continue;
    const replacement = (category.dimensions[dim] || [])
      .find((v) => asymRadioHasData(category, state, machineData, dim, v));
    if (replacement !== undefined) state.selected[dim] = new Set([replacement]);
  }
}

function asymSnap(category, state, machineData) {
  asymSnapRadios(category, state, machineData, null);
}

// Values of a dimension worth offering — the ones that would actually draw
// something. Decided by trying it: copy the state, select the value, snap the
// other radio dimensions the way a real click would, and see whether any curve
// survives.
function asymAvailable(category, state, machineData, dim) {
  return new Set((category.dimensions[dim] || []).filter((value) => {
    const probe = {
      compare: state.compare,
      xAxis: state.xAxis,
      selected: Object.fromEntries(
        ASYM_DIMS.map((d) => [d, new Set(state.selected[d])])),
    };
    probe.selected[dim] = new Set([value]);
    if (dim !== probe.compare) asymSnapRadios(category, probe, machineData, dim);
    if (dim === probe.compare) {
      return asymSeries(category, probe, machineData, value);
    }
    return asymSelected(category, probe, probe.compare)
      .some((v) => asymSeries(category, probe, machineData, v));
  }));
}

// The values of a dimension this machine ever recorded, whatever the rest of
// the selection is. A dimension that has only one is not a control — it is a
// caption, and the section header already carries it — so the filter bar leaves
// it out entirely rather than showing a lone chip that does nothing when
// clicked. Read off the machine's own keys rather than the category's declared
// dimensions, so a machine that ran part of the cube gets a bar describing what
// it has.
function asymPresentValues(machineData, dim) {
  const index = ASYM_DIMS.indexOf(dim);
  const seen = new Set();
  for (const key of Object.keys(machineData)) seen.add(key.split("|")[index]);
  return seen;
}

// The algorithms this machine measured for one problem.
function asymAlgorithmsFor(machineData, problem) {
  const pi = ASYM_DIMS.indexOf("problem");
  const ai = ASYM_DIMS.indexOf("algorithm");
  const seen = new Set();
  for (const key of Object.keys(machineData)) {
    const parts = key.split("|");
    if (parts[pi] === problem) seen.add(parts[ai]);
  }
  return seen;
}

// Whether the algorithm is a function of the problem in this category: every
// problem it measured has exactly one.
//
// This is the structural fact the bar is built around. Where it holds, naming
// the problem names the algorithm, so the algorithm is not a choice at all —
// the field goes away and its value rides along in each curve's legend entry —
// and several problems can share a chart, because each curve still stands for
// exactly one algorithm. Where it fails, a chart of several problems would be
// showing one algorithm per problem and silently hiding the others, so the
// problems are compared one at a time and the algorithm field is the axis.
function asymAlgorithmImplied(category, machineData) {
  for (const problem of asymPresentValues(machineData, "problem")) {
    if (asymAlgorithmsFor(machineData, problem).size > 1) return false;
  }
  return true;
}

// The chips a dimension offers. Everything but the algorithm offers what this
// machine recorded; the algorithm offers nothing at all where it is implied by
// the problem, and otherwise only what was measured for the problem selected —
// an algorithm belonging to some other problem is not an alternative to the one
// in the bar, it is a different measurement wearing the same axis.
function asymDimValues(category, state, machineData, dim) {
  const declared = category.dimensions[dim] || [];
  if (dim === "algorithm") {
    if (asymAlgorithmImplied(category, machineData)) return [];
    const problem = asymSelected(category, state, "problem")[0];
    const measured = asymAlgorithmsFor(machineData, problem);
    return declared.filter((v) => measured.has(v));
  }
  const present = asymPresentValues(machineData, dim);
  return declared.filter((v) => present.has(v));
}

// Whether a dimension may hold the compare axis at all.
const asymCanCompare = (category, machineData, dim) =>
  dim !== "problem" || asymAlgorithmImplied(category, machineData);

// Bring the selection back inside what the bar can express, after a click that
// changed what the other fields offer — switching problem in a category where
// the algorithm is a real choice changes which algorithms exist.
function asymReconcile(category, state, machineData) {
  if (!asymCanCompare(category, machineData, state.compare)) {
    const kept = [...state.selected[state.compare]][0];
    if (kept !== undefined) state.selected[state.compare] = new Set([kept]);
    state.compare = ASYM_DIMS.find(
      (d) => asymCanCompare(category, machineData, d)) || "type";
  }
  if (asymAlgorithmImplied(category, machineData)) return;
  const allowed = asymDimValues(category, state, machineData, "algorithm");
  if (!allowed.length) return;
  const kept = allowed.filter((v) => state.selected.algorithm.has(v));
  if (kept.length) {
    state.selected.algorithm = new Set(kept);
  } else {
    // Nothing selected survives the new problem. On the compare axis that is
    // the moment to show what the problem does offer — the two ways of
    // locating a point, say — rather than an arbitrary one of them.
    state.selected.algorithm = new Set(
      state.compare === "algorithm" ? allowed : allowed.slice(0, 1));
  }
}

// The values of `dim` recorded for the current slice *exactly*, with no
// relaxation — the curves this dimension can put side by side inside one
// genuine slice of the cube.
//
// This is what a dimension is populated with when it becomes the compare axis.
// Selecting everything merely *reachable* would be wrong: in a category whose
// problems each have their own algorithm, that pulls the other problems' curves
// onto the chart, so asking to compare the two ways of locating a point would
// hand back the triangulation build and the index build as well.
function asymExactValues(category, state, machineData, dim) {
  const probe = { ...state, compare: dim };
  return (category.dimensions[dim] || []).filter((value) => {
    const coord = Object.fromEntries(ASYM_DIMS.map((d) => [
      d, d === dim ? value : asymSelected(category, probe, d)[0]]));
    return machineData[asymKey(coord)];
  });
}

function asymInitState(name, category, machineData) {
  const selected = {};
  const state = { selected, compare: ASYM_DIMS[0], xAxis: "size" };
  // Every field on its first value, so the fields the pass below leaves alone
  // are already settled — and so asymDimValues can read the selected problem
  // while deciding what the algorithm field offers.
  for (const d of ASYM_DIMS) {
    selected[d] = new Set(asymDimValues(category, state, machineData, d).slice(0, 1));
  }

  const opened = ASYM_DIMS.find((dim) => {
    if (!asymCanCompare(category, machineData, dim)) return false;
    const values = asymDimValues(category, state, machineData, dim);
    if (values.length < 2) return false;
    // Every value that draws something. One that draws nothing at all is a
    // disabled chip, and selecting it would look like a curve had gone missing.
    const drawable = asymAvailable(category, state, machineData, dim);
    const chosen = values.filter((v) => drawable.has(v));
    state.compare = dim;
    state.selected[dim] = new Set(chosen.length ? chosen : values);
    return true;
  });
  // No field offers a choice in the opening slice — a category whose only real
  // one appears once a different problem is picked. The axis parks on the first
  // field that can hold it and has values to come, so that picking that problem
  // brings them up together rather than one at a time.
  if (!opened) {
    state.compare =
      ASYM_DIMS.find((d) => asymCanCompare(category, machineData, d) &&
                            asymPresentValues(machineData, d).size > 1) ||
      ASYM_DIMS.find((d) => asymCanCompare(category, machineData, d));
  }
  asymState[name] = state;
  return state;
}

// Move the compare axis to `dim`, collapsing the dimension that held it to a
// single value — only one dimension can be multi-select at a time.
function asymPromote(state, dim) {
  if (state.compare === dim) return;
  const previous = state.compare;
  const kept = [...state.selected[previous]][0];
  if (kept !== undefined) state.selected[previous] = new Set([kept]);
  state.compare = dim;
}

// Whether making `dim` the compare axis would cost nothing. Only one dimension
// can be multi-select at a time, so promoting one collapses whichever held the
// axis before; when every other dimension already stands on a single value
// there is nothing to collapse and nothing to lose. That is the case where a
// plain click on a radio chip adds a curve instead of switching to it — asking
// for a second algorithm should not require knowing that ctrl-click means
// "and", and where the promotion is free there is no reading of the click that
// the user has to be protected from.
const asymPromotionIsFree = (state, dim) =>
  ASYM_DIMS.every((d) => d === dim || state.selected[d].size <= 1);

// The selected values of a dimension, in the payload's display order.
const asymSelected = (category, state, dim) =>
  (category.dimensions[dim] || []).filter((v) => state.selected[dim].has(v));

function asymFilterBar(name, category, state, machineData) {
  const bar = document.createElement("div");
  bar.className = "filters asym-filters";

  for (const dim of ASYM_DIMS) {
    const values = asymDimValues(category, state, machineData, dim);
    if (values.length < 2) continue;
    const canCompare = asymCanCompare(category, machineData, dim);

    const group = document.createElement("div");
    group.className = "filter-group";

    // A dimension that cannot hold the compare axis gets a plain caption
    // instead of a button, and says why: there is nothing to click, and an
    // unexplained dead label is worse than no label.
    const label = document.createElement(canCompare ? "button" : "span");
    if (canCompare) label.type = "button";
    label.className = "filter-label" +
      (state.compare === dim ? " comparing" : canCompare ? "" : " static");
    label.textContent = ASYM_DIM_LABEL[dim];
    label.title = !canCompare
      ? "Each problem here is measured with more than one algorithm, so a " +
        "chart of several problems would hide all but one of them. Problems " +
        "are compared one at a time; compare the algorithms instead."
      : state.compare === dim
        ? "This is the compare axis: its selected values are the curves."
        : "Compare along this dimension";
    if (canCompare) {
      label.addEventListener("click", () => {
        if (state.compare === dim) return;
        asymPromote(state, dim);
        const inBar = (list) => [...list].filter((v) => values.includes(v));
        const exact = inBar(asymExactValues(category, state, machineData, dim));
        state.selected[dim] = new Set(exact.length > 1 ? exact
          : inBar(asymAvailable(category, state, machineData, dim)));
        if (!state.selected[dim].size) state.selected[dim] = new Set(values.slice(0, 1));
        renderCategory(name);
      });
    }
    group.appendChild(label);

    const chips = document.createElement("div");
    chips.className = "chips";
    const comparing = state.compare === dim;
    const free = !comparing && canCompare && asymPromotionIsFree(state, dim);
    const chosen = asymSelected(category, state, dim);
    const available = asymAvailable(category, state, machineData, dim);
    for (const value of values) {
      const on = state.selected[dim].has(value);
      const usable = available.has(value);
      const chip = document.createElement("button");
      chip.type = "button";
      chip.className = "chip" + (on ? " on" : "") + (usable ? "" : " unavailable");
      chip.disabled = !usable && !on;
      chip.textContent = value;
      if (!usable) {
        chip.title = "Not measured in this combination";
      } else if (!comparing) {
        if (!on) {
          chip.title = free ? "Draw it alongside what is selected"
            : canCompare ? "Switch to it — ctrl-click to draw both as well"
            : "Switch to it";
        }
      } else {
        // Say up front when picking this would draw a curve from a different
        // slice than the radio buttons name — the same thing the legend
        // qualifier says once it is on the chart.
        const series = asymSeries(category, state, machineData, value);
        if (series && series.relaxed.length) {
          const where = series.relaxed
            .map(([d, v]) => `${ASYM_DIM_LABEL[d].toLowerCase()} ${v}`).join(", ");
          chip.title = `Only measured at ${where}; selecting it draws that curve.`;
        }
      }
      if (comparing && on) {
        // The chip wears its curve's colour, so the legend is the filter bar.
        const color = CURVE_COLORS[chosen.indexOf(value) % CURVE_COLORS.length];
        chip.style.background = color;
        chip.style.borderColor = color;
        chip.style.color = "#fff";
      }
      chip.addEventListener("click", (event) => {
        if (comparing) {
          if (on) {
            if (state.selected[dim].size > 1) state.selected[dim].delete(value);
          } else {
            state.selected[dim].add(value);
          }
        } else if (free ||
                   (canCompare && (event.shiftKey || event.ctrlKey || event.metaKey))) {
          asymPromote(state, dim);
          state.selected[dim].add(value);
        } else {
          state.selected[dim] = new Set([value]);
          asymSnapRadios(category, state, machineData, dim);
        }
        renderCategory(name);
      });
      chips.appendChild(chip);
    }
    group.appendChild(chips);
    bar.appendChild(group);
  }

  // What time is plotted against. The y axis is always time; the choice is
  // whether the x axis is the input the sweep controls or the output the run
  // produced. Output size is recorded on every row — it is the same number the
  // correctness cross-check compares — so it is always offered, but it earns
  // its place in the categories whose cost is driven by what comes out rather
  // than by what goes in: plotted that way, an output-sensitive algorithm is a
  // straight line where against n it is a parabola.
  const xGroup = document.createElement("div");
  xGroup.className = "filter-group";
  const xLabel = document.createElement("span");
  xLabel.className = "filter-label static";
  xLabel.textContent = "X axis";
  xGroup.appendChild(xLabel);
  const xChips = document.createElement("div");
  xChips.className = "chips";
  for (const [key, text] of [["size", "input size"], ["result", "output size"]]) {
    const chip = document.createElement("button");
    chip.type = "button";
    chip.className = "chip" + (state.xAxis === key ? " on" : "");
    chip.textContent = text;
    chip.addEventListener("click", () => { state.xAxis = key; renderCategory(name); });
    xChips.appendChild(chip);
  }
  xGroup.appendChild(xChips);
  bar.appendChild(xGroup);

  return bar;
}

// The Chart.js datasets for one category: one line per (compare value, run),
// newest run at full strength and older ones faded, plus the CGAL curve.
function asymDatasets(category, state, machine, depth) {
  const machineData = (category.data && category.data[machine]) || {};
  const values = asymSelected(category, state, state.compare);
  // Points are laid out against the chosen x and sorted by it: output size is
  // not always monotone in n, and a line drawn in sweep order would zigzag.
  const xOf = (p) => (state.xAxis === "result" ? Number(p.result) : p.size);
  const laid = (list) => list
    .map((p) => ({ x: xOf(p), y: p.time, point: p }))
    .filter((p) => Number.isFinite(p.x) && Number.isFinite(p.y))
    .sort((a, b) => a.x - b.x);

  const datasets = [];
  // The dimensions each drawn curve actually resolved to, for the CGAL lookup
  // below — this has to follow the same relaxation, or a relaxed curve would
  // be given the reference belonging to the slice it isn't in.
  const resolved = [];
  let latestMax = 0;
  values.forEach((value, index) => {
    const color = CURVE_COLORS[index % CURVE_COLORS.length];
    const series = asymSeries(category, state, machineData, value);
    if (!series || !series.runs.length) return;
    // Say so when this curve had to override a radio button to exist at all.
    const qualifier = series.relaxed.length
      ? ` (${series.relaxed.map(([, v]) => v).join(", ")})` : "";
    const shown = series.runs.slice(Math.max(0, series.runs.length - depth));
    const at = Object.fromEntries(series.relaxed);
    resolved.push({
      value,
      color,
      dataset: state.compare === "dataset" ? value
        : at.dataset ?? asymSelected(category, state, "dataset")[0],
      problem: state.compare === "problem" ? value
        : at.problem ?? asymSelected(category, state, "problem")[0],
      algorithm: state.compare === "algorithm" ? value
        : at.algorithm ?? asymSelected(category, state, "algorithm")[0],
    });
    shown.forEach((run, position) => {
      const newest = position === shown.length - 1;
      const points = laid(run.points);
      if (!points.length) return;
      if (newest) {
        latestMax = Math.max(latestMax, ...points.map((p) => p.y));
      }
      // Older runs fade towards the background rather than towards grey: the
      // hue still says which curve it is, the shade says how old.
      const alpha = newest ? 1 : 0.18 + 0.5 * ((position + 1) / shown.length);
      datasets.push({
        label: newest ? `${value}${qualifier}` : `${value}${qualifier} · ${run.commit}`,
        data: points,
        borderColor: newest ? color : hexToRgba(color, alpha),
        backgroundColor: newest ? color : hexToRgba(color, alpha),
        borderWidth: newest ? 2 : 1,
        pointRadius: newest ? 2 : 0,
        pointHoverRadius: 5,
        tension: 0,
        fill: false,
        order: newest ? 0 : 3,
        // Older runs stay off the legend: they are the same curve, and one
        // legend entry per (curve × depth) would bury the four or five that
        // actually name something.
        legendEntry: newest,
      });
    });
  });

  // The CGAL reference curves. Most baselines apply to every pgl algorithm for
  // their dataset and problem. A baseline may instead name `for_algorithm`, in
  // which case it belongs only to that pgl series. This lets, for example, the
  // ordinary and hierarchy CGAL locators follow Triangulation's walk and
  // preprocessed curves respectively.
  const seen = new Set();
  const references = [];
  for (const { value, color, dataset, problem, algorithm } of resolved) {
    const key = `${dataset}|${problem}`;
    const found = (category.baseline && category.baseline[key]) || [];
    found.forEach((baseline, rank) => {
      if (baseline.for_algorithm && baseline.for_algorithm !== algorithm) return;
      // A number-type comparison repeats the same algorithm, so its reference
      // must only be drawn once. Algorithm-specific references use a separate
      // key so selecting walk and preprocessed draws one of each.
      const referenceKey = baseline.for_algorithm
        ? `${key}|${algorithm}|${baseline.algorithm}`
        : `${key}|${baseline.algorithm}`;
      if (seen.has(referenceKey)) return;
      seen.add(referenceKey);
      const points = laid(baseline.points);
      if (points.length) references.push({ value, color, baseline, points, rank });
    });
  }

  // A baseline associated with one pgl algorithm always takes that curve's
  // colour. Unassociated references only take a colour when dataset or problem
  // is being compared, where each one belongs to a distinct curve.
  const specific = values.length > 1 &&
    state.compare !== "algorithm" && state.compare !== "type";
  for (const { value, color, baseline, points, rank } of references) {
    const label = `${baseline.algorithm} (${baseline.number})`;
    const paired = Boolean(baseline.for_algorithm);
    const stroke = paired || specific ? color : BASELINE_COLOR;
    // Where a curve has several references, colour can no longer tell them
    // apart — it is already saying which curve they belong to — so the dash
    // pattern does.
    const dash = BASELINE_DASHES[rank % BASELINE_DASHES.length];
    datasets.push({
      label: paired || specific ? `${label} · ${value}` : label,
      data: points,
      borderColor: stroke,
      backgroundColor: stroke,
      borderDash: dash,
      borderWidth: 1.5,
      // The legend draws point styles, so a reference that now wears a curve's
      // colour would otherwise be a second dot of that colour. A dashed line
      // swatch keeps "this one is CGAL" readable from the legend alone.
      pointStyle: "line",
      pointRadius: 0,
      pointHoverRadius: 5,
      tension: 0,
      fill: false,
      order: 2,
      legendEntry: true,
    });
  }
  return { datasets, latestMax };
}

function asymChart(canvas, name, category, state, machine, depth) {
  const { datasets, latestMax } = asymDatasets(category, state, machine, depth);
  if (asymCharts[name]) asymCharts[name].destroy();
  if (!datasets.length) {
    asymCharts[name] = null;
    return false;
  }

  const yTitle = `time (${category.unit || "µs"})`;
  const xTitle = state.xAxis === "result" ? "output size" : "input size (n)";
  // The y scale is fitted to the newest run alone. An older run or the CGAL
  // curve that happens to be much slower is cropped rather than allowed to
  // squash the curve the page is actually about into the bottom inch.
  const yMax = latestMax > 0 ? latestMax * 1.08 : undefined;

  asymCharts[name] = new Chart(canvas, {
    type: "line",
    data: { datasets },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      parsing: false,
      normalized: true,
      animation: false,
      interaction: { mode: "nearest", intersect: false, axis: "x" },
      scales: {
        x: {
          type: "linear",
          title: { display: true, text: xTitle },
          beginAtZero: true,
          ticks: { maxTicksLimit: 9 },
        },
        y: {
          type: "linear",
          title: { display: true, text: yTitle },
          beginAtZero: true,
          max: yMax,
        },
      },
      plugins: {
        legend: {
          display: true, position: "top",
          labels: {
            boxWidth: 14, usePointStyle: true,
            filter: (item, data) => data.datasets[item.datasetIndex].legendEntry,
          },
        },
        tooltip: {
          callbacks: {
            // n stays in the tooltip whichever axis it is on: with output size
            // on the x axis it is the only thing that says which run a point
            // came from.
            title: (items) => `n = ${items[0].raw.point.size}`,
            label: (item) => {
              const p = item.raw.point;
              return `${item.dataset.label}: ${fmt(p.time)} ${category.unit || "µs"}` +
                     `  ·  output ${p.result}`;
            },
          },
        },
      },
    },
  });
  return true;
}

// The per-category section skeletons, built once and then updated in place.
//
// Rebuilding the whole page on every chip click would work, but it empties the
// document for an instant, and the browser answers a page that briefly has no
// height by scrolling back to the top — so clicking a filter in the last
// category would throw the reader up to the first. A click changes one
// category, so it redraws one category: the headings and the chart holders
// stay put (the holder is a fixed height, so nothing reflows either), and only
// that section's filter bar and chart are replaced.
const asymSections = {};

function asymBuildSections(names) {
  const root = document.getElementById("asymptotic");
  root.innerHTML = "";
  for (const key of Object.keys(asymSections)) delete asymSections[key];

  for (const name of names) {
    const category = DB.asymptotic[name];

    const section = document.createElement("section");
    section.className = "suite asym-category";

    const heading = document.createElement("h2");
    heading.innerHTML = category.source_url
      ? `<a class="suite-link" href="${category.source_url}" target="_blank" rel="noopener">${name}</a>`
      : name;
    if (category.description) {
      const desc = document.createElement("span");
      desc.className = "suite-desc";
      desc.textContent = category.description;
      heading.appendChild(desc);
    }
    section.appendChild(heading);

    // Placeholder for the filter bar; replaced, never emptied, on every redraw.
    const filters = document.createElement("div");
    section.appendChild(filters);

    const holder = document.createElement("div");
    holder.className = "asym-chart";
    const canvas = document.createElement("canvas");
    const empty = document.createElement("p");
    empty.className = "empty-state";
    empty.textContent = "Nothing recorded for this combination on this machine.";
    empty.style.display = "none";
    holder.append(canvas, empty);
    section.appendChild(holder);

    root.appendChild(section);
    asymSections[name] = { filters, canvas, empty };
  }
}

// Redraw one category: its filter bar and its chart, nothing else.
function renderCategory(name) {
  const parts = asymSections[name];
  if (!parts) return;
  const category = DB.asymptotic[name];
  const machine = document.getElementById("machine").value;
  const depth = historyDepth();
  const machineData = (category.data && category.data[machine]) || {};
  const state = asymState[name] || asymInitState(name, category, machineData);
  // Reconcile first: the snap decides whether a radio value has data by looking
  // at the compare axis's selection, so it has to see the algorithms that
  // belong to the problem now selected. The other way round it reads a problem
  // against the previous problem's algorithm, finds nothing, and pushes the
  // problem back to where it was.
  asymReconcile(category, state, machineData);
  asymSnap(category, state, machineData);

  const bar = asymFilterBar(name, category, state, machineData);
  parts.filters.replaceWith(bar);
  parts.filters = bar;

  const drawn = asymChart(parts.canvas, name, category, state, machine, depth);
  parts.canvas.style.display = drawn ? "" : "none";
  parts.empty.style.display = drawn ? "none" : "";
}

function renderAsymptotic() {
  const names = Object.keys(DB.asymptotic || {});
  if (!names.length) {
    document.getElementById("asymptotic").innerHTML =
      '<p class="empty-state">No asymptotic benchmark data recorded yet. Run ' +
      "<code>bash tests/benchmark/record.sh</code>, which records into the " +
      "benchmark data repository.</p>";
    return;
  }
  // Only the first call builds the DOM; later ones (a machine or history-depth
  // change) redraw every category in place, which keeps the scroll position
  // for those too.
  if (Object.keys(asymSections).length !== names.length) asymBuildSections(names);
  for (const name of names) renderCategory(name);

  const generated = DB.generated ? new Date(DB.generated) : null;
  const stamp = document.getElementById("generated");
  if (stamp && generated) stamp.textContent = `updated ${generated.toLocaleDateString()}`;
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
  const mins = points.map((p) => (p.min !== undefined ? p.min : p.time));
  const maxs = points.map((p) => (p.max !== undefined ? p.max : p.time));

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
          // Median — the main, thicker line, drawn on top with coloured points.
          label: "median",
          data: values,
          borderColor: "#0a429e",
          pointBackgroundColor: pointColors,
          pointBorderColor: "#fff",
          pointBorderWidth: 1,
          pointRadius: pointRadii,
          pointHoverRadius: 7,
          borderWidth: 2,
          tension: 0,
          fill: false,
          order: 0,
        },
        {
          // Max — thin line; the band down to min (dataset 2) is shaded.
          label: "max",
          data: maxs,
          borderColor: "rgba(10,66,158,.45)", borderWidth: 1,
          pointRadius: 0, tension: 0, fill: false, order: 2,
        },
        {
          // Min — thin line, fills the area up to the max line (dataset 1).
          label: "min",
          data: mins,
          borderColor: "rgba(10,66,158,.45)", borderWidth: 1,
          backgroundColor: "rgba(10,66,158,.12)",
          pointRadius: 0, tension: 0, fill: { target: 1 }, order: 2,
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
