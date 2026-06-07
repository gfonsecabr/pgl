#pragma once

/**
 * @file intersections.hpp
 * @brief Segment intersection and crossing algorithms.
 *
 * This header contains the Bentley-Ottmann sweep-line machinery together with
 * the public helpers that expose it through the Pangolin API.
 */

#include <queue>
#include <utility>
#include <array>
#include <queue>
#include <set>
#include <map>

#include "../pgl.hpp"

namespace pgl::detail{
template <class Rational, class Point>
class BentleyOttmann {
    using Number = Point::NumberType;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using RPoint = pgl::Point<Rational>;
    using RSegment = pgl::Segment<RPoint>;
    using CrossingPair = std::array<Segment,2>;

    enum class EventEnum {
        RIGHT, CROSS, VERTICAL, LEFT
    };

    struct Event {
        Rational x;
        EventEnum type;
        Segment s1, s2;

        auto operator<(const Event &other) const { // Order is backwards by x
            return other.x < x;
        }

        friend std::ostream &operator<<(std::ostream &out, const Event &e) {
            return out << e.type << "@" << e.x << ": " << e.s1 << " " << e.s2;
        }
    };

    std::priority_queue<Event> queue;
    Rectangle bbox;
    Rational line;
    using Tree = std::set<Segment,std::function<bool(const Segment&a, const Segment &b)>>;
    Tree tree;
    std::set<CrossingPair> crossingsSet, intersectionSet;
    bool onlyCrossings = true;
    bool detectOnly = false;

    void initQueue(const std::vector<Segment> &segments) {
        for (const Segment &s :segments) {
            if(s.isVertical()) {
                queue.emplace(static_cast<Rational>(s.min().x()), EventEnum::VERTICAL, s, s);
            }
            else {
                queue.emplace(static_cast<Rational>(s.min().x()), EventEnum::LEFT, s, s);
            }
        }
    }

    void initBbox(const std::vector<Segment> &segments) {
        // Min and Max y-coordinate for sentinels
        bbox = Rectangle(segments[0]);
        for (const Segment &s :segments) {
            bbox.insert(s);
        }
        // Grow bbox by 1
        bbox = Rectangle(bbox.min().x()-1, bbox.min().y()-1, bbox.max().x()+1, bbox.max().y()+1);
    }

    // Compare segments by intersection points vertically along line
    bool CompareAlongLine (const Segment& a, const Segment& b) const {
        // Vertical segments are never stored in the set.
        // They compare against other segments by min point along line.
        if (a.isVertical()) {
            if (a.min().y() < std::min(b.min().y(),b.max().y()))
                return true;

            if (std::max(b.min().y(),b.max().y()) < a.min().y())
                return false;

            auto b_isec = b.template yAtX<Rational>(line);
            return a.min().y() < *b_isec;
        }
        if(b.isVertical()) {
            if (std::max(a.min().y(),a.max().y()) < b.min().y())
                return true;

            if (b.min().y() < std::min(a.min().y(),a.max().y()))
                return false;

            auto a_isec = a.template yAtX<Rational>(line);
            return *a_isec < b.min().y();
        }

        if (std::max(a.min().y(),a.max().y()) < std::min(b.min().y(),b.max().y()))
            return true;

        if (std::max(b.min().y(),b.max().y()) < std::min(a.min().y(),a.max().y()))
            return false;

        if (a == b) { // Same segment
            return false;
        }

        auto a_isec = a.template yAtX<Rational>(line);
        auto b_isec = b.template yAtX<Rational>(line);

        if (*a_isec != *b_isec) {
            return *a_isec < *b_isec;
        }

        // Segments intersecting line at same point
        auto o = pgl::orientationSign(a.min(), a.max(), b.max());
        if (o > 0) {
            return true;
        }
        if (o < 0) {
            return false;
        }
        // One segment is a subset of the other
        return a < b;
    }

    void initTree() {
        auto cmp = [this](const Segment& a, const Segment& b){return this->CompareAlongLine(a, b);};
        tree = std::set<Segment, std::function<bool(const Segment&a, const Segment &b)>>(cmp);
        tree.emplace(bbox.edges()[0]); // Bottom edge as sentinel
        tree.emplace(bbox.edges()[2]); // Top edge as sentinel
    }

