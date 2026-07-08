<!-- AUTO-GENERATED from doc/raw/canvas.md by doc/raw/doxylink.py — do not edit; edit the raw version and regenerate. -->

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

> ⚠️ **Work in Progress**: This library is still under construction and contains **bugs and missing features**. Use in production environments is not recommended.

## Canvas

[`Canvas`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html "Stores drawable objects and exports them as an SVG image.") is a lightweight SVG renderer for Pangolin shapes. It is designed for
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

    if (s.intersects(t)) {
        // when they cross, we can highlight the exact intersection
        pgl::Shape crossing(s.intersection<pgl::Rational<int>>(t));
        canvas << pgl::stroke("crimson")
               << pgl::fill("none")
               << pgl::pointRadius("15px")
               << crossing;
    }

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
| [`pgl::stroke("value")`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a2e234c901f9abf59d6a354cdc9a9168f "Creates a command that changes the current stroke color.") | Sets the stroke color or stroke paint used for subsequent shapes. Typical values are color names such as `"red"`, hex codes such as `"#3366cc"`, or any SVG paint value. |
| [`pgl::fill("value")`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ac8cbf973d67ef1569c611788f93f6761 "Creates a command that changes the current fill color.") | Sets the interior fill used for subsequent filled shapes and points. Use `"none"` to disable filling. |
| [`pgl::fillOpacity("value")`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a8cf94a6c54fd68e2972ff7440eca978b "Creates a command that changes the current fill opacity.") | Sets the fill opacity for subsequent shapes. Values are forwarded as SVG strings, so `"0.2"` makes the fill translucent. |
| [`pgl::strokeOpacity("value")`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a7c60899586c30e2ea3c3aacfc192228e "Creates a command that changes the current stroke opacity.") | Sets the stroke opacity for subsequent shapes. |
| [`pgl::strokeWidth("value")`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a49586374ddc1970a6253da193b279526 "Creates a command that changes the current stroke width.") | Sets the stroke width for subsequent shapes using a raw SVG length string. |
| [`pgl::pointRadius("value")`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a1e29fcb65cc1bf621121dac846204aeb "Creates a command that changes the current point radius.") | Sets the rendered radius of subsequent [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload.") objects. |

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

### Configuration

These methods configure the exported image or update the current drawing
defaults:

| Method | What it changes |
| --- | --- |
| [`scale(double factor)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#a40b9eaa91fa720483a2999d09e89e47b "Sets the global zoom factor used during SVG export.") | Multiplies the automatically fitted scale by `factor`. Values greater than `1` zoom in; values between `0` and `1` zoom out. The value must be strictly positive. |
| [`width(double widthPixels)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#ae77d195e33e5becef12de273bd327f51 "Sets the exported SVG width in pixels.") | Sets the SVG width in pixels. The value must be strictly positive. |
| [`height(double heightPixels)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#a8256fca52537707a7288cd82a42f016c "Sets the exported SVG height in pixels.") | Sets the SVG height in pixels. The value must be strictly positive. |
| [`size(double widthPixels, double heightPixels)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#ab56282ffe4990051d30b69a0468e2394 "Sets the exported SVG size in pixels.") | Convenience wrapper for setting width and height together. |
| [`margin(double marginPixels)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#ac5d887cdc706ea1473bebdda02c2390d "Sets the margin reserved around the fitted drawing.") | Reserves blank space around the fitted drawing. Increasing the margin gives the geometry more breathing room inside the image. The value must be non-negative. |
| [`borders(bool enabled = true)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#a6621826426cfa82bd0b5fd8221b7376b "Enables or disables the optional border around the SVG.") | Enables or disables a thin rectangular frame around the whole SVG. This is especially helpful when debugging clipping and margins. |
| [`writeSVG(const std::string& path)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#ade8bd4b3d2c895f3659aa101552e3031 "Serializes the canvas to an SVG file.") | Writes the full SVG document to disk. Throws if the output file cannot be opened. |
| [`toSVG()`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#a9d02568a8bf29a1d7c7638a1186a9f14 "Serializes the canvas contents to an SVG string.") | Returns the complete SVG document as a string, which is useful for tests, web responses, or custom output pipelines. |
| [`writePDF(const std::string& path)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#acba14da7f7fc8599cf096bbda6e0075c "Serializes the canvas to a PDF file.") | Writes the full PDF document to disk. Throws if the output file cannot be opened or written. |
| [`toPDF()`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#aba98293b6bb5b0f48390b75ef4c3499a "Serializes the canvas contents to a PDF byte string.") | Returns the complete PDF document as a byte string. |
| [`writeIPE(const std::string& path)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#abc2dd77081933295d416cbd863f72d78 "Serializes the canvas to an Ipe XML file (.ipe).") | Writes the drawing as an Ipe XML document (`.ipe`) to disk, ready to open and edit in the [Ipe extensible drawing editor](https://ipe.otfried.org/). Throws if the output file cannot be opened or written. |
| [`toIPE()`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html#a39f0d14964126ee113728583b5bd9da9 "Serializes the canvas contents to an Ipe XML string.") | Returns the complete Ipe XML document as a string. |


### How fitting works

Canvas fitting is automatic:

- the bounding boxes of all bounded shapes are collected;
- infinite primitives are clipped to the visible viewport;
- the drawing is uniformly scaled to fit inside the chosen width and height;
- the aspect ratio is preserved;
- the configured margin and optional border are respected;
- the y-axis points upwards, as is standard in mathematics.

### Notes

- [`Canvas`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html "Stores drawable objects and exports them as an SVG image.") is intentionally lightweight. It is a geometry inspection tool, not a
  general plotting framework.
- Shapes are stored in insertion order, and SVG output preserves that order, so
  later shapes are drawn on top of earlier ones.
- Because style is captured on insertion, it is easy to layer highlights on top
  of a base drawing by switching style right before inserting the highlighted
  object.
- In SVG, the shape's output string will be shown when you hover over the shape in a browser.
