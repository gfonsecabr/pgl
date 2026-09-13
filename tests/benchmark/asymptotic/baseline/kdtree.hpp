#pragma once
//
// The Kd_trees the baseline's point queries run on, per kernel.
//
// CGAL's defaults -- a sliding-midpoint splitter and buckets of five points --
// are not its fastest tree for these queries, and the reference races CGAL at
// its best. So each query runs on the splitter and bucket size that answer it
// fastest, which makes one tree per query, and a build row reports the fastest
// build among those trees: never a tree tuned only to build fast, which with
// buckets large enough is no index at all. Where two splitters answer a query
// equally fast, the one that builds faster is taken.
//
// The bucket size is what matters, and it pulls the kernels apart. A larger
// bucket trades nodes for points tested per leaf. The counting queries improve
// up to buckets of 16 (rectangles) and 32 (triangles) under both kernels, and
// EPICK's nearest-neighbour search up to 16, but under EPECK that search is
// fastest with buckets of three and nearly twice as slow at 32. The splitter
// matters much less: the median-of-rectangle one counts EPICK rectangles a few
// percent faster than the others, and elsewhere they tie while the median
// splitters build 1.7-2.5x slower.
//
// Measured as the driver measures, each batch timed once on a freshly built
// tree, against CGAL's default tree: every query row is as fast or faster at
// every size of the sweep (rectangles within noise, triangles 8-20% faster,
// nearest neighbour up to 18% faster), and the builds 20-30% faster.
#include "cgal.hpp"

#include <CGAL/Kd_tree.h>
#include <CGAL/Search_traits_2.h>
#include <CGAL/Splitters.h>

namespace bench::cgal {

template <class K>
using Median = CGAL::Median_of_rectangle<CGAL::Search_traits_2<K>>;
template <class K>
using Midpoint = CGAL::Midpoint_of_max_spread<CGAL::Search_traits_2<K>>;

template <class K, class Splitter>
using KdTree = CGAL::Kd_tree<CGAL::Search_traits_2<K>, Splitter>;

template <class K> struct KdTreeTuning;

template <> struct KdTreeTuning<Inexact> {
    using RectangleSplitter = Median<Inexact>;
    using TriangleSplitter  = Midpoint<Inexact>;
    using NearestSplitter   = Midpoint<Inexact>;
    static constexpr int rectangleBucket = 16;
    static constexpr int triangleBucket  = 32;
    static constexpr int nearestBucket   = 16;
};

template <> struct KdTreeTuning<Kernel> {
    using RectangleSplitter = Midpoint<Kernel>;
    using TriangleSplitter  = Midpoint<Kernel>;
    using NearestSplitter   = Midpoint<Kernel>;
    static constexpr int rectangleBucket = 16;
    static constexpr int triangleBucket  = 32;
    static constexpr int nearestBucket   = 3;
};

}  // namespace bench::cgal
