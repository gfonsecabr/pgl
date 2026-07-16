#pragma once

#include "implementation/distance.hpp"
#include "third_party/pdfgen.hpp"

/**
 * @file canvas.hpp
 * @brief Lightweight SVG canvas for drawing Pangolin shapes.
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>


namespace pgl {

/**
 * @brief Names the style property targeted by a canvas command.
 */
enum class CanvasProperty {
    stroke,
    fill,
    fillOpacity,
    strokeOpacity,
    strokeWidth,
    pointRadius,
};

/**
 * @brief Deferred style update applied to the current canvas style.
 */
struct CanvasCommand {
    CanvasProperty property;
    std::string value;
};

/**
 * @brief SVG style captured for each inserted element.
 */
struct CanvasStyle {
    std::string stroke = "black";
    std::string fill = "none";
    std::string fillOpacity = "1";
    std::string strokeOpacity = "1";
    std::string strokeWidth = "2";
    std::string pointRadius = "3";

    /**
     * @brief Applies one style command in place.
     *
     * @param command Command to apply.
     */
    void apply(const CanvasCommand& command) {
        switch (command.property) {
            case CanvasProperty::stroke:
                stroke = command.value;
                break;
            case CanvasProperty::fill:
                fill = command.value;
                break;
            case CanvasProperty::fillOpacity:
                fillOpacity = command.value;
                break;
            case CanvasProperty::strokeOpacity:
                strokeOpacity = command.value;
                break;
            case CanvasProperty::strokeWidth:
                strokeWidth = command.value;
                break;
            case CanvasProperty::pointRadius:
                pointRadius = command.value;
                break;
        }
    }
};

/** @brief Creates a command that changes the current stroke color. */
inline CanvasCommand stroke(std::string value) {
    return {CanvasProperty::stroke, std::move(value)};
}

/** @brief Creates a command that changes the current fill color. */
inline CanvasCommand fill(std::string value) {
    return {CanvasProperty::fill, std::move(value)};
}

/** @brief Creates a command that changes the current fill opacity. */
inline CanvasCommand fillOpacity(std::string value) {
    return {CanvasProperty::fillOpacity, std::move(value)};
}

/** @brief Creates a command that changes the current stroke opacity. */
inline CanvasCommand strokeOpacity(std::string value) {
    return {CanvasProperty::strokeOpacity, std::move(value)};
}

/** @brief Creates a command that changes the current stroke width. */
inline CanvasCommand strokeWidth(std::string value) {
    return {CanvasProperty::strokeWidth, std::move(value)};
}

/** @brief Creates a command that changes the current point radius. */
inline CanvasCommand pointRadius(std::string value) {
    return {CanvasProperty::pointRadius, std::move(value)};
}

/**
 * @brief Stores drawable objects and exports them as an SVG image.
 *
 * The canvas keeps a current SVG style. Each inserted primitive captures the
 * style that was active at insertion time.
 */
class Canvas {
  public:
    /**
     * @brief Creates an empty canvas with default style and viewport settings.
     */
    Canvas() = default;

    /**
     * @brief Sets the global zoom factor used during SVG export.
     *
     * @param factor Positive zoom multiplier.
     * @return This canvas.
     */
    Canvas& scale(double factor) {
        requireStrictlyPositive(factor, "scale");
        zoom_ = factor;
        return *this;
    }

    /**
     * @brief Sets the exported SVG width in pixels.
     *
     * @param widthPixels Positive width in pixels.
     * @return This canvas.
     */
    Canvas& width(double widthPixels) {
        requireStrictlyPositive(widthPixels, "width");
        widthPixels_ = widthPixels;
        return *this;
    }

    /**
     * @brief Sets the exported SVG height in pixels.
     *
     * @param heightPixels Positive height in pixels.
     * @return This canvas.
     */
    Canvas& height(double heightPixels) {
        requireStrictlyPositive(heightPixels, "height");
        heightPixels_ = heightPixels;
        return *this;
    }

    /**
     * @brief Sets the exported SVG size in pixels.
     *
     * @param widthPixels Positive width in pixels.
     * @param heightPixels Positive height in pixels.
     * @return This canvas.
     */
    Canvas& size(double widthPixels, double heightPixels) {
        return width(widthPixels).height(heightPixels);
    }

    /**
     * @brief Enables or disables the optional border around the SVG.
     *
     * @param enabled Whether to draw the border.
     * @return This canvas.
     */
    Canvas& borders(bool enabled = true) {
        drawBorder_ = enabled;
        return *this;
    }

    /**
     * @brief Sets the margin reserved around the fitted drawing.
     *
     * @param marginPixels Non-negative margin in pixels.
     * @return This canvas.
     */
    Canvas& margin(double marginPixels) {
        requireNonNegative(marginPixels, "margin");
        marginPixels_ = marginPixels;
        return *this;
    }

    /**
     * @brief Serializes the canvas to an SVG file.
     *
     * @param path Output file path.
     */
    void writeSVG(const std::string& path) const {
        std::ofstream output(path);
        if (!output) {
            throw std::runtime_error("Could not open SVG output file: " + path);
        }

        output << toSVG();
    }

    /**
     * @brief Serializes the canvas contents to an SVG string.
     *
     * @return Complete SVG document.
     */
    std::string toSVG() const {
        const Bounds bounds = computeBounds();
        const Viewport viewport = computeViewport(bounds);

        std::ostringstream out;
        appendDocumentOpen(out);

        if (needsArrowheadDefinition()) {
            appendArrowheadDefinition(out);
        }

        if (drawBorder_) {
            appendBorder(out);
        }

        for (const Element& element : elements_) {
            out << "  " << elementToSVG(element, viewport) << "\n";
        }

        out << "</svg>\n";
        return out.str();
    }

    /**
     * @brief Serializes the canvas to a PDF file.
     *
     * PDF export reuses the same fitted viewport as SVG export so the page
     * matches the canvas dimensions and overall layout. Stroke and fill opacity
     * are exported through standard PDF ExtGState resources (`/ca` and `/CA`).
     *
     * @param path Output file path.
     */
    void writePDF(const std::string& path) const {
        std::ofstream output(path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("Could not open PDF output file: " + path);
        }

        const std::string pdf = toPDF();
        output.write(pdf.data(), static_cast<std::streamsize>(pdf.size()));
        if (!output) {
            throw std::runtime_error("Could not write PDF output file: " + path);
        }
    }

    /**
     * @brief Serializes the canvas contents to a PDF byte string.
     *
     * @return Complete PDF document bytes.
     */
    std::string toPDF() const {
        const Bounds bounds = computeBounds();
        const Viewport viewport = computeViewport(bounds);

        pdfgen::pdf_info info{};
        std::snprintf(info.creator, sizeof(info.creator), "%s", "pgl::Canvas");
        std::snprintf(info.producer, sizeof(info.producer), "%s", "pgl");
        std::snprintf(info.title, sizeof(info.title), "%s", "Pangolin Canvas Export");

        std::unique_ptr<pdfgen::pdf_doc, void (*)(pdfgen::pdf_doc*)> pdf(
            pdfgen::pdf_create(static_cast<float>(widthPixels_), static_cast<float>(heightPixels_), &info),
            pdfgen::pdf_destroy
        );
        if (!pdf) {
            throw std::runtime_error("Could not create PDF document.");
        }

        pdfgen::pdf_object* page = pdfgen::pdf_append_page(pdf.get());
        if (page == nullptr) {
            throwPDFError(pdf.get(), "append PDF page");
        }

        if (drawBorder_) {
            const int result = pdfgen::pdf_add_rectangle(
                pdf.get(),
                page,
                0.5f,
                0.5f,
                static_cast<float>(std::max(widthPixels_ - 1.0, 0.0)),
                static_cast<float>(std::max(heightPixels_ - 1.0, 0.0)),
                1.0f,
                pdfgen::PDF_BLACK
            );
            if (result < 0) {
                throwPDFError(pdf.get(), "draw PDF border");
            }
        }

        for (const Element& element : elements_) {
            appendElementToPDF(pdf.get(), page, element, viewport);
        }

        std::string bytes;
        if (pdfgen::pdf_save_buffer(pdf.get(), bytes) < 0) {
            throwPDFError(pdf.get(), "serialize PDF");
        }
        return bytes;
    }

    /**
     * @brief Serializes the canvas to an Ipe XML file (.ipe).
     *
     * The exported document defines a single page whose layout matches the
     * canvas dimensions, reusing the same fitted viewport as SVG/PDF export.
     * Colors and pen widths are stored as absolute (non-symbolic) Ipe
     * attribute values. Opacities below 1 are declared as named `<opacity>`
     * entries in the style sheet, since Ipe requires the `opacity` and
     * `stroke-opacity` path attributes to reference a symbolic name.
     *
     * @param path Output file path.
     */
    void writeIPE(const std::string& path) const {
        std::ofstream output(path);
        if (!output) {
            throw std::runtime_error("Could not open IPE output file: " + path);
        }

        output << toIPE();
        if (!output) {
            throw std::runtime_error("Could not write IPE output file: " + path);
        }
    }

    /**
     * @brief Serializes the canvas contents to an Ipe XML string.
     *
     * @return Complete Ipe (.ipe) XML document.
     */
    std::string toIPE() const {
        const Bounds bounds = computeBounds();
        const Viewport viewport = computeViewport(bounds);
        const std::vector<int> opacities = collectIPEOpacities();

        std::ostringstream out;
        out << "<?xml version=\"1.0\"?>\n";
        out << "<!DOCTYPE ipe SYSTEM \"ipe.dtd\">\n";
        out << "<ipe version=\"70218\" creator=\"pgl::Canvas\">\n";
        out << "<ipestyle name=\"pgl\">\n";
        out << "<layout paper=\"" << widthPixels_ << ' ' << heightPixels_
            << "\" origin=\"0 0\" frame=\"" << widthPixels_ << ' ' << heightPixels_ << "\"/>\n";
        for (const int key : opacities) {
            out << "<opacity name=\"" << ipeOpacityName(key) << "\" value=\"" << (key / 1000.0) << "\"/>\n";
        }
        out << "</ipestyle>\n";
        out << "<page>\n";
        out << "<layer name=\"alpha\"/>\n";
        out << "<view layers=\"alpha\" active=\"alpha\"/>\n";

        if (drawBorder_) {
            appendIPEBorder(out);
        }

        for (const Element& element : elements_) {
            appendElementToIPE(out, element, viewport);
        }

        out << "</page>\n";
        out << "</ipe>\n";
        return out.str();
    }