    void printTree() const {
        std::cout << "Tree: ";
        Segment previous = *tree.begin();
        for(Segment s : tree) {
            if (s != *tree.begin()) {
                if (CompareAlongLine(previous,s)) {
                    std::cout << " < ";
                } else if (CompareAlongLine(s,previous)) {
                    std::cout << " _>_ ";
                }
                else {
                    std::cout << " _=_ ";
                }
            }
            std::cout << s;
            previous = s;
        }
        std::cout << std::endl;
    };

    void printCrossings() const {
        std::cout << "Crossings: ";
        for(auto [sa,sb] : crossingsSet) {
            auto p = std::get<0>(*sa.intersection(sb));
            std::cout << sa << "crosses" << sb << " at " << p << "; ";
        }
        std::cout << std::endl;
    }

    void printQueue() const {
        std::cout << "Queue: ";
        auto l = queue;
        while (!l.empty()) {
            auto ev = l.top();
            std::cout << ev;
            l.pop();
        }
        std::cout << std::endl;
    }

    std::vector<std::vector<Event>> getEvents() {
        std::vector<std::vector<Event>> events(4);
        Rational currentX = queue.top().x;
        do {
            int t = static_cast<int>(queue.top().type);
            events.at(t).push_back(queue.top());
            queue.pop();
        } while (!queue.empty() && queue.top().x == currentX);
        return events;
    }

    void possibleCrossing(Tree::iterator ita, Tree::iterator itb) {
        Segment sa = *ita, sb = *itb;

        CrossingPair pair{sa,sb};
        if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);

        if (sa.crosses(sb) && !crossingsSet.contains(pair)) {
            RPoint cross = std::get<RPoint>(*sa.template intersection<Rational>(sb));
            if (cross.x() > line) {
                // assert(CompareAlongLine(sa,sb));
                queue.emplace(cross.x(), EventEnum::CROSS, sa, sb);
                crossingsSet.insert(pair);
            }
        }
    }

    void processRIGHT(const std::vector<Event> &evts) {
        for (Event ev : evts) {
            auto it1 = tree.find(ev.s1);
            auto it0 = it1; --it0;
            auto it2 = it1; ++it2;
            tree.erase(it1);

            possibleCrossing(it0, it2);
        }

    }

    void getNewCrossEvents(std::vector<Event> &events, Rational currentX) {
        while (!queue.empty() && queue.top().x == currentX) {
            events.push_back(queue.top());
            queue.pop();
        }
    }

    auto findFirst (Segment s, RPoint current) {
        auto it = tree.find(s);
        while (it != tree.begin() && it->contains(current)) {
            --it;
        }
        ++it;
        return it;
    }

    std::map<Rational,std::vector<Segment>> getCrossingSegments(std::vector<Event> &evts, const Rational &currentX) {
        std::map<Rational,std::vector<Segment>> ret;
        std::set<Segment> done;

        for (Event ev : evts) {
            if(!done.contains(ev.s1)) {
                auto isec = ev.s1.template yAtX<Rational>(currentX);
                RPoint current =  RPoint(currentX, *isec);
                auto it = findFirst(ev.s1, current);
                for (; it != tree.end() && it->contains(current); ++it) {
                    if (!done.contains(*it)) {
                        ret[current.y()].push_back(*it);
                        done.insert(*it);
                    }
                }
            }
        }

        return ret;
    }

