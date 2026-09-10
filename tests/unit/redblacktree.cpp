#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pgl.hpp"

#include <algorithm>
#include <memory>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <vector>

using pgl::detail::RedBlackTree;

namespace {

template <class Tree>
std::vector<typename Tree::value_type> contents(const Tree &tree) {
    std::vector<typename Tree::value_type> out;
    for (auto h = tree.first(); h; h = Tree::next(h)) {
        out.push_back(h->value);
    }
    return out;
}

// The same walk backwards, which must give the same sequence reversed: the
// forward links are what next() reads and the backward ones what prev() reads,
// and only walking both ways exercises them both.
template <class Tree>
std::vector<typename Tree::value_type> reverseContents(const Tree &tree) {
    std::vector<typename Tree::value_type> out;
    for (auto h = tree.last(); h; h = Tree::prev(h)) {
        out.push_back(h->value);
    }
    std::reverse(out.begin(), out.end());
    return out;
}

// Counts its own live instances, so a test can tell whether the tree destroys
// every element it is asked to drop and no element twice.
struct Counted {
    static inline int live = 0;
    int key = 0;

    Counted(int key = 0) : key(key) { ++live; }
    Counted(const Counted &other) : key(other.key) { ++live; }
    Counted &operator=(const Counted &other) { key = other.key; return *this; }
    ~Counted() { --live; }

    bool operator<(const Counted &other) const { return key < other.key; }
};

}  // namespace

TEST_CASE("Empty tree") {
    RedBlackTree<int> tree;
    CHECK(tree.empty());
    CHECK(tree.size() == 0);
    CHECK(tree.first() == nullptr);
    CHECK(tree.last() == nullptr);
    CHECK(tree.find(3) == nullptr);
    CHECK(tree.lowerBound(3) == nullptr);
    CHECK(tree.upperBound(3) == nullptr);
    CHECK(tree.begin() == tree.end());
    CHECK(tree.valid());
}

TEST_CASE("Ordered insertion of a shuffled range") {
    std::vector<int> keys(200);
    std::iota(keys.begin(), keys.end(), 0);
    std::mt19937 rng(12345);
    std::shuffle(keys.begin(), keys.end(), rng);

    RedBlackTree<int> tree;
    for (int k : keys) {
        auto [h, inserted] = tree.insert(k);
        CHECK(inserted);
        CHECK(h->value == k);
        CHECK(tree.valid());
    }
    CHECK(tree.size() == keys.size());

    std::vector<int> expected(200);
    std::iota(expected.begin(), expected.end(), 0);
    CHECK(contents(tree) == expected);
    CHECK(reverseContents(tree) == expected);
    CHECK(std::vector<int>(tree.begin(), tree.end()) == expected);
}

TEST_CASE("Equivalent elements are rejected") {
    RedBlackTree<int> tree;
    auto [h1, first] = tree.insert(7);
    auto [h2, second] = tree.insert(7);
    CHECK(first);
    CHECK_FALSE(second);
    CHECK(h1 == h2);
    CHECK(tree.size() == 1);
}

TEST_CASE("Searching") {
    RedBlackTree<int> tree;
    for (int k = 0; k < 100; k += 2) {
        tree.insert(k);
    }

    CHECK(tree.find(40)->value == 40);
    CHECK(tree.find(41) == nullptr);
    CHECK(tree.find(-1) == nullptr);
    CHECK(tree.find(100) == nullptr);

    CHECK(tree.lowerBound(40)->value == 40);
    CHECK(tree.lowerBound(41)->value == 42);
    CHECK(tree.lowerBound(-1)->value == 0);
    CHECK(tree.lowerBound(98)->value == 98);
    CHECK(tree.lowerBound(99) == nullptr);

    CHECK(tree.upperBound(40)->value == 42);
    CHECK(tree.upperBound(41)->value == 42);
    CHECK(tree.upperBound(-1)->value == 0);
    CHECK(tree.upperBound(98) == nullptr);
}

TEST_CASE("Positional insertion") {
    RedBlackTree<int> tree;
    // Built entirely without a search: each element goes where the caller says.
    auto h = tree.insertFirst(50);
    tree.insertBefore(h, 40);
    tree.insertAfter(h, 60);
    tree.insertFirst(10);
    tree.insertLast(90);
    CHECK(contents(tree) == std::vector<int>{10, 40, 50, 60, 90});
    CHECK(tree.valid());

    // Where the slot next to the anchor is taken, the new node goes to the
    // neighbour's free slot instead; both paths have to end up in the same
    // place in the order. Repeatedly inserting just before the same node walks
    // through both.
    for (int k : {33, 34, 35, 36, 37, 38, 39}) {
        tree.insertBefore(tree.find(40), k);
        CHECK(tree.valid());
    }
    CHECK(contents(tree) ==
          std::vector<int>{10, 33, 34, 35, 36, 37, 38, 39, 40, 50, 60, 90});
}