    /**
     * @brief Applies a style command to the current drawing style.
     *
     * Existing elements keep their captured style.
     *
     * @param command Style command to apply.
     * @return This canvas.
     */
    Canvas& operator<<(const CanvasCommand& command) {
        style_.apply(command);
        return *this;
    }

    /** @brief Appends a point using the current captured style. */
    template <class Number, class Label>
    Canvas& operator<<(const Point<Number, Label>& point) {
        return push(Point<double>(point), point);
    }

    /** @brief Appends a segment using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const Segment<PointType>& segment) {
        return push(Segment<Point<double>>(segment), segment);
    }

    /** @brief Appends an oriented segment using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const OrientedSegment<PointType>& segment) {
        return push(OrientedSegment<Point<double>>(segment), segment);
    }

    /** @brief Appends a line using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const Line<PointType>& line) {
        return push(Line<Point<double>>(line), line);
    }

    /** @brief Appends an oriented line using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const OrientedLine<PointType>& line) {
        return push(OrientedLine<Point<double>>(line), line);
    }

    /** @brief Appends a ray using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const Ray<PointType>& ray) {
        return push(Ray<Point<double>>(ray), ray);
    }

    /** @brief Appends a half-plane using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const Halfplane<PointType>& halfplane) {
        return push(Halfplane<Point<double>>(halfplane), halfplane);
    }

    /** @brief Appends a rectangle using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const Rectangle<PointType>& rectangle) {
        return push(Rectangle<Point<double>>(rectangle), rectangle);
    }

    /** @brief Appends a triangle using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const Triangle<PointType>& triangle) {
        return push(Triangle<Point<double>>(triangle), triangle);
    }

    /** @brief Appends a convex polygon using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const Convex<PointType>& convex) {
        return push(Convex<Point<double>>(convex), convex);
    }

    /** @brief Appends a disk using the current captured style. */
    template <class PointType, class Label>
    Canvas& operator<<(const Disk<PointType, Label>& disk) {
        return push(Disk<Point<double>>(disk), disk);
    }

    /** @brief Appends a (possibly non-convex) polygon using the current captured style. */
    template <class PointType>
    Canvas& operator<<(const Polygon<PointType>& polygon) {
        return push(Polygon<Point<double>>(polygon), polygon);
    }

    /** @brief Appends an x-monotone chain (an SVG polyline) using the current captured style. */
    template <class PointType, class Label>
    Canvas& operator<<(const MonotoneChain<PointType, Label>& chain) {
        return push(MonotoneChain<Point<double>>(chain), chain);
    }

    /** @brief Appends a polyline (an open, possibly self-intersecting chain) using the current captured style. */
    template <class PointType, class Label>
    Canvas& operator<<(const Polyline<PointType, Label>& polyline) {
        return push(Polyline<Point<double>>(polyline), polyline);
    }

    /** @brief Appends a half-plane intersection (clipped to the viewport, like a half-plane) using the current captured style. */
    template <class PointType, class Label>
    Canvas& operator<<(const HalfplaneIntersection<PointType, Label>& region) {
        return push(HalfplaneIntersection<Point<double>>(region), region);
    }

    /** @brief Appends nothing: the empty shape has no geometry to draw. */
    template <class PointType>
    Canvas& operator<<(const EmptyShape<PointType>&) {
        return *this;
    }

    /** @brief Appends the currently stored alternative of a runtime shape. */
    template <class PointType>
    Canvas& operator<<(const Shape<PointType>& shape) {
        std::visit(
            [this](const auto& value) {
                *this << value;
            },
            shape.variant());
        return *this;
    }

  private:
    struct Bounds {
        double minX = 0.0;
        double minY = 0.0;
        double maxX = 0.0;
        double maxY = 0.0;
        bool initialized = false;

        void include(double x, double y) {
            if (!initialized) {
                minX = x;
                minY = y;
                maxX = x;
                maxY = y;
                initialized = true;
                return;
            }

            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }

        void include(const Bounds& other) {
            if (!other.initialized) {
                return;
            }

            include(other.minX, other.minY);
            include(other.maxX, other.maxY);
        }
    };

    struct Viewport {
        double scale = 1.0;
        double offsetX = 0.0;
        double offsetY = 0.0;

        double mapX(double x) const {
            return offsetX + scale * x;
        }

        double mapY(double y) const {
            return offsetY - scale * y;
        }

        double unmapX(double x) const {
            return (x - offsetX) / scale;
        }

        double unmapY(double y) const {
            return (offsetY - y) / scale;
        }
    };

    struct Element {
        Shape<Point<double>> shape{};
        CanvasStyle style{};
        std::string title;

