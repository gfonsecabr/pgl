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
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QRadioButton>
#include <QSplitter>
#include <QTableWidget>
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
    int minPts;       // minimum number of control points
    bool variable;    // may hold more than minPts control points
};

static const KindInfo kKinds[] = {
    {"Point", 1, false},
    {"Segment", 2, false},
    {"OrientedSegment", 2, false},
    {"Line", 2, false},
    {"OrientedLine", 2, false},
    {"Ray", 2, false},
    {"Halfplane", 2, false},
    {"Rectangle", 2, false},
    {"Triangle", 3, false},
    {"Disk", 3, false},
    {"Convex", 3, true},
    {"MonotoneChain", 3, true},
    {"Polyline", 2, true},
    {"Polygon", 3, true},
};

static const KindInfo& info(Kind k) { return kKinds[static_cast<int>(k)]; }

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
    const bool isPolyline = s.holdsAlternative<pgl::Polyline<EPoint>>();

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

    // Bounded polygonal / polyline shapes.
    QPolygonF poly;
    for (const QPointF& q : w) poly << W2S(q);
    if (isPolyline) {
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

    const QColor colA(0x5c, 0xc8, 0xff), colB(0xff, 0x8a, 0x5c);
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

        auto* boxA = new QGroupBox("Shape A");
        auto* fa = new QFormLayout(boxA);
        fa->addRow("Type", comboA_);
        fa->addRow("Status", degenA_);

        auto* boxB = new QGroupBox("Shape B");
        auto* fb = new QFormLayout(boxB);
        fb->addRow("Type", comboB_);
        fb->addRow("Status", degenB_);

        auto* actBox = new QGroupBox("Active shape (edits + translate)");
        auto* av = new QVBoxLayout(actBox);
        radioA_ = new QRadioButton("A");
        radioB_ = new QRadioButton("B");
        radioA_->setChecked(true);
        av->addWidget(radioA_);
        av->addWidget(radioB_);

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

        auto* help = new QLabel(
            "Drag a handle to move a vertex.  Shift+drag to translate the active "
            "shape.  Double-click to add a vertex (Convex / MonotoneChain / "
            "Polyline / Polygon).  Right-click a handle to remove it.  "
            "Drag empty space to pan, wheel to zoom.  Everything snaps to the grid.");
        help->setWordWrap(true);
        help->setStyleSheet("color:#9aa4b2;");

        pv->addWidget(boxA);
        pv->addWidget(boxB);
        pv->addWidget(actBox);
        pv->addWidget(new QLabel("Predicates"));
        pv->addWidget(table_, 1);
        pv->addWidget(isectBox);
        pv->addWidget(help);
        panel->setMaximumWidth(360);

        auto* split = new QSplitter;
        split->addWidget(canvas_);
        split->addWidget(panel);
        split->setStretchFactor(0, 1);
        setCentralWidget(split);
        setWindowTitle("Pangolin — interactive predicate explorer");
        resize(1040, 900);

        connect(comboA_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int i) { canvas_->setKind(0, static_cast<Kind>(i)); });
        connect(comboB_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int i) { canvas_->setKind(1, static_cast<Kind>(i)); });
        connect(radioA_, &QRadioButton::toggled, this, [this](bool on) {
            if (on) { canvas_->setActive(0); canvas_->update(); }
        });
        connect(radioB_, &QRadioButton::toggled, this, [this](bool on) {
            if (on) { canvas_->setActive(1); canvas_->update(); }
        });
        connect(canvas_, &Canvas::edited, this, &MainWindow::recompute);

        recompute();
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
    QRadioButton *radioA_, *radioB_;
    QTableWidget* table_;
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}

#include "main.moc"
