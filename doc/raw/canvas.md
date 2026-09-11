<img align="left" src="figures/logo.png" width="23%"/>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="figures/logotextdark.svg"/>
  <img alt="Pangolin: Plane Geometry Library" src="figures/logotext.svg" width="65%"/>
</picture>

[![Tests](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml/badge.svg)](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml)
[![Standard](https://img.shields.io/badge/C%2B%2B-20/23/26-rgb(10,66,158).svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-MIT-rgb(216,134,42).svg)](https://opensource.org/licenses/MIT)
[![Benchmarks](https://img.shields.io/badge/benchmarks-online-rgb(21,153,135).svg)](https://gfonsecabr.github.io/pgl/benchmarks/index.html)

<br/>

> ℹ️ **Pre-release**: pgl is extensively tested, but it has not had a stable release yet and its API may still change.

## Canvas

`Canvas` is a lightweight SVG renderer for Pangolin shapes. It is designed for
inspection, debugging, examples, and test output: you push shapes into a
canvas, optionally change the drawing style in between, and then export the
result as an SVG, PDF, or ipe file.

The canvas automatically fits the inserted geometry into the output image,
clips infinite primitives to the visible viewport, and
stores an SVG `<title>` for each inserted element so that exported shapes can be identified precisely.

<table>
  <tr>
    <td valign="top" width="60%">

```c++
#include "pgl.hpp"

int main() {
    pgl::Point p = {1, 0}, q = {4, 7};
    pgl::Segment s = {p, q}, t = {0, 8, 2, 1};
    pgl::Canvas canvas;

    // Draw the two segments s,t with distinct colors
    canvas << pgl::stroke("royalblue") << pgl::fill("none") << s;
    canvas << pgl::stroke("darkorange") << t;
    // Then you can draw endpoints of s so they stay easy to spot
    canvas << pgl::stroke("black") << pgl::fill("black") << p << q;

    // `intersection` is an optional variant: an absent intersection draws nothing.
    canvas << pgl::stroke("crimson")
           << pgl::fill("none")
           << pgl::pointRadius("15px")
           << s.intersection(t); // exact default for integral inputs

    // We save the files in different formats
    canvas.writeSVG("example1.svg");
    canvas.writePDF("example1.pdf");
    canvas.writeIPE("example1.ipe");
}
```

  </td>
    <td valign="top" width="40%">
      <img src="figures/canvas_example_intersection.svg" alt="Canvas intersection example" width="100%"/>
    </td>
  </tr>
</table>

### Style

The canvas maintains a current style. When you insert a shape, that shape
captures the style that is active at that exact moment. Changing the style
afterwards affects only shapes inserted later.

```c++
pgl::Canvas canvas;
pgl::Segment firstSegment = {0, 0, 4, 3}, secondSegment = {0, 3, 4, 0};
canvas << pgl::stroke("royalblue") << firstSegment;
canvas << pgl::stroke("crimson") << secondSegment;
```

| Command | Effect |
| --- | --- |
| `pgl::stroke("value")` | Sets the stroke color or stroke paint used for subsequent shapes. Typical values are color names such as `"red"`, hex codes such as `"#3366cc"`, or any SVG paint value. |
| `pgl::fill("value")` | Sets the interior fill used for subsequent filled shapes and points. Use `"none"` to disable filling. |
| `pgl::fillOpacity("value")` | Sets the fill opacity for subsequent shapes. Values are forwarded as SVG strings, so `"0.2"` makes the fill translucent. |
| `pgl::strokeOpacity("value")` | Sets the stroke opacity for subsequent shapes. |
| `pgl::strokeWidth("value")` | Sets the stroke width for subsequent shapes using a raw SVG length string. |
| `pgl::pointRadius("value")` | Sets the rendered radius of subsequent `Point` objects. |
| `pgl::fontSize("value")` | Sets the font size, in pixels, of subsequent `pgl::Text` that takes its size from the canvas (see [Writing text](#writing-text)). The default is `"16"`. |

Example:

```c++
pgl::Canvas canvas;
pgl::Halfplane halfplane = {0, 0, 4, 2};
pgl::Rectangle rectangle = {{1, 1}, {3, 3}};
// Soft fill for the half-plane so the rest of the drawing still shows through.
canvas << pgl::stroke("teal")
       << pgl::fill("teal")
       << pgl::fillOpacity("0.18")
       << halfplane
       // Then switch gears and draw the rectangle
       << pgl::stroke("sienna")
       << pgl::fill("gold")
       << pgl::fillOpacity("0.22")
       << rectangle;
```

### Writing text

A `pgl::Text` writes a line of text on the canvas, centered either on a point
or inside a box. It is not a shape, only an instruction to the canvas, and like
a shape it captures the style active when it is inserted: the text is painted
in the current stroke color and stroke opacity, or in the fill color and fill
opacity when the stroke is `"none"`.

```c++
pgl::Canvas canvas;
pgl::Point p = {0, 0}, q = {10, 6};
pgl::Rectangle box = {pgl::Point{2, 1}, pgl::Point{8, 3}};

canvas << pgl::stroke("crimson") << pgl::Text("p", p);             // 16 pixels, the default
canvas << pgl::fontSize("24") << pgl::Text("q", q);                // 24 pixels
canvas << pgl::Text("scales", pgl::Point{5.0, 4.5}, 0.5);          // 0.5 plane units
canvas << pgl::stroke("royalblue") << pgl::Text("Fill the box", box);
canvas << pgl::Text("at most 24 pixels", box, pgl::TextFit::shrink);
```

The constructor decides where the font size comes from:

| Constructor | Font size |
| --- | --- |
| `Text(text, point)` | The current `fontSize`, in pixels. The text keeps its size however the drawing is scaled. |
| `Text(text, point, size)` | The given size, in plane units, so the text scales with the drawing as the shapes do. It must be strictly positive. |
| `Text(text, box)` | The largest size at which the text fits inside `box`. Same as passing `pgl::TextFit::fill`. |
| `Text(text, box, pgl::TextFit::shrink)` | The current `fontSize`, reduced only when the text would not fit inside `box`. |

Text takes part in fitting. A box, or the extent of text sized in plane units,
is part of the bounding box. Text sized in pixels widens the padding around the
drawing, as a point's radius does, so text on the edge of the drawing is not
cut off.

All three backends lay the text out with the metrics of Helvetica. The SVG asks
for Helvetica, then Arial, which has the same widths, so text fitted to a box
fills it in every format. PDF uses the standard Helvetica font, whose
encoding covers Latin-1: any other character prints as `?`. Ipe typesets the
text with LaTeX, in Helvetica (`\fontfamily{phv}`), with LaTeX's special
characters escaped so that the text prints exactly as written.

### Variants, optionals, and ranges

`Canvas` accepts the standard result wrappers used throughout Pangolin. It draws
the active value of an `std::variant`, draws an `std::optional` only when it has
a value, and inserts every object in an input range in iteration order. These
rules compose, so an optional variant or a vector of optional results can be
drawn directly.

```c++
pgl::Segment s = {0, 0, 4, 3}, t = {0, 3, 4, 0};
auto first = s.intersection(t); // std::optional<std::variant<...>>
std::vector results = {first};

canvas << pgl::stroke("crimson") << first;
canvas << pgl::stroke("darkorange") << results;
```

Every inserted object captures the style active when it is reached; empty
optionals add nothing.

### Configuration

These methods configure the exported image or update the current drawing
defaults:

| Method | What it changes |
| --- | --- |
| `scale(double factor)` | Multiplies the automatically fitted scale by `factor`. Values greater than `1` zoom in; values between `0` and `1` zoom out. The value must be strictly positive. |
| `width(double widthPixels)` | Sets the SVG width in pixels. The value must be strictly positive. |
| `height(double heightPixels)` | Sets the SVG height in pixels. The value must be strictly positive. |
| `size(double widthPixels, double heightPixels)` | Convenience wrapper for setting width and height together. |
| `margin(double marginPixels)` | Reserves blank space around the fitted drawing. Increasing the margin gives the geometry more breathing room inside the image. The value must be non-negative. |
| `view(const Rectangle& window)` | Fits the export to an explicit window of the plane instead of to the inserted geometry. Infinite primitives are clipped to the window and geometry outside it falls outside the image. `scale` and `margin` still apply on top, and calling it again replaces the window. |
| `borders(bool enabled = true)` | Enables or disables a thin rectangular frame around the whole SVG. This is especially helpful when debugging clipping and margins. |
| `writeSVG(const std::string& path)` | Writes the full SVG document to disk. Throws if the output file cannot be opened. |
| `toSVG()` | Returns the complete SVG document as a string, which is useful for tests, web responses, or custom output pipelines. |
| `writePDF(const std::string& path)` | Writes the full PDF document to disk. Throws if the output file cannot be opened or written. |
| `toPDF()` | Returns the complete PDF document as a byte string. |
| `writeIPE(const std::string& path)` | Writes the drawing as an Ipe XML document (`.ipe`) to disk, ready to open and edit in the [Ipe extensible drawing editor](https://ipe.otfried.org/). Throws if the output file cannot be opened or written. |
| `toIPE()` | Returns the complete Ipe XML document as a string. |


### How fitting works

Canvas fitting is automatic:

- the bounding boxes of all bounded shapes are collected, unless `view(window)` set an explicit window, which replaces them;
- infinite primitives are clipped to the visible viewport;
- the drawing is uniformly scaled to fit inside the chosen width and height;
- the aspect ratio is preserved;
- the configured margin and optional border are respected;
- the y-axis points upwards, as is standard in mathematics.

### Notes

- `Canvas` is intentionally lightweight. It is a geometry inspection tool, not a
  general plotting framework.
- An infinite primitive contributes the points that define it to the collected
  bounding box, so a line defined far from the rest of the drawing stretches the
  fit. That, and a drawing whose interesting part is much smaller than its
  bounding box, are what `view` is for.
- Shapes are stored in insertion order, and SVG output preserves that order, so
  later shapes are drawn on top of earlier ones.
- Because style is captured on insertion, it is easy to layer highlights on top
  of a base drawing by switching style right before inserting the highlighted
  object.
- In SVG, the shape's output string will be shown when you hover over the shape in a browser. Text has no such title, since it is already on display.
- A `PolygonWithHoles` is drawn as a single path with one closed subpath per
  ring, so its holes are punched out of the fill rather than painted over: SVG
  asks for `fill-rule="evenodd"`, and the PDF and Ipe backends get the same
  result from winding each hole against the outer ring.
- A `PolygonSet` is drawn the same way, as **one** path carrying every ring of
  every component. The whole set is a single element, so it has one style and
  one `<title>`, and a set that comes apart into several pieces stays one drawn
  object.
