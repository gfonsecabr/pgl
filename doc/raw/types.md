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

## Template Types

`pgl::Point` is shorthand for `pgl::Point<int>`. All other geometry classes are templates of a point type.

```c++
pgl::Point<double> p = {3,5}, q = {7,9};
pgl::Segment<pgl::Point<double>> s = {p,q};
```

You may use integer, floating-point, rational, or custom numeric coordinate types as long as they support the required arithmetic.
If performance is not critical, you may use arbitrary precision rational numbers everywhere with `ERational`, `EPoint`, `ESegment`, etc.

### Labels

Shapes may carry an additional label so that they can stay associated with a name, id, or weight without going through an external map.

```C++
pgl::Point<int,std::string> p = {3,5,"center"}, q = p;
std::cout << p << ' ' << q << std::endl;
// Output: center:(3,5) center:(3,5)
q.label() = "other";
if (p == q)
    std::cout << p << " == " << q << std::endl;
// Output: center:(3,5) == other:(3,5)
```

Important label conventions:

- labels are not compared;
- labels are not hashed;
- copy construction and point-type conversion copy the label when the target label type can be built from the source label type;
- operations that modify an existing shape keep the label;
- operations that create a new shape return a default-constructed label;
- algorithms that select and return shapes from an input set preserve the labels of the returned shapes.


```c++
using Point = pgl::Point<int,std::string>;
using Segment = pgl::Segment<Point>;
using Triangle = pgl::Triangle<Point>;

Triangle tri({{0,1,"a"}, {5,2,"b"}, {2,9,"c"}});
Segment s = tri.diameter();
std::cout << s << std::endl;
// Output: a:(0,1)--c:(2,9)
```


### Promotion

