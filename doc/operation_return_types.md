# Exact operation return types

These tables show the return type of each callable operation for all concrete `*` shape aliases, whose coordinate type is `Rational`. Rows are receivers and columns are arguments. `—` means that no overload is available for that ordered pair. `optional`, `variant`, and `vector` abbreviate `std::optional`, `std::variant`, and `std::vector`, respectively.

## `minkowskiSum`

| `receiver \ argument` | `EmptyShape` | `Point` | `Segment` | `OrientedSegment` | `Line` | `OrientedLine` | `Ray` | `Halfplane` | `Rectangle` | `Triangle` | `Disk` | `Convex` | `MonotoneChain` | `Polyline` | `Polygon` | `HalfplaneIntersection` | `PolygonWithHoles` | `Shape` |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `Shape` |
| `Point` | `EmptyShape` | `Point` | `Segment` | `OrientedSegment` | `Line` | `OrientedLine` | `Ray` | `Halfplane` | `Rectangle` | `Triangle` | `Disk` | `Convex` | `MonotoneChain` | `Polyline` | `Polygon` | `HalfplaneIntersection` | `PolygonWithHoles` | `Shape` |
| `Segment` | `EmptyShape` | `Segment` | `Convex` | `Convex` | `—` | `—` | `—` | `—` | `Convex` | `Convex` | `—` | `Convex` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `PolygonWithHoles` | `—` | `vector<PolygonWithHoles>` | `Shape` |
| `OrientedSegment` | `EmptyShape` | `OrientedSegment` | `Convex` | `Convex` | `—` | `—` | `—` | `—` | `Convex` | `Convex` | `—` | `Convex` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `PolygonWithHoles` | `—` | `vector<PolygonWithHoles>` | `Shape` |
| `Line` | `EmptyShape` | `Line` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `Shape` |
| `OrientedLine` | `EmptyShape` | `OrientedLine` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `Shape` |
| `Ray` | `EmptyShape` | `Ray` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `Shape` |
| `Halfplane` | `EmptyShape` | `Halfplane` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `Shape` |
| `Rectangle` | `EmptyShape` | `Rectangle` | `Convex` | `Convex` | `—` | `—` | `—` | `—` | `Rectangle` | `Convex` | `—` | `Convex` | `Polygon` | `PolygonWithHoles` | `PolygonWithHoles` | `—` | `PolygonWithHoles` | `Shape` |
| `Triangle` | `EmptyShape` | `Triangle` | `Convex` | `Convex` | `—` | `—` | `—` | `—` | `Convex` | `Convex` | `—` | `Convex` | `Polygon` | `PolygonWithHoles` | `PolygonWithHoles` | `—` | `PolygonWithHoles` | `Shape` |
| `Disk` | `EmptyShape` | `Disk` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `Shape` |
| `Convex` | `EmptyShape` | `Convex` | `Convex` | `Convex` | `—` | `—` | `—` | `—` | `Convex` | `Convex` | `—` | `Convex` | `Polygon` | `PolygonWithHoles` | `PolygonWithHoles` | `—` | `PolygonWithHoles` | `Shape` |
| `MonotoneChain` | `EmptyShape` | `MonotoneChain` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `—` | `—` | `—` | `Polygon` | `Polygon` | `—` | `Polygon` | `—` | `—` | `PolygonWithHoles` | `—` | `vector<PolygonWithHoles>` | `Shape` |
| `Polyline` | `EmptyShape` | `Polyline` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `—` | `—` | `—` | `PolygonWithHoles` | `PolygonWithHoles` | `—` | `PolygonWithHoles` | `—` | `—` | `PolygonWithHoles` | `—` | `vector<PolygonWithHoles>` | `Shape` |
| `Polygon` | `EmptyShape` | `Polygon` | `PolygonWithHoles` | `PolygonWithHoles` | `—` | `—` | `—` | `—` | `PolygonWithHoles` | `PolygonWithHoles` | `—` | `PolygonWithHoles` | `PolygonWithHoles` | `PolygonWithHoles` | `PolygonWithHoles` | `—` | `PolygonWithHoles` | `Shape` |
| `HalfplaneIntersection` | `EmptyShape` | `HalfplaneIntersection` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `Shape` |
| `PolygonWithHoles` | `EmptyShape` | `PolygonWithHoles` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `—` | `—` | `—` | `PolygonWithHoles` | `PolygonWithHoles` | `—` | `PolygonWithHoles` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `PolygonWithHoles` | `—` | `PolygonWithHoles` | `Shape` |
| `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` |

## `unionWith`

| `receiver \ argument` | `EmptyShape` | `Point` | `Segment` | `OrientedSegment` | `Line` | `OrientedLine` | `Ray` | `Halfplane` | `Rectangle` | `Triangle` | `Disk` | `Convex` | `MonotoneChain` | `Polyline` | `Polygon` | `HalfplaneIntersection` | `PolygonWithHoles` | `Shape` |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `EmptyShape` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Point` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Segment` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `OrientedSegment` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Line` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `OrientedLine` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Ray` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Halfplane` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Rectangle` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `Triangle` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `Disk` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Convex` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `MonotoneChain` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Polyline` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Polygon` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `HalfplaneIntersection` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `PolygonWithHoles` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `Shape` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |

## `difference`

| `receiver \ argument` | `EmptyShape` | `Point` | `Segment` | `OrientedSegment` | `Line` | `OrientedLine` | `Ray` | `Halfplane` | `Rectangle` | `Triangle` | `Disk` | `Convex` | `MonotoneChain` | `Polyline` | `Polygon` | `HalfplaneIntersection` | `PolygonWithHoles` | `Shape` |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `EmptyShape` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Point` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Segment` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `OrientedSegment` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Line` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `OrientedLine` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Ray` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Halfplane` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Rectangle` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Triangle` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Disk` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Convex` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `MonotoneChain` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Polyline` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Polygon` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `HalfplaneIntersection` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `PolygonWithHoles` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `Shape` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |

