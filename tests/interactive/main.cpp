// Interactive Pangolin shape explorer.
//
// Pick a shape type for each of two shapes A and B, draw / drag / translate
// them on a snapping grid, and watch — in real time — whether each shape is
// degenerate, the result of every predicate in both directions
// (A.predicate(B) and B.predicate(A)), and the highlighted A.intersection(B).
//
// All geometry is computed with exact pgl::ERational (Rational<BigInt>)
// coordinates; screen coordinates are the only place doubles appear.

#include "pgl.hpp"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QRegularExpression>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QSplitter>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

// ---------------------------------------------------------------------------
// pgl aliases
// ---------------------------------------------------------------------------
using EPoint = pgl::EPoint;                 // Point<Rational<BigInt>>
using EShape = pgl::Shape<EPoint>;

// ---------------------------------------------------------------------------
// Shape kinds the editor can build
// ---------------------------------------------------------------------------
enum class Kind {
    Point,
    Segment,
    OrientedSegment,
    Line,
    OrientedLine,
    Ray,
    Halfplane,
    Rectangle,
    Triangle,
    Disk,
    Convex,
    MonotoneChain,
    Polyline,
    Polygon,
    Count
};

struct KindInfo {
    const char* name;
    const char* cpp;  // pgl type alias used in copied/pasted C++ code
    int minPts;       // minimum number of control points
    bool variable;    // may hold more than minPts control points
};

static const KindInfo kKinds[] = {
    {"Point", "EPoint", 1, false},
    {"Segment", "ESegment", 2, false},
    {"OrientedSegment", "EOrientedSegment", 2, false},
    {"Line", "ELine", 2, false},
    {"OrientedLine", "EOrientedLine", 2, false},
    {"Ray", "ERay", 2, false},
    {"Halfplane", "EHalfplane", 2, false},
    {"Rectangle", "ERectangle", 2, false},
    {"Triangle", "ETriangle", 3, false},
    {"Disk", "EDisk", 3, false},
    {"Convex", "EConvex", 3, true},
    {"MonotoneChain", "EMonotoneChain", 3, true},
    {"Polyline", "EPolyline", 2, true},
    {"Polygon", "EPolygon", 3, true},
};

static const KindInfo& info(Kind k) { return kKinds[static_cast<int>(k)]; }

// Display colors for shapes A and B (blue / orange), shared by the canvas
// rendering and the active-shape box styling. QColor has no constexpr
// constructor, so the value is kept constexpr and wrapped in a QColor on use.
inline constexpr QRgb kShapeRgb[2] = {0x1754b1, 0xea6f12};
inline QColor shapeColor(int which) { return QColor::fromRgb(kShapeRgb[which]); }

// A drawable shape: a kind plus its integer (grid-snapped) control points.
struct Item {
    Kind kind = Kind::Triangle;
    QVector<QPoint> pts;
};

// Sensible starting control points for a kind. `b` shifts shape B so the two
// shapes start in a visibly interesting overlapping-but-distinct position.
static QVector<QPoint> defaultPoints(Kind k, bool b) {
    QVector<QPoint> p;
    switch (k) {
        case Kind::Point: p = {{0, 0}}; break;
        case Kind::Segment:
        case Kind::OrientedSegment:
        case Kind::Line:
        case Kind::OrientedLine:
        case Kind::Ray:
        case Kind::Halfplane: p = {{-4, -2}, {4, 2}}; break;
        case Kind::Rectangle: p = {{-4, -3}, {4, 3}}; break;
        case Kind::Triangle: p = {{-4, -3}, {4, -3}, {0, 4}}; break;
        case Kind::Disk: p = {{-4, 0}, {4, 0}, {0, 4}}; break;
        case Kind::Convex: p = {{-4, -3}, {4, -3}, {4, 3}, {-4, 3}}; break;
        case Kind::MonotoneChain: p = {{-5, 0}, {-1, 3}, {2, -2}, {5, 1}}; break;
        case Kind::Polyline: p = {{-5, -2}, {-1, 3}, {2, -3}, {5, 2}}; break;
        case Kind::Polygon: p = {{-4, -3}, {4, -3}, {4, 3}, {0, 0}, {-4, 3}}; break;
        case Kind::Count: break;
    }
    if (b) {
        for (QPoint& q : p) q += QPoint(2, 3);
    }
    return p;
}