        Bounds bounds() const {
            Bounds b;
            std::visit([&](const auto& value) {
                using V = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<V, Point<double>>) {
                    b.include(value.x(), value.y());
                } else if constexpr (std::same_as<V, Disk<Point<double>>>) {
                    const auto center = value.center();
                    const double radius = value.radius();
                    b.include(center.x() - radius, center.y() - radius);
                    b.include(center.x() + radius, center.y() + radius);
                } else if constexpr (std::same_as<V, HalfplaneIntersection<Point<double>>>) {
                    // The region may be unbounded: focus on its vertices and,
                    // like the Halfplane alternative, on the points defining
                    // its boundary lines.
                    for (const auto& vertexPoint : value.vertices()) {
                        b.include(vertexPoint.x(), vertexPoint.y());
                    }
                    for (const auto& halfplane : value) {
                        b.include(halfplane.source().x(), halfplane.source().y());
                        b.include(halfplane.target().x(), halfplane.target().y());
                    }
                } else {
                    for (std::size_t i = 0; i < value.size(); ++i) {
                        b.include(value[i].x(), value[i].y());
                    }
                }
            }, shape.variant());
            return b;
        }
    };

    static void requireStrictlyPositive(double value, const char* what) {
        if (value <= 0.0) {
            throw std::invalid_argument(std::string("Canvas ") + what + " must be strictly positive.");
        }
    }

    static void requireNonNegative(double value, const char* what) {
        if (value < 0.0) {
            throw std::invalid_argument(std::string("Canvas ") + what + " must be non-negative.");
        }
    }

    static std::string toString(double value) {
        std::ostringstream out;
        out << value;
        return out.str();
    }

    static std::string trim(const std::string& value) {
        std::size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
            ++start;
        }

        std::size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            --end;
        }

        return value.substr(start, end - start);
    }

    static std::optional<double> parseNumericLength(const std::string& value) {
        const std::string trimmed = trim(value);
        if (trimmed.empty()) {
            return std::nullopt;
        }

        std::size_t parsedCharacters = 0;
        try {
            const double numericValue = std::stod(trimmed, &parsedCharacters);
            if (parsedCharacters == trimmed.size()) {
                return numericValue;
            }
        } catch (...) {
        }

        return std::nullopt;
    }

    static std::string escapeXML(const std::string& value, bool escapeQuotes) {
        std::ostringstream out;
        for (const char character : value) {
            switch (character) {
                case '&':
                    out << "&amp;";
                    break;
                case '<':
                    out << "&lt;";
                    break;
                case '>':
                    out << "&gt;";
                    break;
                case '"':
                    if (escapeQuotes) {
                        out << "&quot;";
                    } else {
                        out << character;
                    }
                    break;
                default:
                    out << character;
                    break;
            }
        }
        return out.str();
    }

    static std::string styleAttributes(const CanvasStyle& style) {
        std::ostringstream out;
        out << " stroke=\"" << escapeXML(style.stroke, true) << '"'
            << " fill=\"" << escapeXML(style.fill, true) << '"'
            << " fill-opacity=\"" << escapeXML(style.fillOpacity, true) << '"'
            << " stroke-opacity=\"" << escapeXML(style.strokeOpacity, true) << '"'
            << " stroke-width=\"" << escapeXML(style.strokeWidth, true) << '"'
            << " vector-effect=\"non-scaling-stroke\"";
        return out.str();
    }

    static std::string fillAttributes(const CanvasStyle& style) {
        std::ostringstream out;
        out << " fill=\"" << escapeXML(style.fill, true) << '"'
            << " fill-opacity=\"" << escapeXML(style.fillOpacity, true) << '"';
        return out.str();
    }

    template <class T>
    static std::string titleOf(const T& object) {
        std::ostringstream out;
        out << object;
        return out.str();
    }

    template <class Stored, class Original>
    Canvas& push(Stored&& stored, const Original& original) {
        elements_.push_back({
            Shape<Point<double>>(std::forward<Stored>(stored)),
            style_,
            titleOf(original),
        });
        return *this;
    }

    bool needsArrowheadDefinition() const {
        for (const Element& element : elements_) {
            if (element.shape.template holdsAlternative<OrientedSegment<Point<double>>>() ||
                element.shape.template holdsAlternative<OrientedLine<Point<double>>>() ||
                element.shape.template holdsAlternative<Ray<Point<double>>>()) {
                return true;
            }
        }
        return false;
    }

    double padding() const {
        double value = marginPixels_;
        if (drawBorder_) {
            value += borderInset_;
        }

        for (const Element& element : elements_) {
            if (const std::optional<double> width = parseNumericLength(element.style.strokeWidth)) {
                value = std::max(value, marginPixels_ + *width / 2.0);
            }
            if (const std::optional<double> radius = parseNumericLength(element.style.pointRadius)) {
                value = std::max(value, marginPixels_ + *radius);
            }
        }

        return value;
    }

    Bounds computeBounds() const {
        Bounds bounds;
        for (const Element& element : elements_) {
            bounds.include(element.bounds());
        }
        return bounds;
    }

    Viewport computeViewport(const Bounds& bounds) const {
        Viewport viewport;
        if (!bounds.initialized) {
            viewport.offsetX = widthPixels_ / 2.0;
            viewport.offsetY = heightPixels_ / 2.0;
            return viewport;
        }

        const double inset = padding();
        const double drawableWidth = std::max(widthPixels_ - 2.0 * inset, 1.0);
        const double drawableHeight = std::max(heightPixels_ - 2.0 * inset, 1.0);
        const double contentWidth = bounds.maxX - bounds.minX;
        const double contentHeight = bounds.maxY - bounds.minY;

        const double scaleX = contentWidth == 0.0 ? pgl::detail::numeric_limits<double>::infinity() : drawableWidth / contentWidth;
        const double scaleY = contentHeight == 0.0 ? pgl::detail::numeric_limits<double>::infinity() : drawableHeight / contentHeight;

        double fittedScale = std::min(scaleX, scaleY);
        if (!std::isfinite(fittedScale)) {
            fittedScale = 1.0;
        }

        viewport.scale = fittedScale * zoom_;

        const double drawnWidth = contentWidth * viewport.scale;
        const double drawnHeight = contentHeight * viewport.scale;
        const double extraX = (drawableWidth - drawnWidth) / 2.0;
        const double extraY = (drawableHeight - drawnHeight) / 2.0;

        viewport.offsetX = inset + extraX - bounds.minX * viewport.scale;
        viewport.offsetY = heightPixels_ - inset - extraY + bounds.minY * viewport.scale;
        return viewport;
    }

    void appendDocumentOpen(std::ostringstream& out) const {
        out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
            << widthPixels_ << "\" height=\"" << heightPixels_
            << "\" viewBox=\"0 0 " << widthPixels_ << ' ' << heightPixels_ << "\">\n";
    }

    static void appendArrowheadDefinition(std::ostringstream& out) {
        out << "  <defs>\n";
        out << "    <marker id=\"pgl-arrowhead\" viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\" markerWidth=\"4\" markerHeight=\"4\" orient=\"auto\" markerUnits=\"strokeWidth\">\n";
        out << "      <path d=\"M 0 0 L 10 5 L 0 10 z\" fill=\"context-stroke\" stroke=\"none\"/>\n";
        out << "    </marker>\n";
        out << "  </defs>\n";
    }

    void appendBorder(std::ostringstream& out) const {
        out << "  <rect x=\"0.5\" y=\"0.5\" width=\"" << std::max(widthPixels_ - 1.0, 0.0)
            << "\" height=\"" << std::max(heightPixels_ - 1.0, 0.0)
            << "\" stroke=\"black\" fill=\"none\" stroke-width=\"1\"/>\n";
    }

    struct PDFStyle {
        std::uint32_t stroke = pdfgen::PDF_BLACK;
        std::uint32_t fill = pdfgen::PDF_TRANSPARENT;
        float strokeWidth = 1.0f;
        float pointRadius = 3.0f;
        float fillAlpha = 1.0f;
        float strokeAlpha = 1.0f;
    };

    static void throwPDFError(const pdfgen::pdf_doc* pdf, const std::string& action) {
        int errval = 0;
        const char* message = pdfgen::pdf_get_err(pdf, &errval);
        std::ostringstream out;
        out << "Could not " << action << ": " << (message != nullptr ? message : "unknown PDF error");
        if (errval != 0) {
            out << " (" << errval << ')';
        }
        throw std::runtime_error(out.str());
    }

    static float numericLengthOr(const std::string& value, float fallback) {
        if (const std::optional<double> numeric = parseNumericLength(value)) {
            return static_cast<float>(*numeric);
        }
        return fallback;
    }

    static float opacityOr(const std::string& value, float fallback) {
        if (const std::optional<double> numeric = parseNumericLength(value)) {
            if (std::isfinite(*numeric)) {
                return std::clamp(static_cast<float>(*numeric), 0.0f, 1.0f);
            }
        }
        return fallback;
    }

    static std::string lowercase(const std::string& value) {
        std::string result = value;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return result;
    }

    static std::optional<int> parseHexDigit(char character) {
        if ('0' <= character && character <= '9') return character - '0';
        if ('a' <= character && character <= 'f') return 10 + (character - 'a');
        if ('A' <= character && character <= 'F') return 10 + (character - 'A');
        return std::nullopt;
    }

    static std::optional<unsigned int> parseHexByte(char high, char low) {
        const auto highNibble = parseHexDigit(high);
        const auto lowNibble = parseHexDigit(low);
        if (!highNibble || !lowNibble) {
            return std::nullopt;
        }
        return static_cast<unsigned int>((*highNibble << 4) | *lowNibble);
    }

    static std::optional<unsigned int> parseRGBComponent(const std::string& value) {
        const std::string component = trim(value);
        if (component.empty()) {
            return std::nullopt;
        }
        std::size_t parsedCharacters = 0;
        try {
            const int parsed = std::stoi(component, &parsedCharacters);
            if (parsedCharacters != component.size() || parsed < 0 || parsed > 255) {
                return std::nullopt;
            }
            return static_cast<unsigned int>(parsed);
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::optional<std::uint32_t> parsePDFColor(const std::string& value) {
        const std::string normalized = lowercase(trim(value));
        if (normalized.empty()) {
            return std::nullopt;
        }
        if (normalized == "none") {
            return pdfgen::PDF_TRANSPARENT;
        }

        if (normalized.size() == 7 && normalized[0] == '#') {
            const auto red = parseHexByte(normalized[1], normalized[2]);
            const auto green = parseHexByte(normalized[3], normalized[4]);
            const auto blue = parseHexByte(normalized[5], normalized[6]);
            if (red && green && blue) {
                return pdfgen::PDF_RGB(*red, *green, *blue);
            }
        }

        if (normalized.size() == 4 && normalized[0] == '#') {
            const auto red = parseHexDigit(normalized[1]);
            const auto green = parseHexDigit(normalized[2]);
            const auto blue = parseHexDigit(normalized[3]);
            if (red && green && blue) {
                return pdfgen::PDF_RGB(
                    static_cast<unsigned int>(*red * 17),
                    static_cast<unsigned int>(*green * 17),
                    static_cast<unsigned int>(*blue * 17)
                );
            }
        }

        if (normalized.rfind("rgb(", 0) == 0 && normalized.back() == ')') {
            const std::string body = normalized.substr(4, normalized.size() - 5);
            std::vector<std::string> parts;
            std::size_t start = 0;
            while (start <= body.size()) {
                const std::size_t comma = body.find(',', start);
                if (comma == std::string::npos) {
                    parts.push_back(body.substr(start));
                    break;
                }
                parts.push_back(body.substr(start, comma - start));
                start = comma + 1;
            }
            if (parts.size() == 3) {
                const auto red = parseRGBComponent(parts[0]);
                const auto green = parseRGBComponent(parts[1]);
                const auto blue = parseRGBComponent(parts[2]);
                if (red && green && blue) {
                    return pdfgen::PDF_RGB(*red, *green, *blue);
                }
            }
        }

        static constexpr std::array<std::pair<const char*, std::uint32_t>, 30> namedColors{{
            {"black", pdfgen::PDF_BLACK},
            {"white", pdfgen::PDF_WHITE},
            {"red", pdfgen::PDF_RED},
            {"green", pdfgen::PDF_GREEN},
            {"blue", pdfgen::PDF_BLUE},
            {"yellow", pdfgen::PDF_RGB(255, 255, 0)},
            {"orange", pdfgen::PDF_RGB(255, 165, 0)},
            {"purple", pdfgen::PDF_RGB(128, 0, 128)},
            {"teal", pdfgen::PDF_RGB(0, 128, 128)},
            {"navy", pdfgen::PDF_RGB(0, 0, 128)},
            {"skyblue", pdfgen::PDF_RGB(135, 206, 235)},
            {"gold", pdfgen::PDF_RGB(255, 215, 0)},
            {"royalblue", pdfgen::PDF_RGB(65, 105, 225)},
            {"crimson", pdfgen::PDF_RGB(220, 20, 60)},
            {"darkgreen", pdfgen::PDF_RGB(0, 100, 0)},
            {"darkmagenta", pdfgen::PDF_RGB(139, 0, 139)},
            {"plum", pdfgen::PDF_RGB(221, 160, 221)},
            {"sienna", pdfgen::PDF_RGB(160, 82, 45)},
            {"brown", pdfgen::PDF_RGB(165, 42, 42)},
            {"gray", pdfgen::PDF_RGB(128, 128, 128)},
            {"grey", pdfgen::PDF_RGB(128, 128, 128)},
            {"silver", pdfgen::PDF_RGB(192, 192, 192)},
            {"aqua", pdfgen::PDF_RGB(0, 255, 255)},
            {"cyan", pdfgen::PDF_RGB(0, 255, 255)},
            {"magenta", pdfgen::PDF_RGB(255, 0, 255)},
            {"fuchsia", pdfgen::PDF_RGB(255, 0, 255)},
            {"lime", pdfgen::PDF_RGB(0, 255, 0)},
            {"olive", pdfgen::PDF_RGB(128, 128, 0)},
            {"maroon", pdfgen::PDF_RGB(128, 0, 0)},
            {"pink", pdfgen::PDF_RGB(255, 192, 203)},
        }};
        for (const auto& [name, colour] : namedColors) {
            if (normalized == name) {
                return colour;
            }
        }

        return std::nullopt;
    }

    static PDFStyle pdfStyleOf(const CanvasStyle& style) {
        PDFStyle result;
        result.stroke = parsePDFColor(style.stroke).value_or(pdfgen::PDF_BLACK);
        result.fill = parsePDFColor(style.fill).value_or(pdfgen::PDF_TRANSPARENT);
        result.strokeWidth = numericLengthOr(style.strokeWidth, 1.0f);
        result.pointRadius = numericLengthOr(style.pointRadius, 3.0f);
        result.fillAlpha = opacityOr(style.fillOpacity, 1.0f);
        result.strokeAlpha = opacityOr(style.strokeOpacity, 1.0f);
        return result;
    }

    float pdfYFromSVG(double svgY) const {
        return static_cast<float>(heightPixels_ - svgY);
    }

    template <class PointLike>
    std::pair<float, float> mapPDFPoint(const PointLike& point, const Viewport& viewport) const {
        return {
            static_cast<float>(viewport.mapX(point.x())),
            pdfYFromSVG(viewport.mapY(point.y())),
        };
    }

    static std::vector<pdfgen::pdf_path_operation> polygonPathOperations(
        const std::vector<std::pair<float, float>>& points,
        bool closePath) {
        std::vector<pdfgen::pdf_path_operation> operations;
        if (points.empty()) {
            return operations;
        }

        operations.reserve(points.size() + (closePath ? 1u : 0u));
        operations.push_back({'m', points[0].first, points[0].second, 0.0f, 0.0f, 0.0f, 0.0f});
        for (std::size_t index = 1; index < points.size(); ++index) {
            operations.push_back({'l', points[index].first, points[index].second, 0.0f, 0.0f, 0.0f, 0.0f});
        }
        if (closePath) {
            operations.push_back({'h', 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
        }
        return operations;
    }

    static std::uint32_t arrowColor(const PDFStyle& style) {
        return pdfgen::PDF_IS_TRANSPARENT(style.stroke) ? style.fill : style.stroke;
    }

    static float arrowAlpha(const PDFStyle& style) {
        return pdfgen::PDF_IS_TRANSPARENT(style.stroke) ? style.fillAlpha : style.strokeAlpha;
    }

    void addArrowhead(
        pdfgen::pdf_doc* pdf,
        pdfgen::pdf_object* page,
        float startX,
        float startY,
        float endX,
        float endY,
        const PDFStyle& style) const {
        const std::uint32_t colour = arrowColor(style);
        if (pdfgen::PDF_IS_TRANSPARENT(colour)) {
            return;
        }

        const float dx = endX - startX;
        const float dy = endY - startY;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length == 0.0f) {
            return;
        }

        const float ux = dx / length;
        const float uy = dy / length;
        const float px = -uy;
        const float py = ux;
        const float size = std::max(6.0f, style.strokeWidth * 4.0f);
        const float midX = (startX + endX) / 2.0f;
        const float midY = (startY + endY) / 2.0f;
        const float tipX = midX + ux * size * 0.6f;
        const float tipY = midY + uy * size * 0.6f;
        const float baseCenterX = midX - ux * size * 0.4f;
        const float baseCenterY = midY - uy * size * 0.4f;
        const float halfBase = size * 0.35f;

        const float xs[] = {
            tipX,
            baseCenterX + px * halfBase,
            baseCenterX - px * halfBase,
        };
        const float ys[] = {
            tipY,
            baseCenterY + py * halfBase,
            baseCenterY - py * halfBase,
        };
        if (pdfgen::pdf_add_filled_polygon(pdf, page, xs, ys, 3, 0.0f, colour, arrowAlpha(style), 1.0f) < 0) {
            throwPDFError(pdf, "draw PDF arrowhead");
        }
    }

    void addPath(
        pdfgen::pdf_doc* pdf,
        pdfgen::pdf_object* page,
        const std::vector<std::pair<float, float>>& points,
        bool closePath,
        const PDFStyle& style,
        std::uint32_t fillOverride = pdfgen::PDF_TRANSPARENT) const {
        if (points.empty()) {
            return;
        }
        const auto operations = polygonPathOperations(points, closePath);
        const std::uint32_t fillColour = fillOverride == pdfgen::PDF_TRANSPARENT ? style.fill : fillOverride;
        if (pdfgen::pdf_add_custom_path(
                pdf,
                page,
                operations.data(),
                static_cast<int>(operations.size()),
                style.strokeWidth,
                style.stroke,
                fillColour,
                style.fillAlpha,
                style.strokeAlpha) < 0) {
            throwPDFError(pdf, "draw PDF path");
        }
    }

    void appendElementToPDF(
        pdfgen::pdf_doc* pdf,
        pdfgen::pdf_object* page,
        const Element& element,
        const Viewport& viewport) const {
        using PT = Point<double>;
        const PDFStyle style = pdfStyleOf(element.style);

        std::visit([&](const auto& shape) {
            using S = std::decay_t<decltype(shape)>;

            if constexpr (std::same_as<S, PT>) {
                const auto [x, y] = mapPDFPoint(shape, viewport);
                if (pdfgen::pdf_add_circle(
                        pdf,
                        page,
                        x,
                        y,
                        style.pointRadius,
                        style.strokeWidth,
                        style.stroke,
                        style.fill,
                        style.fillAlpha,
                        style.strokeAlpha) < 0) {
                    throwPDFError(pdf, "draw PDF point");
                }
            } else if constexpr (std::same_as<S, Segment<PT>>) {
                const auto [x1, y1] = mapPDFPoint(shape.min(), viewport);
                const auto [x2, y2] = mapPDFPoint(shape.max(), viewport);
                if (pdfgen::pdf_add_line(pdf, page, x1, y1, x2, y2, style.strokeWidth, style.stroke, style.strokeAlpha) < 0) {
                    throwPDFError(pdf, "draw PDF segment");
                }
            } else if constexpr (std::same_as<S, OrientedSegment<PT>>) {
                const auto [x1, y1] = mapPDFPoint(shape.source(), viewport);
                const auto [x2, y2] = mapPDFPoint(shape.target(), viewport);
                if (pdfgen::pdf_add_line(pdf, page, x1, y1, x2, y2, style.strokeWidth, style.stroke, style.strokeAlpha) < 0) {
                    throwPDFError(pdf, "draw PDF oriented segment");
                }
                // PDF export approximates SVG marker-mid arrowheads with a small
                // filled triangle centered on the visible segment midpoint.
                addArrowhead(pdf, page, x1, y1, x2, y2, style);
            } else if constexpr (std::same_as<S, Line<PT>>) {
                const double x1 = viewport.mapX(shape.min().x());
                const double y1 = viewport.mapY(shape.min().y());
                const double x2 = viewport.mapX(shape.max().x());
                const double y2 = viewport.mapY(shape.max().y());
                const auto visible = clipInfiniteLineToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (!visible) return;
                const auto& [vx1, vy1, vx2, vy2] = *visible;
                if (vx1 == vx2 && vy1 == vy2) {
                    if (pdfgen::pdf_add_circle(
                            pdf,
                            page,
                            static_cast<float>(vx1),
                            pdfYFromSVG(vy1),
                            style.pointRadius,
                            style.strokeWidth,
                            style.stroke,
                            style.fill,
                            style.fillAlpha,
                            style.strokeAlpha) < 0) {
                        throwPDFError(pdf, "draw PDF degenerate line");
                    }
                } else if (pdfgen::pdf_add_line(
                               pdf,
                               page,
                               static_cast<float>(vx1),
                               pdfYFromSVG(vy1),
                               static_cast<float>(vx2),
                               pdfYFromSVG(vy2),
                               style.strokeWidth,
                               style.stroke,
                               style.strokeAlpha) < 0) {
                    throwPDFError(pdf, "draw PDF line");
                }
            } else if constexpr (std::same_as<S, OrientedLine<PT>>) {
                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const auto visible = clipInfiniteLineToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (!visible) return;
                const auto& [vx1, vy1, vx2, vy2] = *visible;
                const float px1 = static_cast<float>(vx1);
                const float py1 = pdfYFromSVG(vy1);
                const float px2 = static_cast<float>(vx2);
                const float py2 = pdfYFromSVG(vy2);
                if (pdfgen::pdf_add_line(pdf, page, px1, py1, px2, py2, style.strokeWidth, style.stroke, style.strokeAlpha) < 0) {
                    throwPDFError(pdf, "draw PDF oriented line");
                }
                addArrowhead(pdf, page, px1, py1, px2, py2, style);
            } else if constexpr (std::same_as<S, Ray<PT>>) {
                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const auto visible = clipRayToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (!visible) return;
                const auto& [vx1, vy1, vx2, vy2] = *visible;
                const float px1 = static_cast<float>(vx1);
                const float py1 = pdfYFromSVG(vy1);
                const float px2 = static_cast<float>(vx2);
                const float py2 = pdfYFromSVG(vy2);
                if (pdfgen::pdf_add_line(pdf, page, px1, py1, px2, py2, style.strokeWidth, style.stroke, style.strokeAlpha) < 0) {
                    throwPDFError(pdf, "draw PDF ray");
                }
                addArrowhead(pdf, page, px1, py1, px2, py2, style);
            } else if constexpr (std::same_as<S, Halfplane<PT>>) {
                const auto polygon = clipHalfplaneToViewport(shape, viewport, widthPixels_, heightPixels_);
                if (!polygon.empty()) {
                    std::vector<std::pair<float, float>> pdfPoints;
                    pdfPoints.reserve(polygon.size());
                    for (const auto& [worldX, worldY] : polygon) {
                        pdfPoints.emplace_back(
                            static_cast<float>(viewport.mapX(worldX)),
                            pdfYFromSVG(viewport.mapY(worldY))
                        );
                    }
                    addPath(
                        pdf,
                        page,
                        pdfPoints,
                        true,
                        PDFStyle{pdfgen::PDF_TRANSPARENT, style.fill, 0.0f, style.pointRadius, style.fillAlpha, 1.0f},
                        style.fill
                    );
                }

                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const auto visible = clipInfiniteLineToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (visible && pdfgen::pdf_add_line(
                        pdf,
                        page,
                        static_cast<float>(visible->x1),
                        pdfYFromSVG(visible->y1),
                        static_cast<float>(visible->x2),
                        pdfYFromSVG(visible->y2),
                        style.strokeWidth,
                        style.stroke,
                        style.strokeAlpha) < 0) {
                    throwPDFError(pdf, "draw PDF halfplane boundary");
                }
            } else if constexpr (std::same_as<S, HalfplaneIntersection<PT>>) {
                const auto polygon = clipRegionToViewport(shape, viewport, widthPixels_, heightPixels_);
                if (!polygon.empty()) {
                    std::vector<std::pair<float, float>> pdfPoints;
                    pdfPoints.reserve(polygon.size());
                    for (const auto& [worldX, worldY] : polygon) {
                        pdfPoints.emplace_back(
                            static_cast<float>(viewport.mapX(worldX)),
                            pdfYFromSVG(viewport.mapY(worldY))
                        );
                    }
                    addPath(
                        pdf,
                        page,
                        pdfPoints,
                        true,
                        PDFStyle{pdfgen::PDF_TRANSPARENT, style.fill, 0.0f, style.pointRadius, style.fillAlpha, 1.0f},
                        style.fill
                    );
                }

                // Stroke only the region's real boundary edges: the viewport
                // sides of the clipped polygon are not part of the boundary.
                for (const auto& piece : regionBoundaryPieces(shape, viewport)) {
                    if (pdfgen::pdf_add_line(
                            pdf,
                            page,
                            static_cast<float>(piece.x1),
                            pdfYFromSVG(piece.y1),
                            static_cast<float>(piece.x2),
                            pdfYFromSVG(piece.y2),
                            style.strokeWidth,
                            style.stroke,
                            style.strokeAlpha) < 0) {
                        throwPDFError(pdf, "draw PDF half-plane intersection boundary");
                    }
                }
            } else if constexpr (std::same_as<S, Rectangle<PT>>) {
                const float x = static_cast<float>(viewport.mapX(shape.min().x()));
                const float y = pdfYFromSVG(viewport.mapY(shape.min().y()));
                const float width = static_cast<float>(std::abs(viewport.mapX(shape.max().x()) - viewport.mapX(shape.min().x())));
                const float height = std::abs(pdfYFromSVG(viewport.mapY(shape.max().y())) - pdfYFromSVG(viewport.mapY(shape.min().y())));
                if (pdfgen::pdf_add_filled_rectangle(
                        pdf,
                        page,
                        x,
                        y,
                        width,
                        height,
                        style.strokeWidth,
                        style.fill,
                        style.stroke,
                        style.fillAlpha,
                        style.strokeAlpha) < 0) {
                    throwPDFError(pdf, "draw PDF rectangle");
                }
            } else if constexpr (std::same_as<S, Triangle<PT>>) {
                addPath(pdf, page, {mapPDFPoint(shape.a(), viewport), mapPDFPoint(shape.b(), viewport), mapPDFPoint(shape.c(), viewport)}, true, style);
            } else if constexpr (std::same_as<S, Convex<PT>>) {
                if (shape.size() == 0) return;
                std::vector<std::pair<float, float>> points;
                points.reserve(shape.size());
                for (const auto& vertex : shape) {
                    points.push_back(mapPDFPoint(vertex, viewport));
                }
                addPath(pdf, page, points, true, style);
            } else if constexpr (std::same_as<S, Polygon<PT>>) {
                if (shape.size() == 0) return;
                std::vector<std::pair<float, float>> points;
                points.reserve(shape.size());
                for (const auto& vertex : shape) {
                    points.push_back(mapPDFPoint(vertex, viewport));
                }
                addPath(pdf, page, points, true, style);
            } else if constexpr (std::same_as<S, MonotoneChain<PT>> || std::same_as<S, Polyline<PT>>) {
                if (shape.size() == 0) return;
                if (shape.size() == 1) {
                    const auto [x, y] = mapPDFPoint(shape[0], viewport);
                    if (pdfgen::pdf_add_circle(
                            pdf,
                            page,
                            x,
                            y,
                            style.pointRadius,
                            style.strokeWidth,
                            style.stroke,
                            style.fill,
                            style.fillAlpha,
                            style.strokeAlpha) < 0) {
                        throwPDFError(pdf, "draw PDF chain point");
                    }
                } else {
                    std::vector<std::pair<float, float>> points;
                    points.reserve(shape.size());
                    for (const auto& vertex : shape) {
                        points.push_back(mapPDFPoint(vertex, viewport));
                    }
                    addPath(
                        pdf,
                        page,
                        points,
                        false,
                        PDFStyle{style.stroke, pdfgen::PDF_TRANSPARENT, style.strokeWidth, style.pointRadius, 1.0f, style.strokeAlpha}
                    );
                }
            } else if constexpr (std::same_as<S, Disk<PT>>) {
                const auto [x, y] = mapPDFPoint(shape.center(), viewport);
                const float radius = static_cast<float>(shape.radius() * viewport.scale);
                if (pdfgen::pdf_add_circle(
                        pdf,
                        page,
                        x,
                        y,
                        radius,
                        style.strokeWidth,
                        style.stroke,
                        style.fill,
                        style.fillAlpha,
                        style.strokeAlpha) < 0) {
                    throwPDFError(pdf, "draw PDF disk");
                }
            }
        }, element.shape.variant());
    }

    std::string elementToSVG(const Element& element, const Viewport& viewport) const {
        using PT = Point<double>;
        const std::string titleTag = "<title>" + escapeXML(element.title, false) + "</title>";

        return std::visit([&](const auto& shape) -> std::string {
            using S = std::decay_t<decltype(shape)>;
            std::ostringstream out;

            if constexpr (std::same_as<S, PT>) {
                const double cx = viewport.mapX(shape.x());
                const double cy = viewport.mapY(shape.y());
                out << "<circle cx=\"" << cx << "\" cy=\"" << cy
                    << "\" r=\"" << element.style.pointRadius << '"'
                    << styleAttributes(element.style) << ">"
                    << titleTag << "</circle>";
            } else if constexpr (std::same_as<S, Segment<PT>>) {
                const double x1 = viewport.mapX(shape.min().x());
                const double y1 = viewport.mapY(shape.min().y());
                const double x2 = viewport.mapX(shape.max().x());
                const double y2 = viewport.mapY(shape.max().y());
                out << "<line x1=\"" << x1 << "\" y1=\"" << y1
                    << "\" x2=\"" << x2 << "\" y2=\"" << y2 << "\""
                    << styleAttributes(element.style) << ">"
                    << titleTag << "</line>";
            } else if constexpr (std::same_as<S, OrientedSegment<PT>>) {
                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const double midX = viewport.mapX((shape.source().x() + shape.target().x()) / 2.0);
                const double midY = viewport.mapY((shape.source().y() + shape.target().y()) / 2.0);
                out << "<path d=\"M " << x1 << ' ' << y1
                    << " L " << midX << ' ' << midY
                    << " L " << x2 << ' ' << y2 << "\""
                    << styleAttributes(element.style)
                    << " marker-mid=\"url(#pgl-arrowhead)\">"
                    << titleTag << "</path>";
            } else if constexpr (std::same_as<S, Line<PT>>) {
                const double x1 = viewport.mapX(shape.min().x());
                const double y1 = viewport.mapY(shape.min().y());
                const double x2 = viewport.mapX(shape.max().x());
                const double y2 = viewport.mapY(shape.max().y());
                const auto visible = clipInfiniteLineToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (!visible) return {};
                const auto& [vx1, vy1, vx2, vy2] = *visible;
                if (vx1 == vx2 && vy1 == vy2) {
                    out << "<circle cx=\"" << vx1 << "\" cy=\"" << vy1
                        << "\" r=\"" << element.style.pointRadius << '"'
                        << styleAttributes(element.style) << ">"
                        << titleTag << "</circle>";
                } else {
                    out << "<line x1=\"" << vx1 << "\" y1=\"" << vy1
                        << "\" x2=\"" << vx2 << "\" y2=\"" << vy2 << "\""
                        << styleAttributes(element.style) << ">"
                        << titleTag << "</line>";
                }
            } else if constexpr (std::same_as<S, OrientedLine<PT>>) {
                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const auto visible = clipInfiniteLineToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (!visible) return {};
                const auto& [vx1, vy1, vx2, vy2] = *visible;
                const double midX = (vx1 + vx2) / 2.0;
                const double midY = (vy1 + vy2) / 2.0;
                out << "<path d=\"M " << vx1 << ' ' << vy1
                    << " L " << midX << ' ' << midY
                    << " L " << vx2 << ' ' << vy2 << "\""
                    << styleAttributes(element.style)
                    << " marker-mid=\"url(#pgl-arrowhead)\">"
                    << titleTag << "</path>";
            } else if constexpr (std::same_as<S, Ray<PT>>) {
                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const auto visible = clipRayToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (!visible) return {};
                const auto& [vx1, vy1, vx2, vy2] = *visible;
                const double midX = (vx1 + vx2) / 2.0;
                const double midY = (vy1 + vy2) / 2.0;
                out << "<path d=\"M " << vx1 << ' ' << vy1
                    << " L " << midX << ' ' << midY
                    << " L " << vx2 << ' ' << vy2 << "\""
                    << styleAttributes(element.style)
                    << " marker-mid=\"url(#pgl-arrowhead)\">"
                    << titleTag << "</path>";
            } else if constexpr (std::same_as<S, Halfplane<PT>>) {
                const auto polygon = clipHalfplaneToViewport(shape, viewport, widthPixels_, heightPixels_);
                if (polygon.empty()) return {};

                out << "<g><polygon points=\"";
                for (std::size_t index = 0; index < polygon.size(); ++index) {
                    if (index != 0) out << ' ';
                    out << viewport.mapX(polygon[index].first) << ',' << viewport.mapY(polygon[index].second);
                }
                out << "\" stroke=\"none\"" << fillAttributes(element.style) << ">"
                    << titleTag << "</polygon>";

                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const auto visible = clipInfiniteLineToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (visible) {
                    const auto& [vx1, vy1, vx2, vy2] = *visible;
                    out << "<line x1=\"" << vx1 << "\" y1=\"" << vy1
                        << "\" x2=\"" << vx2 << "\" y2=\"" << vy2 << "\""
                        << styleAttributes(element.style) << ">"
                        << titleTag << "</line>";
                }
                out << "</g>";
            } else if constexpr (std::same_as<S, HalfplaneIntersection<PT>>) {
                const auto polygon = clipRegionToViewport(shape, viewport, widthPixels_, heightPixels_);
                const auto boundary = regionBoundaryPieces(shape, viewport);
                if (polygon.empty() && boundary.empty()) return {};

                out << "<g>";
                if (!polygon.empty()) {
                    out << "<polygon points=\"";
                    for (std::size_t index = 0; index < polygon.size(); ++index) {
                        if (index != 0) out << ' ';
                        out << viewport.mapX(polygon[index].first) << ',' << viewport.mapY(polygon[index].second);
                    }
                    out << "\" stroke=\"none\"" << fillAttributes(element.style) << ">"
                        << titleTag << "</polygon>";
                }
                // Stroke only the region's real boundary edges: the viewport
                // sides of the clipped polygon are not part of the boundary.
                for (const auto& piece : boundary) {
                    out << "<line x1=\"" << piece.x1 << "\" y1=\"" << piece.y1
                        << "\" x2=\"" << piece.x2 << "\" y2=\"" << piece.y2 << "\""
                        << styleAttributes(element.style) << ">"
                        << titleTag << "</line>";
                }
                out << "</g>";
            } else if constexpr (std::same_as<S, Rectangle<PT>>) {
                const double minX = viewport.mapX(shape.min().x());
                const double maxX = viewport.mapX(shape.max().x());
                const double minY = viewport.mapY(shape.max().y());
                const double maxY = viewport.mapY(shape.min().y());
                out << "<rect x=\"" << std::min(minX, maxX) << "\" y=\"" << std::min(minY, maxY)
                    << "\" width=\"" << std::abs(maxX - minX) << "\" height=\"" << std::abs(maxY - minY) << "\""
                    << styleAttributes(element.style) << ">"
                    << titleTag << "</rect>";
            } else if constexpr (std::same_as<S, Triangle<PT>>) {
                out << "<polygon points=\""
                    << viewport.mapX(shape.a().x()) << ',' << viewport.mapY(shape.a().y()) << ' '
                    << viewport.mapX(shape.b().x()) << ',' << viewport.mapY(shape.b().y()) << ' '
                    << viewport.mapX(shape.c().x()) << ',' << viewport.mapY(shape.c().y()) << "\""
                    << styleAttributes(element.style) << ">"
                    << titleTag << "</polygon>";
            } else if constexpr (std::same_as<S, Convex<PT>>) {
                if (shape.size() == 0) return {};
                out << "<polygon points=\"";
                bool firstVertex = true;
                for (const auto& vertex : shape) {
                    if (!firstVertex) out << ' ';
                    firstVertex = false;
                    out << viewport.mapX(vertex.x()) << ',' << viewport.mapY(vertex.y());
                }
                out << "\""
                    << styleAttributes(element.style) << ">"
                    << titleTag << "</polygon>";
            } else if constexpr (std::same_as<S, Polygon<PT>>) {
                if (shape.size() == 0) return {};
                out << "<polygon points=\"";
                bool firstVertex = true;
                for (const auto& vertex : shape) {
                    if (!firstVertex) out << ' ';
                    firstVertex = false;
                    out << viewport.mapX(vertex.x()) << ',' << viewport.mapY(vertex.y());
                }
                out << "\""
                    << styleAttributes(element.style) << ">"
                    << titleTag << "</polygon>";
            } else if constexpr (std::same_as<S, MonotoneChain<PT>> || std::same_as<S, Polyline<PT>>) {
                if (shape.size() == 0) return {};
                if (shape.size() == 1) {
                    const auto vertex = shape[0];
                    out << "<circle cx=\"" << viewport.mapX(vertex.x())
                        << "\" cy=\"" << viewport.mapY(vertex.y())
                        << "\" r=\"" << element.style.pointRadius << '"'
                        << styleAttributes(element.style) << ">"
                        << titleTag << "</circle>";
                } else {
                    // An open chain: a polyline, never closed or filled.
                    out << "<polyline points=\"";
                    bool firstVertex = true;
                    for (const auto& vertex : shape) {
                        if (!firstVertex) out << ' ';
                        firstVertex = false;
                        out << viewport.mapX(vertex.x()) << ',' << viewport.mapY(vertex.y());
                    }
                    out << "\""
                        << styleAttributes(element.style) << ">"
                        << titleTag << "</polyline>";
                }
            } else if constexpr (std::same_as<S, Disk<PT>>) {
                const auto center = shape.center();
                const double cx = viewport.mapX(center.x());
                const double cy = viewport.mapY(center.y());
                const double r = shape.radius() * viewport.scale;
                out << "<circle cx=\"" << cx << "\" cy=\"" << cy
                    << "\" r=\"" << r << '"'
                    << styleAttributes(element.style) << ">"
                    << titleTag << "</circle>";
            }
            return out.str();
        }, element.shape.variant());
    }

    // ---------------------------------------------------------------
    // Ipe (.ipe) export
    // ---------------------------------------------------------------

    // Ipe's `opacity`/`stroke-opacity` path attributes must reference a
    // symbolic name defined in the style sheet, so distinct opacity values
    // (rounded to three decimal places) are collected up front and declared
    // as `<opacity>` entries named after their rounded per-mille value.
    std::vector<int> collectIPEOpacities() const {
        std::vector<int> keys;
        for (const Element& element : elements_) {
            const PDFStyle style = pdfStyleOf(element.style);
            if (!pdfgen::PDF_IS_TRANSPARENT(style.stroke) && style.strokeAlpha < 1.0f) {
                keys.push_back(static_cast<int>(std::lround(style.strokeAlpha * 1000.0f)));
            }
            if (!pdfgen::PDF_IS_TRANSPARENT(style.fill) && style.fillAlpha < 1.0f) {
                keys.push_back(static_cast<int>(std::lround(style.fillAlpha * 1000.0f)));
            }
        }
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        return keys;
    }

    static std::string ipeOpacityName(int perMille) {
        return "op" + std::to_string(perMille);
    }

    static std::string ipeColorTriplet(std::uint32_t colour) {
        std::ostringstream out;
        out << pdfgen::PDF_RGB_R(colour) << ' ' << pdfgen::PDF_RGB_G(colour) << ' ' << pdfgen::PDF_RGB_B(colour);
        return out.str();
    }

    static std::string ipeStrokeAttributes(const PDFStyle& style) {
        std::ostringstream out;
        if (!pdfgen::PDF_IS_TRANSPARENT(style.stroke)) {
            out << " stroke=\"" << ipeColorTriplet(style.stroke) << '"';
            if (style.strokeAlpha < 1.0f) {
                out << " stroke-opacity=\"" << ipeOpacityName(static_cast<int>(std::lround(style.strokeAlpha * 1000.0f))) << '"';
            }
            if (style.strokeWidth > 0.0f) {
                out << " pen=\"" << style.strokeWidth << '"';
            }
        }
        return out.str();
    }

    static std::string ipeFillAttributes(const PDFStyle& style) {
        std::ostringstream out;
        if (!pdfgen::PDF_IS_TRANSPARENT(style.fill)) {
            out << " fill=\"" << ipeColorTriplet(style.fill) << '"';
            if (style.fillAlpha < 1.0f) {
                out << " opacity=\"" << ipeOpacityName(static_cast<int>(std::lround(style.fillAlpha * 1000.0f))) << '"';
            }
        }
        return out.str();
    }

    static std::string ipeStyleAttributes(const PDFStyle& style) {
        return ipeStrokeAttributes(style) + ipeFillAttributes(style);
    }

    template <class PointLike>
    std::pair<double, double> mapIPEPoint(const PointLike& point, const Viewport& viewport) const {
        return {viewport.mapX(point.x()), pdfYFromSVG(viewport.mapY(point.y()))};
    }

    static void appendIPEPath(
        std::ostringstream& out,
        const std::vector<std::pair<double, double>>& points,
        bool closePath,
        const std::string& attributes) {
        if (points.empty() || attributes.empty()) return;
        out << "<path" << attributes << ">\n"
            << points[0].first << ' ' << points[0].second << " m\n";
        for (std::size_t index = 1; index < points.size(); ++index) {
            out << points[index].first << ' ' << points[index].second << " l\n";
        }
        if (closePath) out << "h\n";
        out << "</path>\n";
    }

    static void appendIPEEllipse(
        std::ostringstream& out,
        double cx,
        double cy,
        double radius,
        const std::string& attributes) {
        if (attributes.empty()) return;
        // Ipe draws an ellipse/circle by applying an affine matrix (a b c d e f)
        // to the unit circle; a circle of the given radius uses "r 0 0 r cx cy".
        out << "<path" << attributes << ">\n"
            << radius << " 0 0 " << radius << ' ' << cx << ' ' << cy << " e\n"
            << "</path>\n";
    }

    // Mirrors the arrowhead triangle used for PDF export (addArrowhead) so
    // that OrientedSegment/OrientedLine/Ray render consistently across
    // SVG's marker-mid, the PDF triangle, and this Ipe triangle.
    static std::array<std::pair<double, double>, 3> ipeArrowTriangle(
        double startX, double startY, double endX, double endY, double strokeWidth) {
        const double dx = endX - startX;
        const double dy = endY - startY;
        const double length = std::sqrt(dx * dx + dy * dy);
        if (length == 0.0) {
            return {{{startX, startY}, {startX, startY}, {startX, startY}}};
        }

        const double ux = dx / length;
        const double uy = dy / length;
        const double px = -uy;
        const double py = ux;
        const double size = std::max(6.0, strokeWidth * 4.0);
        const double midX = (startX + endX) / 2.0;
        const double midY = (startY + endY) / 2.0;
        const double tipX = midX + ux * size * 0.6;
        const double tipY = midY + uy * size * 0.6;
        const double baseCenterX = midX - ux * size * 0.4;
        const double baseCenterY = midY - uy * size * 0.4;
        const double halfBase = size * 0.35;

        return {{
            {tipX, tipY},
            {baseCenterX + px * halfBase, baseCenterY + py * halfBase},
            {baseCenterX - px * halfBase, baseCenterY - py * halfBase},
        }};
    }

    void appendIPEArrowhead(
        std::ostringstream& out,
        double startX, double startY, double endX, double endY,
        const PDFStyle& style) const {
        const std::uint32_t colour = arrowColor(style);
        if (pdfgen::PDF_IS_TRANSPARENT(colour)) return;

        const auto triangle = ipeArrowTriangle(startX, startY, endX, endY, style.strokeWidth);
        if (triangle[0] == triangle[1]) return;

        std::ostringstream attrs;
        attrs << " fill=\"" << ipeColorTriplet(colour) << '"';
        const float alpha = arrowAlpha(style);
        if (alpha < 1.0f) {
            attrs << " opacity=\"" << ipeOpacityName(static_cast<int>(std::lround(alpha * 1000.0f))) << '"';
        }
        appendIPEPath(out, {triangle[0], triangle[1], triangle[2]}, true, attrs.str());
    }

    void appendIPEBorder(std::ostringstream& out) const {
        const double x1 = 0.5;
        const double y1 = 0.5;
        const double x2 = std::max(widthPixels_ - 0.5, 0.0);
        const double y2 = std::max(heightPixels_ - 0.5, 0.0);
        appendIPEPath(out, {{x1, y1}, {x2, y1}, {x2, y2}, {x1, y2}}, true, " stroke=\"0 0 0\" pen=\"1\"");
    }

    void appendElementToIPE(std::ostringstream& out, const Element& element, const Viewport& viewport) const {
        using PT = Point<double>;
        const PDFStyle style = pdfStyleOf(element.style);
        const std::string attrs = ipeStyleAttributes(style);

        std::visit([&](const auto& shape) {
            using S = std::decay_t<decltype(shape)>;

            if constexpr (std::same_as<S, PT>) {
                const auto [cx, cy] = mapIPEPoint(shape, viewport);
                appendIPEEllipse(out, cx, cy, style.pointRadius, attrs);
            } else if constexpr (std::same_as<S, Segment<PT>>) {
                appendIPEPath(out, {mapIPEPoint(shape.min(), viewport), mapIPEPoint(shape.max(), viewport)}, false, attrs);
            } else if constexpr (std::same_as<S, OrientedSegment<PT>>) {
                const auto p1 = mapIPEPoint(shape.source(), viewport);
                const auto p2 = mapIPEPoint(shape.target(), viewport);
                appendIPEPath(out, {p1, p2}, false, attrs);
                appendIPEArrowhead(out, p1.first, p1.second, p2.first, p2.second, style);
            } else if constexpr (std::same_as<S, Line<PT>>) {
                const double x1 = viewport.mapX(shape.min().x());
                const double y1 = viewport.mapY(shape.min().y());
                const double x2 = viewport.mapX(shape.max().x());
                const double y2 = viewport.mapY(shape.max().y());
                const auto visible = clipInfiniteLineToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (!visible) return;
                const auto& [vx1, vy1, vx2, vy2] = *visible;
                if (vx1 == vx2 && vy1 == vy2) {
                    appendIPEEllipse(out, vx1, pdfYFromSVG(vy1), style.pointRadius, attrs);
                } else {
                    appendIPEPath(out, {{vx1, pdfYFromSVG(vy1)}, {vx2, pdfYFromSVG(vy2)}}, false, attrs);
                }
            } else if constexpr (std::same_as<S, OrientedLine<PT>>) {
                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const auto visible = clipInfiniteLineToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (!visible) return;
                const auto& [vx1, vy1, vx2, vy2] = *visible;
                const double p1x = vx1;
                const double p1y = pdfYFromSVG(vy1);
                const double p2x = vx2;
                const double p2y = pdfYFromSVG(vy2);
                appendIPEPath(out, {{p1x, p1y}, {p2x, p2y}}, false, attrs);
                appendIPEArrowhead(out, p1x, p1y, p2x, p2y, style);
            } else if constexpr (std::same_as<S, Ray<PT>>) {
                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const auto visible = clipRayToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (!visible) return;
                const auto& [vx1, vy1, vx2, vy2] = *visible;
                const double p1x = vx1;
                const double p1y = pdfYFromSVG(vy1);
                const double p2x = vx2;
                const double p2y = pdfYFromSVG(vy2);
                appendIPEPath(out, {{p1x, p1y}, {p2x, p2y}}, false, attrs);
                appendIPEArrowhead(out, p1x, p1y, p2x, p2y, style);
            } else if constexpr (std::same_as<S, Halfplane<PT>>) {
                const auto polygon = clipHalfplaneToViewport(shape, viewport, widthPixels_, heightPixels_);
                if (!polygon.empty()) {
                    std::vector<std::pair<double, double>> points;
                    points.reserve(polygon.size());
                    for (const auto& [worldX, worldY] : polygon) {
                        points.emplace_back(viewport.mapX(worldX), pdfYFromSVG(viewport.mapY(worldY)));
                    }
                    appendIPEPath(out, points, true, ipeFillAttributes(style));
                }

                const double x1 = viewport.mapX(shape.source().x());
                const double y1 = viewport.mapY(shape.source().y());
                const double x2 = viewport.mapX(shape.target().x());
                const double y2 = viewport.mapY(shape.target().y());
                const auto visible = clipInfiniteLineToBox(x1, y1, x2, y2, 0.0, 0.0, widthPixels_, heightPixels_);
                if (visible) {
                    appendIPEPath(
                        out,
                        {{visible->x1, pdfYFromSVG(visible->y1)}, {visible->x2, pdfYFromSVG(visible->y2)}},
                        false,
                        ipeStrokeAttributes(style));
                }
            } else if constexpr (std::same_as<S, HalfplaneIntersection<PT>>) {
                const auto polygon = clipRegionToViewport(shape, viewport, widthPixels_, heightPixels_);
                if (!polygon.empty()) {
                    std::vector<std::pair<double, double>> points;
                    points.reserve(polygon.size());
                    for (const auto& [worldX, worldY] : polygon) {
                        points.emplace_back(viewport.mapX(worldX), pdfYFromSVG(viewport.mapY(worldY)));
                    }
                    appendIPEPath(out, points, true, ipeFillAttributes(style));
                }

                // Stroke only the region's real boundary edges: the viewport
                // sides of the clipped polygon are not part of the boundary.
                for (const auto& piece : regionBoundaryPieces(shape, viewport)) {
                    appendIPEPath(
                        out,
                        {{piece.x1, pdfYFromSVG(piece.y1)}, {piece.x2, pdfYFromSVG(piece.y2)}},
                        false,
                        ipeStrokeAttributes(style));
                }
            } else if constexpr (std::same_as<S, Rectangle<PT>>) {
                const double minX = viewport.mapX(shape.min().x());
                const double maxX = viewport.mapX(shape.max().x());
                const double minY = pdfYFromSVG(viewport.mapY(shape.max().y()));
                const double maxY = pdfYFromSVG(viewport.mapY(shape.min().y()));
                appendIPEPath(out, {{minX, minY}, {maxX, minY}, {maxX, maxY}, {minX, maxY}}, true, attrs);
            } else if constexpr (std::same_as<S, Triangle<PT>>) {
                appendIPEPath(
                    out,
                    {mapIPEPoint(shape.a(), viewport), mapIPEPoint(shape.b(), viewport), mapIPEPoint(shape.c(), viewport)},
                    true,
                    attrs);
            } else if constexpr (std::same_as<S, Convex<PT>> || std::same_as<S, Polygon<PT>>) {
                if (shape.size() == 0) return;
                std::vector<std::pair<double, double>> points;
                points.reserve(shape.size());
                for (const auto& vertex : shape) {
                    points.push_back(mapIPEPoint(vertex, viewport));
                }
                appendIPEPath(out, points, true, attrs);
            } else if constexpr (std::same_as<S, MonotoneChain<PT>> || std::same_as<S, Polyline<PT>>) {
                if (shape.size() == 0) return;
                if (shape.size() == 1) {
                    const auto [cx, cy] = mapIPEPoint(shape[0], viewport);
                    appendIPEEllipse(out, cx, cy, style.pointRadius, attrs);
                } else {
                    std::vector<std::pair<double, double>> points;
                    points.reserve(shape.size());
                    for (const auto& vertex : shape) {
                        points.push_back(mapIPEPoint(vertex, viewport));
                    }
                    // An open chain: a polyline, never closed or filled.
                    appendIPEPath(out, points, false, ipeStrokeAttributes(style));
                }
            } else if constexpr (std::same_as<S, Disk<PT>>) {
                const auto center = mapIPEPoint(shape.center(), viewport);
                const double radius = shape.radius() * viewport.scale;
                appendIPEEllipse(out, center.first, center.second, radius, attrs);
            }
        }, element.shape.variant());
    }

    CanvasStyle style_{};
    std::vector<Element> elements_{};
    double zoom_ = 1.0;
    double widthPixels_ = 1000.0;
    double heightPixels_ = 1000.0;
    double marginPixels_ = 20.0;
    bool drawBorder_ = false;

    static constexpr double borderInset_ = 20.0;

    struct ClippedSegment {
        double x1;
        double y1;
        double x2;
        double y2;
    };

    static std::optional<ClippedSegment> clipInfiniteLineToBox(
        double x1,
        double y1,
        double x2,
        double y2,
        double minX,
        double minY,
        double maxX,
        double maxY) {
        const double dx = x2 - x1;
        const double dy = y2 - y1;

        if (dx == 0.0 && dy == 0.0) {
            if (x1 < minX || maxX < x1 || y1 < minY || maxY < y1) {
                return std::nullopt;
            }
            return ClippedSegment{x1, y1, x1, y1};
        }

        std::vector<std::pair<double, double>> intersections;
        const auto append_if_inside = [&intersections, minX, minY, maxX, maxY](double x, double y) {
            const double epsilon = 1e-9;
            if (x + epsilon < minX || maxX + epsilon < x || y + epsilon < minY || maxY + epsilon < y) {
                return;
            }

            for (const auto& [existing_x, existing_y] : intersections) {
                if (std::abs(existing_x - x) <= epsilon && std::abs(existing_y - y) <= epsilon) {
                    return;
                }
            }

            intersections.emplace_back(x, y);
        };

        if (dx != 0.0) {
            const double t_left = (minX - x1) / dx;
            append_if_inside(minX, y1 + t_left * dy);

            const double t_right = (maxX - x1) / dx;
            append_if_inside(maxX, y1 + t_right * dy);
        }

        if (dy != 0.0) {
            const double t_top = (minY - y1) / dy;
            append_if_inside(x1 + t_top * dx, minY);

            const double t_bottom = (maxY - y1) / dy;
            append_if_inside(x1 + t_bottom * dx, maxY);
        }

        if (intersections.size() < 2) {
            return std::nullopt;
        }

        return ClippedSegment{
            intersections.front().first,
            intersections.front().second,
            intersections.back().first,
            intersections.back().second,
        };
    }

    static std::optional<ClippedSegment> clipRayToBox(
        double x1,
        double y1,
        double x2,
        double y2,
        double minX,
        double minY,
        double maxX,
        double maxY) {
        const double dx = x2 - x1;
        const double dy = y2 - y1;

        if (dx == 0.0 && dy == 0.0) {
            if (x1 < minX || maxX < x1 || y1 < minY || maxY < y1) {
                return std::nullopt;
            }
            return ClippedSegment{x1, y1, x1, y1};
        }

        std::vector<std::tuple<double, double, double>> candidates;
        const auto append_if_inside = [&candidates, minX, minY, maxX, maxY](double t, double x, double y) {
            const double epsilon = 1e-9;
            if (t + epsilon < 0.0) {
                return;
            }
            if (x + epsilon < minX || maxX + epsilon < x || y + epsilon < minY || maxY + epsilon < y) {
                return;
            }

            for (const auto& [existingT, existingX, existingY] : candidates) {
                if (std::abs(existingT - t) <= epsilon &&
                    std::abs(existingX - x) <= epsilon &&
                    std::abs(existingY - y) <= epsilon) {
                    return;
                }
            }

            candidates.emplace_back(t, x, y);
        };

        append_if_inside(0.0, x1, y1);

        if (dx != 0.0) {
            const double t_left = (minX - x1) / dx;
            append_if_inside(t_left, minX, y1 + t_left * dy);

            const double t_right = (maxX - x1) / dx;
            append_if_inside(t_right, maxX, y1 + t_right * dy);
        }

        if (dy != 0.0) {
            const double t_top = (minY - y1) / dy;
            append_if_inside(t_top, x1 + t_top * dx, minY);

            const double t_bottom = (maxY - y1) / dy;
            append_if_inside(t_bottom, x1 + t_bottom * dx, maxY);
        }

        if (candidates.empty()) {
            return std::nullopt;
        }

        std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
            return std::get<0>(left) < std::get<0>(right);
        });

        if (candidates.size() == 1) {
            const auto& [t, x, y] = candidates.front();
            (void)t;
            return ClippedSegment{x, y, x, y};
        }

        const auto& [startT, startX, startY] = candidates.front();
        const auto& [endT, endX, endY] = candidates.back();
        (void)startT;
        (void)endT;
        return ClippedSegment{startX, startY, endX, endY};
    }

    // The viewport rectangle as a CCW polygon in world coordinates: the
    // Sutherland–Hodgman seed for clipping half-planes and half-plane
    // intersections to the visible area.
    static std::vector<std::pair<double, double>> viewportPolygon(
        const Viewport& viewport,
        double widthPixels,
        double heightPixels) {
        return {
            {viewport.unmapX(0.0), viewport.unmapY(heightPixels)},
            {viewport.unmapX(widthPixels), viewport.unmapY(heightPixels)},
            {viewport.unmapX(widthPixels), viewport.unmapY(0.0)},
            {viewport.unmapX(0.0), viewport.unmapY(0.0)},
        };
    }

    static std::vector<std::pair<double, double>> clipPolygonToHalfplane(
        const std::vector<std::pair<double, double>>& polygon,
        const Halfplane<Point<double>>& halfplane) {
        const Point<double> first = halfplane.source();
        const Point<double> second = halfplane.target();

        const auto inside = [&first, &second](const std::pair<double, double>& point) {
            return orientationSign(first, second, Point<double>(point.first, point.second)) != std::partial_ordering::less;
        };

        const auto intersection = [&first, &second](const std::pair<double, double>& left, const std::pair<double, double>& right) {
            const double ax = first.x();
            const double ay = first.y();
            const double bx = second.x();
            const double by = second.y();
            const double px = left.first;
            const double py = left.second;
            const double qx = right.first;
            const double qy = right.second;

            const double denominator = (bx - ax) * (qy - py) - (by - ay) * (qx - px);
            if (denominator == 0.0) {
                return left;
            }

            const double numerator = (px - ax) * (qy - py) - (py - ay) * (qx - px);
            const double t = numerator / denominator;
            return std::pair<double, double>{
                ax + t * (bx - ax),
                ay + t * (by - ay),
            };
        };

        std::vector<std::pair<double, double>> output;
        for (std::size_t index = 0; index < polygon.size(); ++index) {
            const auto& current = polygon[index];
            const auto& previous = polygon[(index + polygon.size() - 1) % polygon.size()];
            const bool currentInside = inside(current);
            const bool previousInside = inside(previous);

            if (currentInside) {
                if (!previousInside) {
                    output.push_back(intersection(previous, current));
                }
                output.push_back(current);
            } else if (previousInside) {
                output.push_back(intersection(previous, current));
            }
        }

        return output;
    }

    static std::vector<std::pair<double, double>> clipHalfplaneToViewport(
        const Halfplane<Point<double>>& halfplane,
        const Viewport& viewport,
        double widthPixels,
        double heightPixels) {
        return clipPolygonToHalfplane(viewportPolygon(viewport, widthPixels, heightPixels), halfplane);
    }

    // World-space polygon of the region clipped to the viewport rectangle:
    // Sutherland–Hodgman against every stored half-plane in turn. Empty for
    // the empty region (and for a region that misses the viewport); the whole
    // plane keeps the full viewport rectangle.
    static std::vector<std::pair<double, double>> clipRegionToViewport(
        const HalfplaneIntersection<Point<double>>& region,
        const Viewport& viewport,
        double widthPixels,
        double heightPixels) {
        if (region.isEmpty()) {
            return {};
        }
        std::vector<std::pair<double, double>> polygon = viewportPolygon(viewport, widthPixels, heightPixels);
        for (const auto& halfplane : region) {
            polygon = clipPolygonToHalfplane(polygon, halfplane);
            if (polygon.empty()) {
                break;
            }
        }
        return polygon;
    }

    // The visible portions of the region's boundary edges, mapped to SVG
    // pixel coordinates: segment edges map directly, ray and line edges are
    // clipped to the pixel box like the Ray and Line alternatives.
    std::vector<ClippedSegment> regionBoundaryPieces(
        const HalfplaneIntersection<Point<double>>& region,
        const Viewport& viewport) const {
        std::vector<ClippedSegment> pieces;
        if (region.isEmpty()) {
            return pieces;
        }
        for (std::size_t index = 0; index < region.size(); ++index) {
            std::visit(
                [&](const auto& piece) {
                    using P = std::decay_t<decltype(piece)>;
                    if constexpr (std::same_as<P, Segment<Point<double>>>) {
                        pieces.push_back(ClippedSegment{
                            viewport.mapX(piece.min().x()),
                            viewport.mapY(piece.min().y()),
                            viewport.mapX(piece.max().x()),
                            viewport.mapY(piece.max().y()),
                        });
                    } else if constexpr (std::same_as<P, Ray<Point<double>>>) {
                        const auto visible = clipRayToBox(
                            viewport.mapX(piece.source().x()), viewport.mapY(piece.source().y()),
                            viewport.mapX(piece.target().x()), viewport.mapY(piece.target().y()),
                            0.0, 0.0, widthPixels_, heightPixels_);
                        if (visible) {
                            pieces.push_back(*visible);
                        }
                    } else {
                        const auto visible = clipInfiniteLineToBox(
                            viewport.mapX(piece.min().x()), viewport.mapY(piece.min().y()),
                            viewport.mapX(piece.max().x()), viewport.mapY(piece.max().y()),
                            0.0, 0.0, widthPixels_, heightPixels_);
                        if (visible) {
                            pieces.push_back(*visible);
                        }
                    }
                },
                region.edge(index));
        }
        return pieces;
    }
};

} // namespace pgl