## `symmetricDifference`

| `receiver \ argument` | `EmptyShape` | `Point` | `Segment` | `OrientedSegment` | `Line` | `OrientedLine` | `Ray` | `Halfplane` | `Rectangle` | `Triangle` | `Disk` | `Convex` | `MonotoneChain` | `Polyline` | `Polygon` | `HalfplaneIntersection` | `PolygonWithHoles` | `Shape` |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `EmptyShape` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Point` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Segment` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `OrientedSegment` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Line` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `OrientedLine` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Ray` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Halfplane` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Rectangle` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `Triangle` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `Disk` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Convex` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `MonotoneChain` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Polyline` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Polygon` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `HalfplaneIntersection` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `PolygonWithHoles` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` | `—` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `—` |
| `Shape` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |

## `intersection`

| `receiver \ argument` | `EmptyShape` | `Point` | `Segment` | `OrientedSegment` | `Line` | `OrientedLine` | `Ray` | `Halfplane` | `Rectangle` | `Triangle` | `Disk` | `Convex` | `MonotoneChain` | `Polyline` | `Polygon` | `HalfplaneIntersection` | `PolygonWithHoles` | `Shape` |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` | `EmptyShape` |
| `Point` | `EmptyShape` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `optional<Point>` | `—` |
| `Segment` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `—` | `optional<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `—` |
| `OrientedSegment` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `—` | `optional<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `—` |
| `Line` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Line>>` | `optional<variant<Point, Line>>` | `optional<variant<Point, Ray>>` | `optional<variant<Point, Line, Ray>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `—` | `optional<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `optional<variant<Point, Segment, Ray, Line>>` | `vector<variant<Point, Segment>>` | `—` |
| `OrientedLine` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Line>>` | `optional<variant<Point, Line>>` | `optional<variant<Point, Ray>>` | `optional<variant<Point, Line, Ray>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `—` | `optional<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `optional<variant<Point, Segment, Ray, Line>>` | `vector<variant<Point, Segment>>` | `—` |
| `Ray` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Ray>>` | `optional<variant<Point, Ray>>` | `optional<variant<Point, Segment, Ray>>` | `optional<variant<Point, Segment, Ray>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `—` | `optional<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `optional<variant<Point, Segment, Ray>>` | `vector<variant<Point, Segment>>` | `—` |
| `Halfplane` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Line, Ray>>` | `optional<variant<Point, Line, Ray>>` | `optional<variant<Point, Segment, Ray>>` | `HalfplaneIntersection` | `optional<variant<Point, Segment, Convex>>` | `optional<variant<Point, Segment, Convex>>` | `—` | `optional<variant<Point, Segment, Convex>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment, Polygon>>` | `HalfplaneIntersection` | `vector<PolygonWithHoles>` | `—` |
| `Rectangle` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment, Convex>>` | `optional<Rectangle>` | `optional<variant<Point, Segment, Convex>>` | `—` | `optional<variant<Point, Segment, Convex>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Polyline, Polygon>>` | `HalfplaneIntersection` | `vector<PolygonWithHoles>` | `—` |
| `Triangle` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment, Convex>>` | `optional<variant<Point, Segment, Convex>>` | `optional<variant<Point, Segment, Convex>>` | `—` | `optional<variant<Point, Segment, Convex>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Polyline, Polygon>>` | `HalfplaneIntersection` | `vector<PolygonWithHoles>` | `—` |
| `Disk` | `EmptyShape` | `optional<Point>` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` | `—` |
| `Convex` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment, Convex>>` | `optional<variant<Point, Segment, Convex>>` | `optional<variant<Point, Segment, Convex>>` | `—` | `optional<variant<Point, Segment, Convex>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Polyline, Polygon>>` | `HalfplaneIntersection` | `vector<PolygonWithHoles>` | `—` |
| `MonotoneChain` | `EmptyShape` | `optional<Point>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `—` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `—` | `vector<variant<Point, Segment>>` | `—` |
| `Polyline` | `EmptyShape` | `optional<Point>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `—` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `—` | `vector<variant<Point, Segment>>` | `—` |
| `Polygon` | `EmptyShape` | `optional<Point>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment, Polygon>>` | `vector<variant<Point, Polyline, Polygon>>` | `vector<variant<Point, Polyline, Polygon>>` | `—` | `vector<variant<Point, Polyline, Polygon>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Polyline, Polygon>>` | `vector<variant<Point, Polyline, Polygon>>` | `vector<PolygonWithHoles>` | `—` |
| `HalfplaneIntersection` | `EmptyShape` | `optional<Point>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment>>` | `optional<variant<Point, Segment, Ray, Line>>` | `optional<variant<Point, Segment, Ray, Line>>` | `optional<variant<Point, Segment, Ray>>` | `HalfplaneIntersection` | `HalfplaneIntersection` | `HalfplaneIntersection` | `—` | `HalfplaneIntersection` | `—` | `—` | `vector<variant<Point, Polyline, Polygon>>` | `HalfplaneIntersection` | `vector<PolygonWithHoles>` | `—` |
| `PolygonWithHoles` | `EmptyShape` | `optional<Point>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` | `vector<PolygonWithHoles>` | `vector<variant<Point, Segment>>` | `vector<variant<Point, Segment>>` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `vector<PolygonWithHoles>` | `—` |
| `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` | `Shape` |
