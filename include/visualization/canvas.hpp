#pragma once

#ifndef PGL_HPP_INCLUDED
#error "Do not include this Pangolin header directly; include \"pgl.hpp\" instead."
#endif

/**
 * @file canvas.hpp
 * @brief Lightweight SVG canvas for drawing Pangolin shapes.
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
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
     * @brief Sets the rendered radius of point primitives.
     *
     * @param radiusPixels Positive radius in pixels.
     * @return This canvas.
     */
    Canvas& pointRadius(double radiusPixels) {
        requireStrictlyPositive(radiusPixels, "point radius");
        pointRadius_ = radiusPixels;
        return *this;
    }

    /**
     * @brief Sets the current stroke width.
     *
     * New elements inserted afterwards capture this width.
     *
     * @param widthPixels Positive stroke width in pixels.
     * @return This canvas.
     */
    Canvas& strokeWidth(double widthPixels) {
        requireStrictlyPositive(widthPixels, "stroke width");
        style_.strokeWidth = toString(widthPixels);
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

        value = std::max(value, marginPixels_ + pointRadius_);
        for (const Element& element : elements_) {
            if (const std::optional<double> width = parseNumericLength(element.style.strokeWidth)) {
                value = std::max(value, marginPixels_ + *width / 2.0);
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

        const double scaleX = contentWidth == 0.0 ? std::numeric_limits<double>::infinity() : drawableWidth / contentWidth;
        const double scaleY = contentHeight == 0.0 ? std::numeric_limits<double>::infinity() : drawableHeight / contentHeight;

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
                    << "\" r=\"" << pointRadius_ << '"'
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
                        << "\" r=\"" << pointRadius_ << '"'
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

    CanvasStyle style_{};
    std::vector<Element> elements_{};
    double zoom_ = 1.0;
    double widthPixels_ = 1000.0;
    double heightPixels_ = 1000.0;
    double pointRadius_ = 3;
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

    static std::vector<std::pair<double, double>> clipHalfplaneToViewport(
        const Halfplane<Point<double>>& halfplane,
        const Viewport& viewport,
        double widthPixels,
        double heightPixels) {
        std::vector<std::pair<double, double>> polygon = {
            {viewport.unmapX(0.0), viewport.unmapY(heightPixels)},
            {viewport.unmapX(widthPixels), viewport.unmapY(heightPixels)},
            {viewport.unmapX(widthPixels), viewport.unmapY(0.0)},
            {viewport.unmapX(0.0), viewport.unmapY(0.0)},
        };

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
};

} // namespace pgl