The geometric [predicates](shape_methods.md#predicates) do not use division anywhere, so they should be exact for integers, unless there is an overflow. To minimize the chances of an overflow, whenever we need to multiply two coordinates, we promote the type to a larger one:

- `int8_t` is promoted to `int16_t`
- `int16_t` is promoted to `int32_t`
- `int32_t` is promoted to `int64_t`
- `int64_t` is promoted to [`pgl::int128`](#large_integers)
- `pgl::int128` is promoted to [`pgl::BigInt`](#large_integers)
- `float` is promoted to `double`
- `double` is promoted to `long double`

You may disable promotion by defining `PGL_DISABLE_PROMOTION` before including any PGL header.


### Large Integers

128-bit integers are available on most compilers (g++ and clang++, but not MSVC) on modern machines. For compatibility, Pangolin defines the type `pgl::int128` as the native `__int128_t` if available, and uses boost to emulate 128 bit integers when not available. Boost is a dependency of pgl only when `__int128_t`is not available (for example under MSVC).

The `BigInt` class is available for arbitrary precision integers. We've chosen to provide our own `BigInt` class for two main reasons. One is to avoid unneeded dependencies. The other is because we found that `boost::multiprecision::cpp_int` is slow in our use case. In contrast to cryptographic applications, the typical use case of computational geometry includes many small numbers. The `BigInt` type is optimized to be fast when the numbers are not too big, and only allocates heap storage for numbers larger than $2^{127}$. It doesn't even include Karatsuba multiplication, as the numbers are not big enough for Karatsuba to be faster. Check the [benchmark](#benchmark) for details.


### Overflow

As mentioned before, if the coordinate type is an integer, then the predicates are exact unless there is an overflow. Next, we define precise bounds that are guaranteed to avoid overflows. We start with the [predicates](shape_methods.md#predicates) for all shapes except disks, which we consider later on.

If promotion is disabled, then let $T$ denote the type being used to store the coordinates. If promotion is enabled, then let $T$ denote the type after promotion.
Let $MAX(T)$ be the largest value that the integer type $T$ can hold, which is roughly $2^{b-1}$ where $b$ is the number of bits. Let $SAFE(T)$ be the largest absolute value that we can use for $T$ with no overflow. Essentially, we have\
$$\mathrm{SAFE}(T) = \left\lfloor \frac{\sqrt{\mathrm{MAX}(T)}}{2} \right\rfloor$$

Hence, it is safe to use coordinates of the following values.

| base type     |            promotion enabled |           promotion disabled |
| ------------- | ---------------------------: | ---------------------------: |
| `int16_t`     |             $23170 > 2^{14}$ |                 $90 > 2^{6}$ |
| `int32_t`     |    $1.5 \cdot 10^9 > 2^{30}$ |             $23170 > 2^{14}$ |
| `int64_t`     | $6.5 \cdot 10^{18} > 2^{62}$ |    $1.5 \cdot 10^9 > 2^{30}$ |
| `int128`      |                        never | $6.5 \cdot 10^{18} > 2^{62}$ |

For disks, the inCircle test promotes numbers twice to avoid overflows.


### Rational Numbers

Important geometric properties may need coordinates that are not integers. For example, we may check if two segments intersect using only integers, but the point of intersection may have non-integer rational coordinates. Floating point numbers are fast and will provide exact results for division by a power of 2, but not for other divisions. The safest choice is to use rational numbers.

Pangolin comes with its own rational number class template `pgl::Rational<T>`, where `T` is set to `int64_t` by default, but may be any integer type, including `pgl::BigInt`. The class stores numbers as a numerator and denominator of type `T` and transparently simplifies the fraction. The simplified numerator and denominator of a `Rational r` are accessible with `r.numerator()` and `r.denominator()`. Rational numbers are never promoted.

Notice that numerators and denominators may grow from $p$ to roughly $p^4$ for prime numbers, even for a simple\
dot product
$$\frac{a}{a'}\cdot\frac{b}{b'} + \frac{c}{c'}\cdot\frac{d}{d'} = \frac{ab}{a'b'} + \frac{cd}{c'd'} = \frac{abc'd' + cda'b'}{a'b'c'd'}$$\
and orientation test
$$\left(\frac{a}{a'}+\frac{b}{b'}\right) \cdot \left(\frac{c}{c'}+\frac{d}{d'}\right) = \frac{(ab'+a'b)(cd'+c'd)}{a'b'c'd'} = \frac{ab'cd' + ab'c'd + a'bcd' + a'bc'd}{a'b'c'd'}$$.\
In case of overflow problems or critical applications, `pgl::Rational<pgl::BigInt>` should be used.

Using rational coordinates is fairly easy:

```c++
using Point = pgl::Point<pgl::Rational<pgl::BigInt>>; // Rational coordinates with BigInt numerator and denominator
using Segment = pgl::Segment<Point>;

Point p = {1,0}, q = {4,7};
Segment s = {p,q}, t = {0,8,2,1};
if (s.intersects(t)) {
    std::cout << s << " intersects " << t << " at point ";
    pgl::Shape isec(s.intersection(t));
    Point cross(isec);
    std::cout << cross << std::endl;
}
// Output: (1,0)--(4,7) intersects (0,8)--(2,1) at point (62/35,9/5)
```

However, rational numbers are significantly slower than integers and floating point numbers. Hence, it is a good idea to defer usage of rational coordinates until necessary:

```c++
pgl::Point p = {1,0}, q = {4,7};
pgl::Segment s = {p,q}, t = {0,8,2,1};
if (s.intersects(t)) {
    std::cout << s << " intersects " << t << " at point ";
    pgl::Shape isec(s.intersection<pgl::Rational<int>>(t)); // Use rational here
    pgl::Point<pgl::Rational<int>> cross(isec);
    std::cout << cross << std::endl;
}
// Output: (1,0)--(4,7) intersects (0,8)--(2,1) at point (62/35,9/5)
```

You may have noticed that the `intersection` method does not return a point. This is because the intersection of two segments may be null, a point, or a segment. Hence, the result is an [`std::optional`](https://en.cppreference.com/w/cpp/utility/optional.html) of [`std::variant`](https://en.cppreference.com/w/cpp/utility/variant.html) of both point and segment.


### Exact Type Aliases

For the common exact, overflow-free configuration, `pgl.hpp` defines `E`-prefixed aliases over `pgl::Rational<pgl::BigInt>` coordinates (labelless shapes). They spare you from spelling out the nested template every time:

| Alias | Definition |
| ----- | ---------- |
| `pgl::ERational` | `pgl::Rational<pgl::BigInt>` |
| `pgl::EPoint` | `pgl::Point<pgl::ERational>` |
| `pgl::EEmptyShape` | `pgl::EmptyShape<pgl::EPoint>` |
| `pgl::ESegment` | `pgl::Segment<pgl::EPoint>` |
| `pgl::EOrientedSegment` | `pgl::OrientedSegment<pgl::EPoint>` |
| `pgl::ELine` | `pgl::Line<pgl::EPoint>` |
| `pgl::EOrientedLine` | `pgl::OrientedLine<pgl::EPoint>` |
| `pgl::ERay` | `pgl::Ray<pgl::EPoint>` |
| `pgl::EHalfplane` | `pgl::Halfplane<pgl::EPoint>` |
| `pgl::ERectangle` | `pgl::Rectangle<pgl::EPoint>` |
| `pgl::ETriangle` | `pgl::Triangle<pgl::EPoint>` |
| `pgl::EDisk` | `pgl::Disk<pgl::EPoint>` |
| `pgl::EConvex` | `pgl::Convex<pgl::EPoint>` |
| `pgl::EPolygon` | `pgl::Polygon<pgl::EPoint>` |
| `pgl::EShape` | `pgl::Shape<pgl::EPoint>` |

The rational example above becomes:

```c++
pgl::ESegment s = {1,0,4,7}, t = {0,8,2,1};
if (s.intersects(t)) {
    pgl::EShape isec(s.intersection(t));
    pgl::EPoint cross(isec);
    std::cout << cross << std::endl;
}
// Output: (62/35,9/5)
```

Since `pgl::Rational<pgl::BigInt>` is roughly 50 times slower than `int` (see the benchmark below), prefer these aliases only when performance is not critical.


### Benchmark

To give an idea of the cost of using different number types, we show a benchmark of the times to test if two segments cross using different types, with and without promotion. The time shown is the average time of the `crosses` predicate on two uniform random segments with integer endpoint coordinates in the -500 to 500 range. Since the class `Rational` is optimized to handle integer coordinates faster, we also perform the same test on rational numbers with the segment coordinates divided by 60. All times are in nanoseconds.

| Type                | promotion <br/> integer | no promotion <br/> integer | no promotion <br/> integer / 60 |
| ------------------- | ----------------------: | -------------------------: | ------------------------------: |
| `int16_t`           |                    4.63 |                       4.62 |                                 |
| `int32_t`           |                    4.59 |                       4.39 |                                 |
| `int64_t`           |                    7.14 |                       4.40 |                                 |
| `int128`            |                   48.74 |                       7.99 |                                 |
| `pgl::BigInt`       |                         |                      34.47 |                                 |
| `float`             |                    6.29 |                       5.76 |                                 |
| `double`            |                   10.33 |                       5.81 |                                 |
| `long double`       |                         |                      12.61 |                                 |
| `Rational<int32_t>` |                         |                      36.82 |                           83.62 |
| `Rational<int64_t>` |                         |                      29.64 |                           30.39 |
| `Rational<int128>`  |                         |                      49.24 |                           53.76 |
| `Rational<BigInt>`  |                         |                     185.28 |                          187.05 |

Notice that the exact `pgl::Rational<BigInt>` type is around 40 times slower than a 32-bit `int` type, even when a 32-bit `int` is enough to calculate the predicate exactly. If performance is not critical, we encourage you to use `pgl::Rational<BigInt>` everywhere (as the python binding does), but if performance is important, avoid using rational numbers when they are not needed (for example, calculating predicates between shapes with integer coordinates).

### Boost Number Types

Boost number types work well with pgl, but are slower in our tests.
We perform tests on the time of the `Segment::crosses` predicate using boost types, including boost GMP wrappers (native GMP wrappers cannot be used because pgl uses `auto` types). All tests are performed without promotion in the same setting as the previous benchmarks:

| Type                              | integer | integer / 60 |
| --------------------------------- | ------: | -----------: |
| `int128`                          |    8.09 |              |
| `boost::multiprecision::int128_t` |   20.34 |              |
| `pgl::BigInt`                     |   51.04 |              |
| `boost::cpp_int`                  |  188.71 |              |
| `GMP mpz_int`                     |  338.66 |              |
| `pgl::Rational<int64_t>`          |   29.56 |        36.04 |
| `boost::rational<int64_t>`        |  105.75 |       225.51 |
| `pgl::Rational<pgl::BigInt>`      |  205.57 |       214.43 |
| `boost::rational<boost::cpp_int>` | 1818.49 |      1931.34 |
| `GMP mpq_rational`                | 1146.64 |      1569.48 |
