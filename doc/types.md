<img align="left" src="figures/logo.png" width="23%"/>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="figures/logotextdark.svg"/>
  <img alt="Pangoling: Plane Geometry Library" src="figures/logotext.svg" width="65%"/>
</picture>

[![Tests](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml/badge.svg)](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml)
[![Standard](https://img.shields.io/badge/C%2B%2B-20/23/26-rgb(10,66,158).svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-MIT-rgb(216,134,42).svg)](https://opensource.org/licenses/MIT)

<br/>

> ⚠️ **Work in Progress**: This library is still under construction and contains **bugs and missing features**. Use in production environments is not recommended.

## Template Types

`pgl::Point` is shorthand for `pgl::Point<int>`. All other geometry classes are
templates of a point type.

```c++
pgl::Point<double> p = {3,5}, q = {7,9};
pgl::Segment<pgl::Point<double>> s = {p,q};
```

You may use integer, floating-point, rational, or custom numeric coordinate
types as long as they support the required arithmetic.

### Point Label

Points may carry an additional label so that they can stay associated with a
name, id, or payload without going through an external map.

```C++
pgl::Point<int,std::string> p = {3,5,"center"}, q = p;
std::cout << p << << ' ' << q << std::endl;
// Output: center:(3,5) center:(3,5)
q.label() = "other";
if (p == q)
    std::cout << p << << " == " << q << std::endl;
// Output: center:(3,5) == other:(3,5)
```

Important label conventions:

- labels are not compared;
- labels are not hashed;
- copy construction and point-type conversion copy the label when the target
  label type can be built from the source label type;
- arithmetic that modifies an existing point keeps its label;
- arithmetic that creates a new point returns a fresh point whose label is
  default-constructed;
- algorithms that select and return points from an input set preserve the labels
  of the returned input points.


```c++
using Point = pgl::Point<int,std::string>;
using Segment = pgl::Segment<Point>;
using Triangle = pgl::Triangle<Point>;

Triangle tri = {{{0,1,"a"}, {5,2,"b"}, {2,9,"c"}}};
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

You may disable promotion by defining `PGL_DISABLE_PROMOTION` before including


### Large Integers

128-bit integers are available on most compilers (g++ and clang++, but not MSVC) on modern machines. For compatibility, Pangolin defines the type `pgl::int128` as the native `__int128_t` if available, and uses boost to emulate 128 bit integers when not available. Boost is a dependency of pgl only when `__int128_t`is not available (for example under MSVC).

The `BigInt` class is available for arbitrary precision integers. We've chosen to provide our own `BigInt` class for two main reasons. One is to avoid unneeded dependencies. The other is because we found that `boost::multiprecision::cpp_int` is very slow in our use case. In contrast to cryptographic applications, the typical use case of computational geometry includes many small numbers. The `BigInt` type is optimized to be fast when the numbers are not too big, and only allocates heap storage for numbers larger than $2^127$. Check the [benchmark](#benchmark) for details.


### Overflow

As mentioned before, if the coordinate type is an integer, then the predicates are exact unless there is an overflow. Next, we define precise bounds that are guaranteed to avoid overflows. We start with the [predicates](shape_methods.md#predicates) for all shapes except disks, which we consider later on.

If promotion is disabled, then let $T$ denote the type being used to store the coordinates. If promotion is enabled, then let $T$ denote the type after promotion.
Let $MAX(T)$ be the largest value that the integer type $T$ can hold, which is roughly $2^{b-1}$ where $b$ is the number of bits. Let $SAFE(T)$ be the largest absolute value that we can use for $T$ with no overflow. Essentially, we have\
$$\mathrm{SAFE}(T) = \left\lfloor \frac{\sqrt{\mathrm{MAX}(T)}}{2} \right\rfloor$$

Hence, it is safe to use coordinates of the following values.

| base type     | promotion enabled   | promotion disabled  |
| ------------- | ------------------: | ------------------: |
| `int16_t`     |             $23170$ |                $90$ |
| `int32_t`     |    $1.5 \cdot 10^9$ |             $23170$ |
| `int64_t`     | $6.5 \cdot 10^{18}$ |    $1.5 \cdot 10^9$ |
| `int128`      | never               | $6.5 \cdot 10^{18}$ |

For disks, however, we need to use the incircle test predicate and the values are much smaller. They are given by the formula:\
$$\mathrm{SAFE}(T) = \left\lfloor \sqrt{\sqrt{\mathrm{MAX}(T) / 128}} \right\rfloor$$

| base type     | promotion enabled   | promotion disabled  |
| ------------- | ------------------: | ------------------: |
| `int16_t`     |                $63$ |                 $3$ |
| `int32_t`     |             $16383$ |                $63$ |
| `int64_t`     |            $10^{9}$ |             $16383$ |
| `int128`      | never               |            $10^{9}$ |


### Rational Numbers

Important geometric properties may need coordinates that are not integers. For example, we may check if two segments intersect using only integers, but the point of intersection may have non-integer rational coordinates. Floating point numbers are fast and will provide exact results for division by a power of 2, but not for other divisions. The safest choice is to use rational numbers.

Pangolin comes with its own rational number class template `pgl::Rational<T>`, where `T` is set to `int64_t` by default, but may be any integer type, including `pgl::BigInt`. The class stores numbers as a numerator and denominator of type `T` and transparently simplifies the fraction. The simplified numerator and denominator of a `Rational r` are accessible with `r.numerator()` and `r.denominator()`. The same promtion rules are used for `pgl::Rational`:

- `pgl::Rational<int8_t>` is promoted to `pgl::Rational<int16_t>`
- `pgl::Rational<int16_t>` is promoted to `pgl::Rational<int32_t>`
- `pgl::Rational<int32_t>` is promoted to `pgl::Rational<int64_t>`
- `pgl::Rational<int64_t>` is promoted to `pgl::Rational<pgl::int128>`
- `pgl::Rational<pgl::int128>` is promoted to `pgl::Rational<pgl::BigInt>`

If you want to disable all type promotions, it suffices to `#define PGL_DISABLE_PROMOTION` before `#include "pgl.hpp"`.
Notice that the promotion rules are not enough to avoid overflows in some cases as numerators and denominators may grow from $p$ to roughly $p^4$ for prime numbers, even for a simple\
dot product 
$$\frac{a}{a'}\cdot\frac{b}{b'} + \frac{c}{c'}\cdot\frac{d}{d'} = \frac{ab}{a'b'} + \frac{cd}{c'd'} = \frac{abc'd' + cda'b'}{a'b'c'd'}$$\
and orientation test 
$$\left(\frac{a}{a'}+\frac{b}{b'}\right) \cdot \left(\frac{c}{c'}+\frac{d}{d'}\right) = \frac{(ab'+a'b)(cd'+c'd)}{a'b'c'd'} = \frac{ab'cd' + ab'c'd + a'bcd' + a'bc'd}{a'b'c'd'}$$.\
In case of overflow problems or critical applications, `pgl::Rational<pgl::BigInt>` should be used.

Using rational coordinates is fairly easy:

```c++
#include <iostream>
#include "pgl.hpp"

int main() {
    using Point = pgl::Point<pgl::Rational<int>>; // Rational coordinates with int numerator and denominator
    using Segment = pgl::Segment<Point>;

    Point p = {1,0}, q = {4,7};
    Segment s = {p,q}, t = {0,8,2,1};
    if (s.intersects(t)) {
        std::cout << s << " intersects " << t << " at point ";
        auto isec = s.intersection<pgl::Rational<int>>(t);
        Point cross = std::get<0>(*isec);
        std::cout << cross << std::endl;
    }

    return 0;
} // Output: (1,0)--(4,7) intersects (0,8)--(2,1) at point (62/35,9/5)
```

However, rational numbers are significantly slower than integers and floating point numbers. Hence, it is a good idea to defer usage of rational coordinates until necessary:

```c++
#include <iostream>
#include "pgl.hpp"

int main() {
    pgl::Point p = {1,0}, q = {4,7};
    pgl::Segment s = {p,q}, t = {0,8,2,1};
    if (s.intersects(t)) {
        std::cout << s << " intersects " << t << " at point ";
        auto isec = s.intersection<pgl::Rational<int>>(t); // Use rational here
        pgl::Rational<int>>(t) cross = std::get<0>(*isec);
        std::cout << cross << std::endl;
    }

    return 0;
} // Output: (1,0)--(4,7) intersects (0,8)--(2,1) at point (62/35,9/5)
```

You may have noticed that the `intersection` method does not return a point. This is because the intersection of two segments may be null, a point, or a segment. Hence, the result is an [`std::optional`](https://en.cppreference.com/w/cpp/utility/optional.html) of [`std::variant`](https://en.cppreference.com/w/cpp/utility/variant.html) of both point and segment.


### Benchmark

To give an idea of the cost of using different number types, we show a benchmark of the times to test if two segments cross using different types, with and without promotion. The time shown is the average time of the `crosses` predicate on two uniform random segments with integer endpoint coordinates in the -500 to 500 range. Since the class `Rational` is optimized to handle integer coordinates faster, we also perform the same test on rational numbers with the segment coordinates divided by 60. All times are in nanoseconds.

| Type                 | promotion <br/> integer     | no promotion <br/> integer     | promotion <br/> integer / 60 | no promotion <br/> integer / 60 |
| -------------------- | -----------: | -----------: | -----------: | -----------: |
| `int16_t`            | 4.58         | 4.55         |              |              |
| `int32_t`            | 4.55         | 4.39         |              |              |
| `int64_t`            | 7.16         | 4.49         |              |              |
| `int128`             | ?            | 7.80         |              |              |
| `BigInt`             | ?            | ?            |              |              |
| `float`              | 6.92         | 6.01         |              |              |
| `double`             | 12.11        | 5.93         |              |              |
| `long double`        |              | 13.59        |              |              |
| `Rational<int32_t>`  | 18.32        | 15.93        |              | 94.84        |
| `Rational<int64_t>`  | 25.18        | 15.71        | 161.47       | 93.32        |
| `Rational<int128>`   | ?            | 45.27        | ?            | 187.3        |
| `Rational<BigInt>  ` |              | 45.27        |              | 187.3        |


### Boost Number Types

For comparison, we now perform tests on the time of the `pgl::Segment::cross` predicate using boost number types, all without promotion:

| Type                                  |  integer     | integer / 60 |
| ------------------------------------- | -----------: | -----------: |
| `int128`                              |   7.79       |              |
| `boost::multiprecision::int128_t`     |   20.35      |              |
| `BigInt`                              |   190.42     |              |
| `boost::cpp_int`                      |   190.42     |              |
| `pgl::Rational<int64_t>`              |   18.27      |  95.77       |
| `boost::rational<int64_t>`            |   103.17     |  213.80      |
| `pgl::Rational<BigInt>`               |   103.17     |  213.80      |
| `boost::rational<boost::cpp_int>`     |   1956.78    |  2024.58     |

Notice that `boost rational<boost::cpp_int>` is an exact rational number with arbitrarily large integers, which ensures exact calculations, but comes at a high cost in performance: over 100 times when compared to native integer types.