TEST_CASE("Positional insertion builds a balanced tree") {
    // A thousand appends in order: the shape has to stay logarithmic, which is
    // what distinguishes rebalancing from a linked list.
    RedBlackTree<int> tree;
    for (int k = 0; k < 1000; ++k) {
        tree.insertLast(k);
    }
    CHECK(tree.valid());
    CHECK(tree.size() == 1000);
    std::vector<int> expected(1000);
    std::iota(expected.begin(), expected.end(), 0);
    CHECK(contents(tree) == expected);

    RedBlackTree<int> backwards;
    for (int k = 999; k >= 0; --k) {
        backwards.insertFirst(k);
    }
    CHECK(backwards.valid());
    CHECK(contents(backwards) == expected);
}

TEST_CASE("Erasing returns the following node") {
    RedBlackTree<int> tree;
    for (int k = 0; k < 10; ++k) {
        tree.insert(k);
    }
    auto h = tree.find(4);
    auto after = tree.erase(h);
    REQUIRE(after != nullptr);
    CHECK(after->value == 5);
    CHECK(tree.erase(tree.find(9)) == nullptr);
    CHECK(contents(tree) == std::vector<int>{0, 1, 2, 3, 5, 6, 7, 8});
    CHECK(tree.valid());
}

TEST_CASE("Erasing a run by walking it") {
    RedBlackTree<int> tree;
    for (int k = 0; k < 50; ++k) {
        tree.insert(k);
    }
    auto h = tree.find(20);
    for (int i = 0; i < 10; ++i) {
        h = tree.erase(h);
        CHECK(tree.valid());
    }
    CHECK(tree.size() == 40);
    CHECK(h->value == 30);
    CHECK(tree.find(25) == nullptr);
    CHECK(tree.find(19)->value == 19);
    CHECK(tree.find(30)->value == 30);
}

TEST_CASE("Handles survive other nodes' insertions and erasures") {
    RedBlackTree<int> tree;
    std::vector<RedBlackTree<int>::Handle> handles;
    for (int k = 0; k < 200; ++k) {
        handles.push_back(tree.insert(k * 2).first);
    }
    // Enough insertions to rotate the tree many times over.
    for (int k = 0; k < 200; ++k) {
        tree.insert(k * 2 + 1);
    }
    for (int k = 0; k < 200; ++k) {
        CHECK(handles[k]->value == k * 2);
    }
    for (int k = 1; k < 400; k += 2) {
        tree.erase(tree.find(k));
    }
    for (int k = 0; k < 200; ++k) {
        CHECK(handles[k]->value == k * 2);
    }
    CHECK(tree.valid());
}

TEST_CASE("Swapping two nodes exchanges their positions") {
    RedBlackTree<int, std::greater<int>> tree{std::greater<int>()};
    // A comparator that is not std::less, so the swap cannot be checked against
    // an order the tree is not actually keeping.
    for (int k = 0; k < 20; ++k) {
        tree.insert(k);
    }
    std::vector<int> expected(20);
    std::iota(expected.rbegin(), expected.rend(), 0);
    REQUIRE(contents(tree) == expected);

    auto a = tree.find(12);
    auto b = tree.find(11);
    tree.swap(a, b);
    CHECK(a->value == 12);
    CHECK(b->value == 11);
    std::swap(expected[7], expected[8]);
    CHECK(contents(tree) == expected);
    CHECK(reverseContents(tree) == expected);
    CHECK(tree.valid());
}

TEST_CASE("Swapping is exhaustive over every pair of positions") {
    // Every pair, at every size up to one that forces several tree shapes: the
    // parent-child cases are the ones a generic swap gets wrong, and they only
    // arise for particular pairs of particular shapes.
    for (int n = 2; n <= 40; ++n) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                RedBlackTree<int> tree;
                std::vector<RedBlackTree<int>::Handle> handles;
                for (int k = 0; k < n; ++k) {
                    handles.push_back(tree.insert(k).first);
                }
                std::vector<int> expected(n);
                std::iota(expected.begin(), expected.end(), 0);

                tree.swap(handles[i], handles[j]);
                std::swap(expected[i], expected[j]);

                REQUIRE(tree.valid());
                REQUIRE(contents(tree) == expected);
                REQUIRE(reverseContents(tree) == expected);
                REQUIRE(handles[i]->value == i);
                REQUIRE(handles[j]->value == j);
            }
        }
    }
}