    void processCROSS(std::vector<Event> &evts, const Rational &currentX) {
        // 3) Check possible new cross events
        getNewCrossEvents(evts, currentX);

        std::map<Rational,std::vector<Segment>> crossingAt = getCrossingSegments(evts, currentX);

        // 4) Do all CROSS removals from tree
        for (const auto &[currentY,segs] : crossingAt) {
            for (Segment s : segs) {
                tree.erase(s);
            }
        }

        // 5) Move the line to currentX
        line = currentX;

        // 6) Do all CROSS insertions to tree
        for (const auto &[currentY,segs] : crossingAt) {
            for (Segment s : segs) {
                tree.insert(s);
            }
        }

        // 7) Create all CROSS new events
        for (const auto &[currentY,segs] : crossingAt) {
            RPoint current(currentX,currentY);
            auto it1 = findFirst(*segs.begin(), current);
            auto it2 = it1;
            for (; it2 != tree.end() && it2->contains(current); ++it2) {
            }
            --it2;

            auto it0 = it1; --it0;
            auto it3 = it2; ++it3;

            // Possible new crossings
            possibleCrossing(it0, it1);
            possibleCrossing(it2, it3);
        }

        // 8) Add crossings to set
        for (const auto &[currentY,segs] : crossingAt) {
            for (size_t i = 0; i+1 < segs.size(); i++) {
                for (size_t j = i+1; j < segs.size(); j++) {
                    CrossingPair pair{segs[i], segs[j]};
                    if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);

                    if (pair[0].crosses(pair[1])) {
                        crossingsSet.insert(pair);
                        if (detectOnly)
                            return;
                    }
                }
            }
        }
    }

    void processRIGHT_interior(const std::vector<Event> &evts) {
        for (Event ev : evts) {
            // Find top segment intersecting ev.s1 on line
            // Use fake vertical segment
            Segment sv(ev.s1.max().x(), ev.s1.max().y(), ev.s1.max().x(), ev.s1.max().y()+1);
            auto it = tree.lower_bound(sv);

            for (; it != tree.begin() && it->contains(ev.s1.max()); --it) {
            }
            if (it != tree.end()) {
                ++it;
            }

            for (; it != tree.end() && it->contains(ev.s1.max()); ++it) {
                CrossingPair pair{ev.s1,*it};
                if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                intersectionSet.insert(pair);
                if (detectOnly)
                    return;
            }
        }
    }

    void processVERTICAL(const std::vector<Event> &evts) {
        for (Event ev : evts) {
            for (auto it = tree.lower_bound(ev.s1); it != tree.end(); ++it) {
                if (!ev.s1.intersects(*it))
                    break;
                if (onlyCrossings) {
                    if (ev.s1.crosses(*it)) {
                        CrossingPair pair{ev.s1, *it};
                        if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                        crossingsSet.insert(pair);
                    }
                }
                else {
                    if (ev.s1.intersects(*it)) {
                        CrossingPair pair{ev.s1, *it};
                        if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                        crossingsSet.insert(pair);
                    }
                }
            }
        }
    }

    void processVERTICAL_interior(const std::vector<Event> &v_evts, const std::vector<Event> &r_evts, const std::vector<Event> &l_evts) {
        std::vector<std::pair<Number, Segment>> order;
        for (Event ev : l_evts) {
            order.emplace_back(ev.s1.min().y(), ev.s1);
        }
        for (Event ev : r_evts) {
            order.emplace_back(ev.s1.max().y(), ev.s1);
        }
        for (Event ev : v_evts) {
            order.emplace_back(ev.s1.min().y(), ev.s1);
            order.emplace_back(ev.s1.max().y(), ev.s1);
        }
        std::sort(order.begin(),order.end());

        for (Event ev : v_evts) {
            auto y1 = ev.s1.min().y();
            auto y2 = ev.s1.max().y();
            for (auto it = std::lower_bound(order.begin(), order.end(), std::make_pair(y1, Segment()));
                 it != order.end() && it->first < y2;
                 ++it) {
                CrossingPair pair{ev.s1, it->second};
                if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                if (pair[0] != pair[1])
                    intersectionSet.insert(pair);
            }
        }
    }

    void processLEFT(const std::vector<Event> &evts) {
        for (Event ev : evts) {
            auto [it1,_] = tree.insert(ev.s1);
            auto it0 = it1; --it0;
            auto it2 = it1; ++it2;

            queue.emplace(static_cast<Rational>(ev.s1.max().x()), EventEnum::RIGHT, ev.s1, ev.s1);
            possibleCrossing(it0, it1);
            possibleCrossing(it1, it2);

            if (!onlyCrossings) {
                while (it0->contains(ev.s1.min())) {
                    CrossingPair pair{*it0, ev.s1};
                    if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                    crossingsSet.insert(pair);
                    --it0;
                }
                while (it2->contains(ev.s1.min())) {
                    CrossingPair pair{*it2, ev.s1};
                    if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                    crossingsSet.insert(pair);
                    ++it2;
                }
            }
        }
    }


    void run(const std::vector<Segment> &segments) {
        // Return directly for 0 or 1 segment
        if (segments.size() <= (size_t) 1)
            return;

        initQueue(segments);
        initBbox(segments);
        line = bbox.min().x();
        initTree();

        while (!queue.empty()) {
            // printTree();
            // 1) Get all events with same x into events
            Rational currentX = queue.top().x;
            std::vector<std::vector<Event>> events = getEvents();

            // 2) Do all RIGHT events
            processRIGHT(events[(size_t)EventEnum::RIGHT]);

            // 3) Check possible new cross events
            // 4) Do all CROSS removals from tree
            // 5) Move the line to currentX
            // 6) Do all CROSS insertions to tree
            // 7) Create all CROSS new events
            // 8) Add new crossings to the output
            processCROSS(events[1], currentX);

            if (!onlyCrossings) {
                processRIGHT_interior(events[(size_t)EventEnum::RIGHT]);
                if (detectOnly && !intersectionSet.empty())
                    break;
            }

            // 10) Do all VERTICAL events
            processVERTICAL(events[(size_t)EventEnum::VERTICAL]);
            if (!onlyCrossings) {
                processVERTICAL_interior(events[(size_t)EventEnum::VERTICAL],
                                         events[(size_t)EventEnum::RIGHT],
                                         events[(size_t)EventEnum::LEFT]);
                if (detectOnly && !intersectionSet.empty())
                    break;
            }

            // 11) Do all LEFT events
            processLEFT(events[(size_t)EventEnum::LEFT]);

            if (detectOnly && !crossingsSet.empty())
                break;
        }
    }

