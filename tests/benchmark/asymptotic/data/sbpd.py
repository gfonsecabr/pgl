#!/usr/bin/env python3
"""Extract polygons of the Salzburg Database into sbpd-*.polygons.

The asymptotic benchmarks sweep three polygon datasets taken from the Salzburg
Database of Polygonal Data (SBPD), release sbgdb-20200507:

  fpg        polygons/random/fpg             (triangulation perturbation)
  spg        polygons/random/spg-a-2opt      (line sweep and 2-opt, first instance "_1")
  fpg-holes  polygons-with-holes/random/fpg  (fpg with holes, the most holes at each size)

The database holds one polygon per size, at sizes of its own choosing, and the
whole release is far too large to check in. So only the sizes a sweep uses are
kept: each sweep's list (bench::linearSizes(max) in ../sizes.hpp, mirrored
below) snapped to the nearest size the database has. The drivers snap their
lists against the sizes found in the file the same way, so whenever a maximum
in sizes.hpp changes, rerun this script with the new maxima.

A polygon with holes is stored at each size with several hole counts; the one
with the most holes is kept. The database's size counts the outer ring alone,
while a benchmark's n counts every vertex of its input, so a polygon with holes
is keyed, and snapped to, by its total over all rings.

The database stores floating-point coordinates; the benchmarks run on integers.
spg's are the unit square with six decimals, so scaling by 10^6 is exact. fpg's
lie in [-1500, 1500]^2 with sixteen digits and are scaled by 1000 and rounded,
except fpg-holes', scaled by 100000: rounded to thousandths, one of its holes
crosses itself, a vertex lying that close to an edge of its own ring. The
script checks that no vertex collapses onto another, and the drivers check
that the rounded rings are still simple and disjoint as they load them.

Usage:
    python3 sbpd.py [--zips DIR] [--max 10000,200]

DIR holds (or receives) the release's zip files, as named on Zenodo; missing
ones are downloaded from doi:10.5281/zenodo.3784788.
"""

import argparse
import io
import lzma
import pathlib
import re
import urllib.request
import zipfile

RECORD = "https://zenodo.org/api/records/3784789/files"
RELEASE = "sbgdb-20200507"
HERE = pathlib.Path(__file__).resolve().parent
FPG_SIZES = list(range(10, 100, 10)) + list(range(100, 10001, 100))

# name -> (zip on Zenodo, folder in it, member name pattern with {n}, database
# sizes, scale). A pattern with {h} is a polygon with holes, of which the file
# with the largest h is taken.
DATASETS = {
    "fpg": (
        f"{RELEASE}--polygons--random--fpg.zip",
        "polygons/random/fpg", "fpg-poly_{n:010d}.graphml.xz",
        FPG_SIZES, 1000,
    ),
    "spg": (
        f"{RELEASE}--polygons--random--spg-a-2opt.zip",
        "polygons/random/spg-a-2opt", "spg-a-poly_{n:07d}_1.graphml.xz",
        list(range(10, 100, 10)) + list(range(100, 1000, 100)) + list(range(1000, 10001, 1000)),
        1000000,
    ),
    "fpg-holes": (
        f"{RELEASE}--polygons-with-holes--random--fpg.zip",
        "polygons-with-holes/random/fpg", "fpg-poly_{n:010d}_h{h}.graphml.xz",
        FPG_SIZES, 100000,
    ),
}

SAMPLES = 32  # bench::kSamples


def linear_sizes(maximum):
    """bench::linearSizes."""
    floor = 4 if maximum // SAMPLES < 4 else maximum // SAMPLES
    return [floor + (maximum - floor) * i // (SAMPLES - 1) for i in range(SAMPLES)]


def snap(n, available):
    """The nearest of `available` to n, the smaller on a tie (bench::sbpdSizes)."""
    return min(available, key=lambda a: (abs(a - n), a))


def fetch(zips, name):
    path = zips / name
    if not path.exists():
        print(f"downloading {name}")
        with urllib.request.urlopen(f"{RECORD}/{name}/content") as r, open(path, "wb") as out:
            while chunk := r.read(1 << 20):
                out.write(chunk)
    return path


def member(z, folder, pattern, n):
    """The zip member holding the size-n polygon: for a pattern with {h}, the
    one with the most holes."""
    prefix = f"{RELEASE}/{folder}/"
    if "{h}" not in pattern:
        return prefix + pattern.format(n=n)
    head = re.escape(pattern.split("{h}")[0].format(n=n))
    counts = [int(m.group(1)) for name in z.namelist()
              if (m := re.fullmatch(re.escape(prefix) + head + r"(\d+)\.graphml\.xz", name))]
    assert counts, f"no polygon with holes of {n} vertices"
    return prefix + pattern.format(n=n, h=max(counts))


def twice_area(ring):
    return abs(sum(x0 * y1 - x1 * y0
                   for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1])))