// ---------------------------------------------------------------------------
// Build an exact pgl::Shape from an Item. Returns nullopt (and an explanatory
// message) when the control points cannot form the requested shape.
// ---------------------------------------------------------------------------
static EPoint mk(const QPoint& p) {
    return EPoint(pgl::ERational(static_cast<long>(p.x())),
                  pgl::ERational(static_cast<long>(p.y())));
}

static std::optional<EShape> buildShape(const Item& it, QString* err) {
    const KindInfo& ki = info(it.kind);
    if (static_cast<int>(it.pts.size()) < ki.minPts) {
        if (err) *err = QString("needs %1 points").arg(ki.minPts);
        return std::nullopt;
    }
    try {
        auto P = [&](int i) { return mk(it.pts[i]); };
        std::vector<EPoint> v;
        v.reserve(it.pts.size());
        for (const QPoint& q : it.pts) v.push_back(mk(q));

        switch (it.kind) {
            case Kind::Point: return EShape(P(0));
            case Kind::Segment: return EShape(pgl::Segment<EPoint>(P(0), P(1)));
            case Kind::OrientedSegment: return EShape(pgl::OrientedSegment<EPoint>(P(0), P(1)));
            case Kind::Line: return EShape(pgl::Line<EPoint>(P(0), P(1)));
            case Kind::OrientedLine: return EShape(pgl::OrientedLine<EPoint>(P(0), P(1)));
            case Kind::Ray: return EShape(pgl::Ray<EPoint>(P(0), P(1)));
            case Kind::Halfplane: return EShape(pgl::Halfplane<EPoint>(P(0), P(1)));
            case Kind::Rectangle: return EShape(pgl::Rectangle<EPoint>(P(0), P(1)));
            case Kind::Triangle: return EShape(pgl::Triangle<EPoint>(P(0), P(1), P(2)));
            case Kind::Disk: return EShape(pgl::Disk<EPoint>(P(0), P(1), P(2)));
            case Kind::Convex: return EShape(pgl::Convex<EPoint>(v));
            case Kind::MonotoneChain: return EShape(pgl::MonotoneChain<EPoint>(v));
            case Kind::Polyline: return EShape(pgl::Polyline<EPoint>(v));
            case Kind::Polygon: return EShape(pgl::Polygon<EPoint>(v));
            case Kind::Count: break;
        }
    } catch (const std::exception& e) {
        if (err) *err = QString::fromUtf8(e.what());
        return std::nullopt;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Clipboard round-trip: a shape as a line of pgl C++ you can paste into code,
// e.g.  ETriangle a{{-4, -3}, {4, -3}, {0, 4}};  or  EPoint b{1, 2};
// ---------------------------------------------------------------------------
static QString serializeShape(const Item& it, char name) {
    QString head = QString("%1 %2").arg(info(it.kind).cpp).arg(QChar(name));
    if (it.pts.isEmpty()) return head + "{};";
    if (it.kind == Kind::Point)
        return head + QString("{%1, %2};").arg(it.pts[0].x()).arg(it.pts[0].y());
    QStringList parts;
    for (const QPoint& p : it.pts)
        parts << QString("{%1, %2}").arg(p.x()).arg(p.y());
    return head + "{" + parts.join(", ") + "};";
}

// Parses text produced by serializeShape (leading type name, then a flat list
// of integer coordinates). Lenient about the variable name, whitespace, and
// trailing semicolon; coordinates are rounded to the integer grid. Returns
// false when the type is unknown or too few coordinates are present.
static bool parseShape(const QString& text, Kind& kind, QVector<QPoint>& pts) {
    static const QRegularExpression idRe("[A-Za-z_]\\w*");
    QRegularExpressionMatch idm = idRe.match(text);
    if (!idm.hasMatch()) return false;
    const QString token = idm.captured(0);
    int ki = -1;
    for (int i = 0; i < static_cast<int>(Kind::Count); ++i)
        if (token.compare(kKinds[i].cpp, Qt::CaseInsensitive) == 0 ||
            token.compare(kKinds[i].name, Qt::CaseInsensitive) == 0) {
            ki = i;
            break;
        }
    if (ki < 0) return false;

    const int brace = text.indexOf('{');
    if (brace < 0) return false;
    static const QRegularExpression numRe("[-+]?\\d+(?:\\.\\d+)?");
    QVector<double> nums;
    auto matches = numRe.globalMatch(text.mid(brace));
    while (matches.hasNext()) nums << matches.next().captured(0).toDouble();
    if (nums.size() < 2 || nums.size() % 2 != 0) return false;

    QVector<QPoint> parsed;
    for (int i = 0; i + 1 < nums.size(); i += 2)
        parsed << QPoint(qRound(nums[i]), qRound(nums[i + 1]));
    if (static_cast<int>(parsed.size()) < kKinds[ki].minPts) return false;
    if (!kKinds[ki].variable) parsed = parsed.mid(0, kKinds[ki].minPts);

    kind = static_cast<Kind>(ki);
    pts = parsed;
    return true;
}

// ---------------------------------------------------------------------------
// Predicates evaluated for a pair, in both directions.
// ---------------------------------------------------------------------------
static const char* kPredicateNames[] = {
    "contains", "boundaryContains", "interiorContains", "intersects",
    "interiorsIntersect", "separates", "crosses"};
static constexpr int kPredicateCount = 7;

static void evalPredicates(const EShape& a, const EShape& b, bool ab[7], bool ba[7]) {
    ab[0] = a.contains(b);            ba[0] = b.contains(a);
    ab[1] = a.boundaryContains(b);    ba[1] = b.boundaryContains(a);
    ab[2] = a.interiorContains(b);    ba[2] = b.interiorContains(a);
    ab[3] = a.intersects(b);          ba[3] = b.intersects(a);
    ab[4] = a.interiorsIntersect(b);  ba[4] = b.interiorsIntersect(a);
    ab[5] = a.separates(b);           ba[5] = b.separates(a);
    ab[6] = a.crosses(b);             ba[6] = b.crosses(a);
}

// A short human description of an intersection result shape.
static QString describe(const EShape& s) {
    if (s.empty()) return "empty (disjoint)";
    if (const EPoint* p = s.getIf<EPoint>()) {
        return QString("Point (%1, %2)")
            .arg(static_cast<double>(p->x()))
            .arg(static_cast<double>(p->y()));
    }
    static const char* names[] = {"", "Point", "Segment", "OrientedSegment",
                                  "Line", "OrientedLine", "Ray", "Halfplane",
                                  "Rectangle", "Triangle", "Disk", "Convex",
                                  "MonotoneChain", "Polyline", "Polygon"};
    // variant index: 0 == EmptyShape, matches the Shape::Variant order.
    return names[s.variant().index()];
}

// ===========================================================================
// Canvas: draws the grid and shapes, handles all editing interaction.
// ===========================================================================
class Canvas : public QWidget {
    Q_OBJECT
  public:
    Item items[2];
    std::optional<EShape> built[2];
    std::optional<EShape> highlight;
    int active = 0;

    Canvas(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(560, 560);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        items[0].kind = Kind::Triangle;
        items[0].pts = defaultPoints(Kind::Triangle, false);
        items[1].kind = Kind::Segment;
        items[1].pts = defaultPoints(Kind::Segment, true);
    }

    void setKind(int which, Kind k) {
        items[which].kind = k;
        items[which].pts = defaultPoints(k, which == 1);
        emit edited();
    }
    void setActive(int which) { active = which; }
    // Replaces a shape's kind and control points wholesale (used by paste).
    void setShape(int which, Kind k, const QVector<QPoint>& pts) {
        items[which].kind = k;
        items[which].pts = pts;
        emit edited();
    }

  signals:
    void edited();  // control points or kinds changed

  protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

  private:
    // View transform: world (grid) units <-> screen pixels, y pointing up.
    double scale_ = 34.0;
    QPointF origin_{0, 0};  // screen pixel of world (0,0); set on first paint

    enum class Mode { None, DragHandle, Translate, Pan };
    Mode mode_ = Mode::None;
    int dragShape_ = -1, dragIndex_ = -1;
    QPoint transStart_;
    QVector<QPoint> transOrig_;
    QPointF lastPx_;
    bool originInit_ = false;

    QPointF w2s(const QPointF& w) const {
        return QPointF(origin_.x() + w.x() * scale_, origin_.y() - w.y() * scale_);
    }
    QPointF s2w(const QPointF& s) const {
        return QPointF((s.x() - origin_.x()) / scale_, (origin_.y() - s.y()) / scale_);
    }
    QPoint snap(const QPointF& w) const {
        return QPoint(qRound(w.x()), qRound(w.y()));
    }
    static QPointF toWorldF(const EPoint& p) {
        return QPointF(static_cast<double>(p.x()), static_cast<double>(p.y()));
    }

    // Returns (shape, index) of a control-point handle within `tol` px of `px`,
    // or (-1,-1). The active shape wins ties so its handles stay grabbable.
    std::pair<int, int> handleAt(const QPointF& px, double tol = 10.0) const {
        int best = -1, bestS = -1;
        double bestD = tol * tol;
        for (int pass = 0; pass < 2; ++pass) {
            int s = (pass == 0) ? active : 1 - active;
            for (int i = 0; i < items[s].pts.size(); ++i) {
                QPointF d = w2s(items[s].pts[i]) - px;
                double dd = d.x() * d.x() + d.y() * d.y();
                if (dd <= bestD) { bestD = dd; best = i; bestS = s; }
            }
            if (bestS != -1) break;  // prefer active shape's handles
        }
        return {bestS, best};
    }

    int insertIndex(const Item& it, const QPoint& p) const;
    void drawEShape(QPainter& p, const EShape& s, const QColor& stroke,
                    const QColor& fill, double penW);
    void drawRaw(QPainter& p, const Item& it, const QColor& stroke);
    void drawHandles(QPainter& p, const Item& it, const QColor& c);
};

void Canvas::wheelEvent(QWheelEvent* e) {
    QPointF at = e->position();
    QPointF before = s2w(at);
    double f = std::pow(1.0015, e->angleDelta().y());
    scale_ = std::clamp(scale_ * f, 6.0, 220.0);
    // keep the world point under the cursor fixed
    origin_ = QPointF(at.x() - before.x() * scale_, at.y() + before.y() * scale_);
    update();
}

void Canvas::mousePressEvent(QMouseEvent* e) {
    lastPx_ = e->position();
    QPointF w = s2w(lastPx_);

    if (e->button() == Qt::RightButton) {
        auto [s, i] = handleAt(lastPx_);
        if (s != -1 && info(items[s].kind).variable &&
            items[s].pts.size() > info(items[s].kind).minPts) {
            items[s].pts.remove(i);
            emit edited();
            update();
        }
        return;
    }
    if (e->button() != Qt::LeftButton) return;

    if (e->modifiers() & Qt::ShiftModifier) {
        mode_ = Mode::Translate;
        transStart_ = snap(w);
        transOrig_ = items[active].pts;
        return;
    }
    auto [s, i] = handleAt(lastPx_);
    if (s != -1) {
        mode_ = Mode::DragHandle;
        dragShape_ = s;
        dragIndex_ = i;
        items[s].pts[i] = snap(w);
        emit edited();
        update();
        return;
    }
    mode_ = Mode::Pan;
}

void Canvas::mouseMoveEvent(QMouseEvent* e) {
    QPointF px = e->position();
    QPointF w = s2w(px);
    switch (mode_) {
        case Mode::DragHandle:
            items[dragShape_].pts[dragIndex_] = snap(w);
            emit edited();
            update();
            break;
        case Mode::Translate: {
            QPoint delta = snap(w) - transStart_;
            for (int i = 0; i < transOrig_.size(); ++i)
                items[active].pts[i] = transOrig_[i] + delta;
            emit edited();
            update();
            break;
        }
        case Mode::Pan:
            origin_ += px - lastPx_;
            lastPx_ = px;
            update();
            break;
        case Mode::None:
            break;
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent*) { mode_ = Mode::None; }

// For a Polyline or Polygon, the position at which inserting `p` grows the
// perimeter the least; the best edge to split, or (for the open polyline) an
// end when simply extending the chain is cheaper. Other variable shapes (Convex
// hull, MonotoneChain sort) reorder their vertices themselves, so p is appended.
int Canvas::insertIndex(const Item& it, const QPoint& p) const {
    const int n = it.pts.size();
    if ((it.kind != Kind::Polyline && it.kind != Kind::Polygon) || n < 2)
        return n;  // append
    auto dist = [](const QPoint& a, const QPoint& b) {
        return std::hypot(double(a.x() - b.x()), double(a.y() - b.y()));
    };
    const bool closed = (it.kind == Kind::Polygon);
    double best = std::numeric_limits<double>::max();
    int bestAt = n;
    const int edges = closed ? n : n - 1;
    for (int i = 0; i < edges; ++i) {
        const QPoint& a = it.pts[i];
        const QPoint& b = it.pts[(i + 1) % n];
        double cost = dist(a, p) + dist(p, b) - dist(a, b);
        if (cost < best) { best = cost; bestAt = i + 1; }
    }
    if (!closed) {  // extending an open chain adds a single edge
        if (double c = dist(p, it.pts[0]); c < best) { best = c; bestAt = 0; }
        if (double c = dist(it.pts[n - 1], p); c < best) { best = c; bestAt = n; }
    }
    return bestAt;
}

void Canvas::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    Item& it = items[active];
    if (!info(it.kind).variable) return;
    QPoint np = snap(s2w(e->position()));
    it.pts.insert(insertIndex(it, np), np);
    emit edited();
    update();
}

void Canvas::drawHandles(QPainter& p, const Item& it, const QColor& c) {
    p.setPen(QPen(c.darker(160), 1.4));
    p.setBrush(c);
    for (const QPoint& q : it.pts) {
        QPointF s = w2s(q);
        p.drawEllipse(s, 4.5, 4.5);
    }
}

// Fallback outline when a shape cannot be built: dashed through control points.
void Canvas::drawRaw(QPainter& p, const Item& it, const QColor& stroke) {
    if (it.pts.size() < 2) return;
    QPen pen(stroke, 1.4, Qt::DashLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPolygonF poly;
    for (const QPoint& q : it.pts) poly << w2s(q);
    p.drawPolyline(poly);
}

// Draw an exact built shape. Bounded polygonal shapes are filled polygons;
// unbounded ones (Line/Ray/Halfplane/OrientedLine) are extended far past the
// viewport; a Disk is a true circle from its exact centre and radius.
void Canvas::drawEShape(QPainter& p, const EShape& s, const QColor& stroke,
                        const QColor& fill, double penW) {
    if (s.empty()) return;
    QPen pen(stroke, penW);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(fill);

    if (const EPoint* pt = s.getIf<EPoint>()) {
        p.setBrush(stroke);
        p.drawEllipse(w2s(toWorldF(*pt)), penW + 2.5, penW + 2.5);
        return;
    }
    if (const auto* d = s.getIf<pgl::Disk<EPoint>>()) {
        QPointF c = toWorldF(d->center());
        double r = std::sqrt(static_cast<double>(d->squaredRadius()));
        QPointF cs = w2s(c);
        p.drawEllipse(cs, r * scale_, r * scale_);
        return;
    }

    // Collect the defining points (world coords).
    std::vector<QPointF> w;
    const std::size_t n = s.size();
    for (std::size_t i = 0; i < n; ++i) w.push_back(toWorldF(s[i]));
    if (w.empty()) return;

    auto W2S = [&](const QPointF& q) { return w2s(q); };
    auto norm = [](QPointF v) {
        double l = std::hypot(v.x(), v.y());
        return l > 0 ? v / l : QPointF(0, 0);
    };
    const double BIG = 4.0 * (width() + height()) / scale_ + 100.0;

    auto arrow = [&](QPointF fromS, QPointF toS) {
        QPointF d = toS - fromS;
        double l = std::hypot(d.x(), d.y());
        if (l < 1e-6) return;
        d /= l;
        QPointF nrm(-d.y(), d.x());
        double a = 11.0;
        QPolygonF head;
        head << toS << (toS - d * a + nrm * a * 0.5) << (toS - d * a - nrm * a * 0.5);
        p.setBrush(stroke);
        p.drawPolygon(head);
        p.setBrush(fill);
    };

    const bool isSeg = s.holdsAlternative<pgl::Segment<EPoint>>();
    const bool isOSeg = s.holdsAlternative<pgl::OrientedSegment<EPoint>>();
    const bool isLine = s.holdsAlternative<pgl::Line<EPoint>>();
    const bool isOLine = s.holdsAlternative<pgl::OrientedLine<EPoint>>();
    const bool isRay = s.holdsAlternative<pgl::Ray<EPoint>>();
    const bool isHP = s.holdsAlternative<pgl::Halfplane<EPoint>>();
    // Polyline and the (weakly x-monotone) MonotoneChain are open chains: draw
    // them as an unfilled polyline, without the closing edge a polygon adds.
    const bool isOpen = s.holdsAlternative<pgl::Polyline<EPoint>>() ||
                        s.holdsAlternative<pgl::MonotoneChain<EPoint>>();

    if (isSeg || isOSeg) {
        p.drawLine(W2S(w[0]), W2S(w[1]));
        if (isOSeg) arrow(W2S(w[0]), W2S(w[1]));
        return;
    }
    if (isLine || isOLine || isRay || isHP) {
        if (w.size() < 2) return;
        QPointF d = norm(w[1] - w[0]);
        if (d == QPointF(0, 0)) return;
        QPointF a = (isRay ? w[0] : w[0] - d * BIG);
        QPointF b = w[1] + d * BIG;
        if (isHP) {
            QPointF ln = QPointF(-d.y(), d.x());  // left normal, world y-up
            QPolygonF fillPoly;
            fillPoly << W2S(a) << W2S(b) << W2S(b + ln * BIG) << W2S(a + ln * BIG);
            QColor hp = fill;
            p.setBrush(hp);
            p.setPen(Qt::NoPen);
            p.drawPolygon(fillPoly);
            p.setPen(pen);
        }
        p.drawLine(W2S(a), W2S(b));
        if (isOLine) arrow(W2S(w[0]), W2S(w[1]));
        return;
    }

    // Bounded polygonal / open-chain shapes.
    QPolygonF poly;
    for (const QPointF& q : w) poly << W2S(q);
    if (isOpen) {
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(poly);
    } else {
        p.drawPolygon(poly);
    }
}

void Canvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (!originInit_) {
        origin_ = QPointF(width() / 2.0, height() / 2.0);
        originInit_ = true;
    }

    p.fillRect(rect(), QColor(0x1f, 0x22, 0x28));

    // Grid.
    QPointF tl = s2w(QPointF(0, 0));
    QPointF br = s2w(QPointF(width(), height()));
    int x0 = std::floor(std::min(tl.x(), br.x())), x1 = std::ceil(std::max(tl.x(), br.x()));
    int y0 = std::floor(std::min(tl.y(), br.y())), y1 = std::ceil(std::max(tl.y(), br.y()));
    p.setPen(QPen(QColor(0x2c, 0x31, 0x3a), 1.0));
    for (int x = x0; x <= x1; ++x) {
        double sx = w2s(QPointF(x, 0)).x();
        p.drawLine(QPointF(sx, 0), QPointF(sx, height()));
    }
    for (int y = y0; y <= y1; ++y) {
        double sy = w2s(QPointF(0, y)).y();
        p.drawLine(QPointF(0, sy), QPointF(width(), sy));
    }
    // Axes.
    p.setPen(QPen(QColor(0x4a, 0x52, 0x60), 1.6));
    p.drawLine(QPointF(0, origin_.y()), QPointF(width(), origin_.y()));
    p.drawLine(QPointF(origin_.x(), 0), QPointF(origin_.x(), height()));

    // Intersection highlight (drawn under the outlines so outlines stay legible).
    if (highlight && !highlight->empty()) {
        drawEShape(p, *highlight, QColor(0xff, 0xd1, 0x66),
                   QColor(0xff, 0xd1, 0x66, 130), 6.0);
    }

    const QColor colA = shapeColor(0), colB = shapeColor(1);
    // Shape A.
    if (built[0])
        drawEShape(p, *built[0], colA, QColor(colA.red(), colA.green(), colA.blue(), 55), 2.2);
    else
        drawRaw(p, items[0], colA);
    // Shape B.
    if (built[1])
        drawEShape(p, *built[1], colB, QColor(colB.red(), colB.green(), colB.blue(), 55), 2.2);
    else
        drawRaw(p, items[1], colB);

    drawHandles(p, items[0], colA);
    drawHandles(p, items[1], colB);

    // Mark the active shape.
    p.setPen(QColor(0xd8, 0xde, 0xe6));
    p.drawText(10, 22, QString("Active: shape %1  (%2)")
                           .arg(active == 0 ? 'A' : 'B')
                           .arg(info(items[active].kind).name));
}

// A group box that emits clicked() when pressed, so the shape's box can be
// clicked to make that shape active. Clicks land here for the box background
// and its passive labels; the combo and buttons handle their own clicks.
class ClickBox : public QGroupBox {
    Q_OBJECT
  public:
    using QGroupBox::QGroupBox;
  signals:
    void clicked();

  protected:
    void mousePressEvent(QMouseEvent* e) override {
        emit clicked();
        QGroupBox::mousePressEvent(e);
    }
};

// ===========================================================================
// Main window: kind selectors, live status, predicate table.
// ===========================================================================
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    MainWindow() {
        canvas_ = new Canvas;

        auto* panel = new QWidget;
        auto* pv = new QVBoxLayout(panel);

        comboA_ = makeCombo();
        comboB_ = makeCombo();
        comboA_->setCurrentIndex(static_cast<int>(canvas_->items[0].kind));
        comboB_->setCurrentIndex(static_cast<int>(canvas_->items[1].kind));

        degenA_ = new QLabel;
        degenB_ = new QLabel;

        // A type combo flanked on the right by copy/paste-as-pgl-code buttons.
        auto button = [](const QString& glyph, const QString& tip) {
            auto* b = new QToolButton;
            b->setText(glyph);
            b->setToolTip(tip);
            b->setAutoRaise(true);
            b->setCursor(Qt::PointingHandCursor);
            return b;
        };
        auto typeRow = [](QComboBox* combo, QToolButton* copyB, QToolButton* pasteB) {
            auto* w = new QWidget;
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(4);
            h->addWidget(combo, 1);
            h->addWidget(copyB);
            h->addWidget(pasteB);
            return w;
        };
        auto* copyA = button("📋", "Copy shape A as pgl C++ code");
        auto* pasteA = button("📥", "Set shape A from pasted pgl C++ code");
        auto* copyB = button("📋", "Copy shape B as pgl C++ code");
        auto* pasteB = button("📥", "Set shape B from pasted pgl C++ code");

        boxA_ = new ClickBox("Shape A  (click to make active)");
        auto* fa = new QFormLayout(boxA_);
        fa->addRow("Type", typeRow(comboA_, copyA, pasteA));
        fa->addRow("Status", degenA_);

        boxB_ = new ClickBox("Shape B  (click to make active)");
        auto* fb = new QFormLayout(boxB_);
        fb->addRow("Type", typeRow(comboB_, copyB, pasteB));
        fb->addRow("Status", degenB_);

        table_ = new QTableWidget(kPredicateCount, 2);
        table_->setHorizontalHeaderLabels({"A ? B", "B ? A"});
        QStringList rows;
        for (int i = 0; i < kPredicateCount; ++i) rows << kPredicateNames[i];
        table_->setVerticalHeaderLabels(rows);
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table_->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionMode(QAbstractItemView::NoSelection);
        table_->setFocusPolicy(Qt::NoFocus);
        table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        for (int r = 0; r < kPredicateCount; ++r)
            for (int c = 0; c < 2; ++c) {
                auto* it = new QTableWidgetItem;
                it->setTextAlignment(Qt::AlignCenter);
                table_->setItem(r, c, it);
            }

        isect_ = new QLabel;
        isect_->setWordWrap(true);
        auto* isectBox = new QGroupBox("A.intersection(B)  [exact ERational]");
        auto* iv = new QVBoxLayout(isectBox);
        iv->addWidget(isect_);

        pv->addWidget(boxA_);
        pv->addWidget(boxB_);
        pv->addWidget(new QLabel("Predicates"));
        pv->addWidget(table_, 1);
        pv->addWidget(isectBox);
        panel->setMaximumWidth(360);

        // The canvas expands to fill horizontal growth; the panel is pinned to
        // its width so no empty gap opens to the right of it.
        canvas_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto* split = new QSplitter;
        split->addWidget(canvas_);
        split->addWidget(panel);
        split->setStretchFactor(0, 1);
        split->setStretchFactor(1, 0);
        setCentralWidget(split);
        setWindowTitle("Pangolin — interactive predicate explorer");
        resize(1040, 900);

        connect(comboA_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int i) { canvas_->setKind(0, static_cast<Kind>(i)); });
        connect(comboB_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int i) { canvas_->setKind(1, static_cast<Kind>(i)); });
        connect(boxA_, &ClickBox::clicked, this, [this] { setActive(0); });
        connect(boxB_, &ClickBox::clicked, this, [this] { setActive(1); });
        connect(canvas_, &Canvas::edited, this, &MainWindow::recompute);

        connect(copyA, &QToolButton::clicked, this, [this] { copyShape(0); });
        connect(copyB, &QToolButton::clicked, this, [this] { copyShape(1); });
        connect(pasteA, &QToolButton::clicked, this, [this] { pasteShape(0, comboA_); });
        connect(pasteB, &QToolButton::clicked, this, [this] { pasteShape(1, comboB_); });

        // Keyboard shortcuts: a/b make shape A/B active; Ctrl+a / Ctrl+b cycle
        // that shape to the next type; Ctrl+c / Ctrl+v copy / paste the active
        // shape. Routed through the shared handlers so the panel stays in sync.
        auto shortcut = [this](const QKeySequence& keys, auto fn) {
            connect(new QShortcut(keys, this), &QShortcut::activated, this, fn);
        };
        shortcut(QKeySequence(Qt::Key_A), [this] { setActive(0); });
        shortcut(QKeySequence(Qt::Key_B), [this] { setActive(1); });
        shortcut(QKeySequence(QStringLiteral("Ctrl+A")), [this] { cycleKind(comboA_); });
        shortcut(QKeySequence(QStringLiteral("Ctrl+B")), [this] { cycleKind(comboB_); });
        shortcut(QKeySequence::Copy, [this] { copyShape(canvas_->active); });
        shortcut(QKeySequence::Paste, [this] {
            pasteShape(canvas_->active, canvas_->active == 0 ? comboA_ : comboB_);
        });

        setActive(0);
        recompute();
    }

    // Makes shape `which` active and restyles the boxes to show it.
    void setActive(int which) {
        canvas_->setActive(which);
        boxA_->setStyleSheet(boxStyle(which == 0, shapeColor(0)));
        boxB_->setStyleSheet(boxStyle(which == 1, shapeColor(1)));
        canvas_->update();
    }

  private slots:
    void recompute() {
        QString errA, errB;
        canvas_->built[0] = buildShape(canvas_->items[0], &errA);
        canvas_->built[1] = buildShape(canvas_->items[1], &errB);

        setStatus(degenA_, canvas_->built[0], errA);
        setStatus(degenB_, canvas_->built[1], errB);

        canvas_->highlight.reset();
        if (canvas_->built[0] && canvas_->built[1]) {
            const EShape& a = *canvas_->built[0];
            const EShape& b = *canvas_->built[1];
            bool ab[7], ba[7];
            evalPredicates(a, b, ab, ba);
            for (int r = 0; r < kPredicateCount; ++r) {
                fillCell(table_->item(r, 0), ab[r]);
                fillCell(table_->item(r, 1), ba[r]);
            }
            try {
                EShape isec = a.intersection<pgl::ERational>(b);
                canvas_->highlight = isec;
                isect_->setText(describe(isec));
            } catch (const std::exception& e) {
                isect_->setText(QString("not representable as one Shape: %1")
                                    .arg(QString::fromUtf8(e.what())));
            }
        } else {
            for (int r = 0; r < kPredicateCount; ++r) {
                clearCell(table_->item(r, 0));
                clearCell(table_->item(r, 1));
            }
            isect_->setText("—");
        }
        canvas_->update();
    }

  private:
    static QComboBox* makeCombo() {
        auto* c = new QComboBox;
        for (int i = 0; i < static_cast<int>(Kind::Count); ++i)
            c->addItem(kKinds[i].name);
        return c;
    }
    static void cycleKind(QComboBox* c) {
        c->setCurrentIndex((c->currentIndex() + 1) % c->count());
    }
    void copyShape(int which) {
        QGuiApplication::clipboard()->setText(
            serializeShape(canvas_->items[which], which == 0 ? 'a' : 'b'));
    }
    void pasteShape(int which, QComboBox* combo) {
        Kind k;
        QVector<QPoint> pts;
        if (!parseShape(QGuiApplication::clipboard()->text(), k, pts)) return;
        // Sync the combo without its handler resetting the pasted points.
        QSignalBlocker block(combo);
        combo->setCurrentIndex(static_cast<int>(k));
        canvas_->setShape(which, k, pts);
    }
    static QString boxStyle(bool active, const QColor& accent) {
        const QString border = active ? accent.name() : QStringLiteral("#3a404b");
        const QString title = active ? accent.name() : QStringLiteral("#9aa4b2");
        return QString("QGroupBox{border:2px solid %1;border-radius:6px;"
                       "margin-top:9px;padding-top:4px;}"
                       "QGroupBox::title{subcontrol-origin:margin;left:9px;"
                       "padding:0 4px;color:%2;}")
            .arg(border, title);
    }
    static void fillCell(QTableWidgetItem* it, bool v) {
        it->setText(v ? "✅" : "❌");
        it->setForeground(QColor(0xd8, 0xde, 0xe6));
    }
    static void clearCell(QTableWidgetItem* it) {
        it->setText("—");
        it->setForeground(QColor(0x66, 0x6d, 0x78));
    }
    static void setStatus(QLabel* lbl, const std::optional<EShape>& s, const QString& err) {
        if (!s) {
            lbl->setText("invalid: " + err);
            lbl->setStyleSheet("color:#ff8a5c;");
        } else if (s->isDegenerate()) {
            lbl->setText("degenerate (results unreliable)");
            lbl->setStyleSheet("color:#ffd166;");
        } else {
            lbl->setText("valid");
            lbl->setStyleSheet("color:#6cdf8a;");
        }
    }

    Canvas* canvas_;
    QComboBox *comboA_, *comboB_;
    QLabel *degenA_, *degenB_, *isect_;
    ClickBox *boxA_, *boxB_;
    QTableWidget* table_;
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/logo.png"));
    MainWindow w;
    w.show();
    return app.exec();
}

#include "main.moc"