TEST_CASE("A swapped tree still inserts and erases correctly") {
    RedBlackTree<int> tree;
    std::vector<RedBlackTree<int>::Handle> handles;
    for (int k = 0; k < 64; ++k) {
        handles.push_back(tree.insert(k * 10).first);
    }
    // Reverse a run of eight by swapping its ends inward, the way a sweep line
    // turns a crossing over, then keep using the tree.
    for (int i = 0; i < 4; ++i) {
        tree.swap(handles[20 + i], handles[27 - i]);
    }
    CHECK(tree.valid());
    std::vector<int> expected;
    for (int k = 0; k < 64; ++k) {
        expected.push_back(k * 10);
    }
    std::reverse(expected.begin() + 20, expected.begin() + 28);
    CHECK(contents(tree) == expected);

    for (int i = 0; i < 4; ++i) {
        tree.swap(handles[20 + i], handles[27 - i]);
    }
    CHECK(tree.valid());
    for (int k = 0; k < 64; ++k) {
        CHECK(tree.find(k * 10) == handles[k]);
    }
    for (int k = 0; k < 64; k += 2) {
        tree.erase(handles[k]);
        CHECK(tree.valid());
    }
    CHECK(tree.size() == 32);
}

TEST_CASE("Random operations against std::set") {
    std::mt19937 rng(987654321);
    for (int trial = 0; trial < 20; ++trial) {
        RedBlackTree<int> tree;
        std::set<int> reference;
        for (int op = 0; op < 3000; ++op) {
            const int key = std::uniform_int_distribution<int>(0, 300)(rng);
            switch (std::uniform_int_distribution<int>(0, 3)(rng)) {
            case 0:
            case 1: {
                const auto [h, inserted] = tree.insert(key);
                const bool expected = reference.insert(key).second;
                REQUIRE(inserted == expected);
                REQUIRE(h->value == key);
                break;
            }
            case 2: {
                auto h = tree.find(key);
                REQUIRE((h != nullptr) == reference.contains(key));
                if (h) {
                    tree.erase(h);
                    reference.erase(key);
                }
                break;
            }
            case 3: {
                auto h = tree.lowerBound(key);
                auto it = reference.lower_bound(key);
                REQUIRE((h == nullptr) == (it == reference.end()));
                if (h) {
                    REQUIRE(h->value == *it);
                }
                break;
            }
            }
            REQUIRE(tree.size() == reference.size());
        }
        REQUIRE(tree.valid());
        REQUIRE(contents(tree) == std::vector<int>(reference.begin(), reference.end()));
        REQUIRE(reverseContents(tree) == std::vector<int>(reference.begin(), reference.end()));
    }
}

TEST_CASE("Random positional operations against a vector") {
    // insertBefore, insertAfter, erase and swap all address the sequence rather
    // than the order, so a vector is the reference they answer to. The
    // comparator calls every element equivalent, which is the honest one for a
    // tree used only positionally.
    struct Unordered {
        bool operator()(int, int) const { return false; }
    };
    std::mt19937 rng(24680);
    for (int trial = 0; trial < 20; ++trial) {
        RedBlackTree<int, Unordered> tree;
        std::vector<RedBlackTree<int, Unordered>::Handle> handles;
        std::vector<int> reference;
        int nextKey = 0;

        handles.push_back(tree.insertFirst(nextKey));
        reference.push_back(nextKey++);

        for (int op = 0; op < 800; ++op) {
            const std::size_t at =
                std::uniform_int_distribution<std::size_t>(0, reference.size() - 1)(rng);
            switch (std::uniform_int_distribution<int>(0, 3)(rng)) {
            case 0: {
                handles.insert(handles.begin() + at,
                               tree.insertBefore(handles[at], nextKey));
                reference.insert(reference.begin() + at, nextKey++);
                break;
            }
            case 1: {
                handles.insert(handles.begin() + at + 1,
                               tree.insertAfter(handles[at], nextKey));
                reference.insert(reference.begin() + at + 1, nextKey++);
                break;
            }
            case 2: {
                if (reference.size() > 1) {
                    tree.erase(handles[at]);
                    handles.erase(handles.begin() + at);
                    reference.erase(reference.begin() + at);
                }
                break;
            }
            case 3: {
                const std::size_t other =
                    std::uniform_int_distribution<std::size_t>(0, reference.size() - 1)(rng);
                tree.swap(handles[at], handles[other]);
                std::swap(handles[at], handles[other]);
                std::swap(reference[at], reference[other]);
                break;
            }
            }
            REQUIRE(tree.size() == reference.size());
        }
        REQUIRE(tree.valid());
        REQUIRE(contents(tree) == reference);
        REQUIRE(reverseContents(tree) == reference);
        for (std::size_t i = 0; i < handles.size(); ++i) {
            REQUIRE(handles[i]->value == reference[i]);
        }
    }
}