public:
    std::vector<CrossingPair> findCrossings(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        detectOnly = false;
        run(segments);
        return std::vector(crossingsSet.begin(), crossingsSet.end());
    }

    std::vector<CrossingPair> findIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        detectOnly = false;
        run(segments);
        intersectionSet.insert(crossingsSet.begin(), crossingsSet.end());

        // Insert segments sharing an endpoint
        std::map<Point,std::vector<Segment>> adjacent;
        for (const Segment &s : segments) {
            adjacent[s.min()].push_back(s);
            adjacent[s.max()].push_back(s);
        }
        for (const auto &[_,segs] : adjacent) {
            for (size_t i = 0; i+1 < segs.size(); i++) {
                for (size_t j = i+1; j < segs.size(); j++) {
                    CrossingPair pair{segs[i], segs[j]};
                    if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                    intersectionSet.insert(pair);
                }
            }
        }

        return std::vector(intersectionSet.begin(), intersectionSet.end());
    }

    bool detectCrossings(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        detectOnly = true;
        run(segments);
        return !crossingsSet.empty();
    }

    bool detectIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        detectOnly = true;
        run(segments);

        if (!crossingsSet.empty() || !intersectionSet.empty())
            return true;

        // Insert segments sharing an endpoint
        std::set<Point> adjacent;
        for (const Segment &s : segments) {
            auto [_1,b1] = adjacent.insert(s.min());
            if (!b1)
                return true;
            auto [_2,b2] = adjacent.insert(s.max());
            if (!b2)
                return true;
        }

        return false;
    }

    // Tests if every vertex appears in exactly 2 segments
    // and has no intersection elsewhere
    bool testPolygon(const std::vector<Segment> &segments) {
        std::map<Point,size_t> adjacent;
        for (const Segment &s : segments) {
            adjacent[s.min()]++;
            adjacent[s.max()]++;
        }
        for (const auto &[_,cnt] : adjacent) {
            if (cnt != 2) {
                return false;
            }
        }

        onlyCrossings = false;
        detectOnly = true;
        run(segments);

        return crossingsSet.empty() && intersectionSet.empty();
    }

    // Tests if every vertex appears in exactly 2 segments
    // except for two vertices appearing only once
    // and has no intersection elsewhere
    bool testPolyLine(const std::vector<Segment> &segments) {
        std::map<Point,size_t> adjacent;
        int endpoints = 0;
        for (const Segment &s : segments) {
            adjacent[s.min()]++;
            adjacent[s.max()]++;
        }
        for (const auto &[_,cnt] : adjacent) {
            if (cnt > 2) {
                return false;
            }
            if (cnt == 1) {
                if (++endpoints > 2) {
                    return false;
                }
            }
        }

        onlyCrossings = false;
        detectOnly = true;
        run(segments);

        return crossingsSet.empty() && intersectionSet.empty();
    }
}; // class BentleyOttmann
} // namespace pgl::detail