def rings(graphml, scale):
    """The polygon's rings in boundary order, as scaled integers, the outer one
    (the one enclosing the largest area) first."""
    text = lzma.decompress(graphml).decode()
    coords = {}
    for node_id, body in re.findall(r'<node id="(\d+)">(.*?)</node>', text, re.S):
        x = re.search(r'<data key="x">([^<]*)<', body).group(1)
        y = re.search(r'<data key="y">([^<]*)<', body).group(1)
        coords[int(node_id)] = (round(float(x) * scale), round(float(y) * scale))
    adjacent = {v: [] for v in coords}
    for a, b in re.findall(r'<edge source="(\d+)" target="(\d+)"', text):
        adjacent[int(a)].append(int(b))
        adjacent[int(b)].append(int(a))
    assert all(len(nbrs) == 2 for nbrs in adjacent.values()), "not a union of cycles"
    unvisited = set(coords)
    found = []
    while unvisited:
        start = min(unvisited)
        order, previous, current = [start], None, start
        while True:
            a, b = adjacent[current]
            following = b if a == previous else a
            if following == start:
                break
            order.append(following)
            previous, current = current, following
        unvisited -= set(order)
        found.append([coords[v] for v in order])
    points = [p for ring in found for p in ring]
    assert len(set(points)) == len(points), "rounding merged two vertices"
    return sorted(found, key=twice_area, reverse=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--zips", type=pathlib.Path, default=pathlib.Path("."))
    parser.add_argument("--max", default="10000,200",
                        help="the maxima of the sizes.hpp lists that sweep these datasets")
    args = parser.parse_args()
    args.zips.mkdir(parents=True, exist_ok=True)
    maxima = [int(m) for m in args.max.split(",")]

    for name, (archive, folder, pattern, available, scale) in DATASETS.items():
        holes = "{h}" in pattern
        with zipfile.ZipFile(fetch(args.zips, archive)) as z:
            # total vertex count -> the member holding it
            members = {}
            for size in available:
                chosen = member(z, folder, pattern, size)
                total = sum(map(len, rings(z.read(chosen), scale))) if holes else size
                members.setdefault(total, chosen)
            sizes = sorted({snap(n, list(members)) for m in maxima for n in linear_sizes(m)})
            out = io.StringIO()
            out.write(f"# SBPD {RELEASE} {folder}, coordinates scaled by {scale} and rounded\n")
            out.write("# source: doi:10.5281/zenodo.3784788, extracted by sbpd.py\n")
            if holes:
                out.write("# each region: a line 'region <n>' for its n vertices in all; then per\n"
                          "# ring, the outer one first, a line 'ring <m>' and its m vertices 'x y'\n")
            else:
                out.write("# each polygon: a line 'polygon <n>', then its n vertices 'x y' in "
                          "boundary order\n")
            for n in sizes:
                found = rings(z.read(members[n]), scale)
                assert sum(map(len, found)) == n, f"{name} {n}: wrong vertex count"
                assert holes or len(found) == 1, f"{name} {n}: more than one ring"
                if holes:
                    out.write(f"region {n}\n")
                    for ring in found:
                        out.write(f"ring {len(ring)}\n")
                        out.writelines(f"{x} {y}\n" for x, y in ring)
                else:
                    out.write(f"polygon {n}\n")
                    out.writelines(f"{x} {y}\n" for x, y in found[0])
        target = HERE / f"sbpd-{name}.polygons"
        target.write_text(out.getvalue())
        print(f"{target.name}: {len(sizes)} polygons, sizes {sizes}")


if __name__ == "__main__":
    main()