TEST_CASE("Elements are destroyed exactly once") {
    REQUIRE(Counted::live == 0);
    {
        RedBlackTree<Counted> tree;
        for (int k = 0; k < 500; ++k) {
            tree.insert(Counted(k));
        }
        CHECK(Counted::live == 500);
        for (int k = 0; k < 500; k += 2) {
            tree.erase(tree.find(Counted(k)));
        }
        CHECK(Counted::live == 250);
        tree.clear();
        CHECK(Counted::live == 0);
        // Cleared, then refilled out of the same chunks.
        for (int k = 0; k < 100; ++k) {
            tree.insert(Counted(k));
        }
        CHECK(Counted::live == 100);
        CHECK(tree.valid());
    }
    CHECK(Counted::live == 0);
}

TEST_CASE("Moving a tree") {
    RedBlackTree<Counted> tree;
    for (int k = 0; k < 100; ++k) {
        tree.insert(Counted(k));
    }
    auto handle = tree.find(Counted(42));

    RedBlackTree<Counted> moved(std::move(tree));
    CHECK(moved.size() == 100);
    CHECK(Counted::live == 100);
    CHECK(handle->value.key == 42);  // handles follow the nodes, not the tree
    CHECK(moved.valid());

    RedBlackTree<Counted> target;
    target.insert(Counted(-1));
    target = std::move(moved);
    CHECK(target.size() == 100);
    CHECK(Counted::live == 100);
    CHECK(target.valid());
    CHECK(target.find(Counted(42)) == handle);
}

TEST_CASE("A stateful comparator") {
    // The sweep line's comparator reads state that changes under the tree, so
    // the tree has to hold the comparator by value and use that copy.
    struct ByOffset {
        const int *offset;
        bool operator()(int a, int b) const {
            return ((a + *offset) % 10) < ((b + *offset) % 10);
        }
    };
    int offset = 0;
    RedBlackTree<int, ByOffset> tree{ByOffset{&offset}};
    for (int k = 0; k < 10; ++k) {
        tree.insert(k);
    }
    CHECK(contents(tree) == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    CHECK(tree.find(7)->value == 7);
    offset = 3;
    // The stored order is now stale, but the sequence the tree holds is not:
    // walking it still gives what was inserted, in the order it was inserted in.
    CHECK(contents(tree) == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
}

TEST_CASE("Segments in sweep order") {
    // The use the tree exists for: shapes, a geometric comparator, and nodes
    // reordered without asking it anything.
    using Segment = pgl::Segment<pgl::Point<int>>;
    struct ByHeight {
        bool operator()(const Segment &a, const Segment &b) const {
            if (a.min().y() != b.min().y()) {
                return a.min().y() < b.min().y();
            }
            return a < b;
        }
    };
    RedBlackTree<Segment, ByHeight> tree;
    std::vector<RedBlackTree<Segment, ByHeight>::Handle> handles;
    for (int y = 0; y < 8; ++y) {
        handles.push_back(tree.insert(Segment(0, y, 10, y)).first);
    }
    CHECK(tree.valid());
    CHECK(tree.first()->value == Segment(0, 0, 10, 0));
    CHECK(tree.last()->value == Segment(0, 7, 10, 7));

    for (int i = 0; i < 4; ++i) {
        tree.swap(handles[i], handles[7 - i]);
    }
    CHECK(tree.valid());
    std::vector<Segment> reversed;
    for (auto h = tree.first(); h; h = decltype(tree)::next(h)) {
        reversed.push_back(h->value);
    }
    REQUIRE(reversed.size() == 8);
    for (int i = 0; i < 8; ++i) {
        CHECK(reversed[i] == Segment(0, 7 - i, 10, 7 - i));
    }
}

TEST_CASE("Searching with a key that is not an element") {
    // A comparator that orders elements against each other and against a key of
    // another type entirely, the way a sweep line's status orders edges among
    // themselves and against a point.
    struct ByTens {
        bool operator()(int a, int b) const { return a < b; }
        bool operator()(int a, const std::string &b) const { return std::to_string(a) < b; }
        bool operator()(const std::string &a, int b) const { return a < std::to_string(b); }
    };
    RedBlackTree<int, ByTens> tree;
    for (int k : {100, 200, 300, 400}) {
        tree.insert(k);
    }
    CHECK(tree.lowerBound(std::string("200"))->value == 200);
    CHECK(tree.lowerBound(std::string("250"))->value == 300);
    CHECK(tree.lowerBound(std::string("500")) == nullptr);
    CHECK(tree.upperBound(std::string("200"))->value == 300);
    CHECK(tree.find(std::string("300"))->value == 300);
    CHECK(tree.find(std::string("350")) == nullptr);

    // The element-typed calls still resolve, and to the same answers.
    CHECK(tree.lowerBound(250)->value == 300);
    CHECK(tree.find(300)->value == 300);
    CHECK(tree.find(350) == nullptr);
}