namespace pgl {

/**
 * @brief Finds all intersecting segment pairs with Bentley-Ottmann.
 *
 * Runs in `O((n + k) log n)` where `n` is the number of input segments and
 * `k` is the number of reported pairs.
 *
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of intersecting segment pairs.
 */
template<class Rational = pgl::Rational<__int128_t>, class Container>
auto findIntersections(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<pgl::Segment<Point>> v(segments.begin(),segments.end());

    pgl::detail::BentleyOttmann<Rational, Point> bo;
    return bo.findIntersections(v);
}

/**
 * @brief Finds all proper crossing segment pairs with Bentley-Ottmann.
 *
 * Runs in `O((n + k) log n)` where `n` is the number of input segments and
 * `k` is the number of reported crossing pairs.
 *
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of crossing segment pairs.
 */
template<class Rational = pgl::Rational<__int128_t>, class Container>
auto findCrossings(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<pgl::Segment<Point>> v(segments.begin(),segments.end());

    pgl::detail::BentleyOttmann<Rational,Point> bo;
    return bo.findCrossings(v);
}


/**
 * @brief Detects whether any two segments intersect.
 *
 * Runs in `O(n log n)` in the positive or negative detection mode used here.
 *
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return `true` if at least one intersecting pair exists.
 */
template<class Rational = pgl::Rational<__int128_t>, class Container>
bool detectIntersections(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<pgl::Segment<Point>> v(segments.begin(),segments.end());

    pgl::detail::BentleyOttmann<Rational,Point> bo;
    return bo.detectIntersections(v);
}

/**
 * @brief Detects whether any two segments properly cross.
 *
 * Runs in `O(n log n)` in the positive or negative detection mode used here.
 *
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return `true` if at least one crossing pair exists.
 */
template<class Rational = pgl::Rational<__int128_t>, class Container>
bool detectCrossings(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<pgl::Segment<Point>> v(segments.begin(),segments.end());

    pgl::detail::BentleyOttmann<Rational,Point> bo;
    return bo.detectCrossings(v);
}

/**
 * @brief Finds all crossing segment pairs by brute force.
 *
 * Checks every unordered pair in quadratic time.
 *
 * @tparam Rational Unused template parameter kept for API symmetry.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of crossing segment pairs.
 */
template<class Rational = pgl::Rational<__int128_t>, class Container>
auto bruteForceCrossings(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<std::array<pgl::Segment<Point>,2>> ret;

    for (auto it_i = segments.begin(); it_i != segments.end(); ++it_i) {
        for (auto it_j = it_i; it_j != segments.end(); ++it_j) {
            if (it_i != it_j) {
                pgl::Segment<Point> s1 = *it_i;
                pgl::Segment<Point> s2 = *it_j;
                if (s1.crosses(s2)) {
                    if (s2 < s1)
                        std::swap(s1,s2);
                    ret.push_back({s1,s2});
                }
            }
        }
    }

    return ret;
}


/**
 * @brief Finds all intersecting segment pairs by brute force.
 *
 * Checks every unordered pair in quadratic time.
 *
 * @tparam Rational Unused template parameter kept for API symmetry.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of intersecting segment pairs.
 */
template<class Rational = pgl::Rational<__int128_t>, class Container>
auto bruteForceIntersections(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<std::array<pgl::Segment<Point>,2>> ret;

    for (auto it_i = segments.begin(); it_i != segments.end(); ++it_i) {
        for (auto it_j = it_i; it_j != segments.end(); ++it_j) {
            if (it_i != it_j) {
                pgl::Segment<Point> s1 = *it_i;
                pgl::Segment<Point> s2 = *it_j;
                if (s1.intersects(s2)) {
                    if (s2 < s1)
                        std::swap(s1,s2);
                    ret.push_back({s1,s2});
                }
            }
        }
    }

    return ret;
}

} // namespace pgl

