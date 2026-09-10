#pragma once

#include "algorithm/graph.hpp"

/**
 * @file redblacktree.hpp
 * @brief A red-black tree whose nodes are addressable, movable and reorderable.
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace pgl::detail {

/**
 * @brief An ordered set of `T` whose nodes the caller can hold on to, insert at
 * a known position, and exchange without any comparison.
 *
 * @c std::set orders every operation by its comparator: a node is reached by
 * searching for it, and a node moves by being erased and inserted again. That
 * is the right bargain when a comparison is a machine instruction. It is the
 * wrong one for a sweep line, where a comparison is a geometric predicate over
 * exact arithmetic and the caller usually already knows where the node is and
 * where it belongs.
 *
 * So this tree separates the two. @ref insert, @ref find and @ref lowerBound
 * search, and cost the comparisons a search costs. @ref insertBefore,
 * @ref insertAfter, @ref erase and @ref swap take nodes and cost none: the
 * caller says where, and the tree only rebalances. Handles are stable — a node
 * stays at the same address, holding the same value, from its insertion until
 * its erasure, through any number of rebalancings and swaps of other nodes.
 *
 * The order the tree maintains is the caller's responsibility where the caller
 * takes it over. Positional insertion trusts the position it is given, and
 * checks it against the comparator when assertions are enabled. @ref swap
 * cannot be checked that way — a reordering is a sequence of swaps, and the
 * states in between it are meant to be inconsistent — so it is trusted.
 *
 * @tparam T Element type.
 * @tparam Compare Strict weak ordering on `T`. Equivalent elements are
 *         rejected by @ref insert, as in @c std::set.
 */
template <class T, class Compare = std::less<T>>
class RedBlackTree {
public:
    /**
     * @brief One element's node. Handles are these, and they are stable.
     *
     * `value` is exposed to be read. Assigning to it is allowed only when the
     * new value sits at the same place in the order as the old one.
     */
    struct Node {
        // In a union so that the node can exist before its element does: the
        // chunks below hold nodes the tree has not filled yet, and a node whose
        // element has been erased goes back to the free list still a node. The
        // tree starts and ends the element's lifetime; nothing else may.
        union {
            T value;
        };

        Node() {}
        Node(const Node &) = delete;
        Node &operator=(const Node &) = delete;
        ~Node() {}

    private:
        friend class RedBlackTree;
        Node *up;
        Node *down[2];  // 0 is the smaller side, 1 the larger
        bool red;
    };

    using Handle = Node *;
    using value_type = T;
    using key_compare = Compare;

    RedBlackTree() = default;
    explicit RedBlackTree(Compare comp) : comp(std::move(comp)) {}

    RedBlackTree(const RedBlackTree &) = delete;
    RedBlackTree &operator=(const RedBlackTree &) = delete;

    RedBlackTree(RedBlackTree &&other) noexcept { steal(other); }

    RedBlackTree &operator=(RedBlackTree &&other) noexcept {
        if (this != &other) {
            release();
            steal(other);
        }
        return *this;
    }

    ~RedBlackTree() { release(); }

    // ── Bulk state ──────────────────────────────────────────────────────────

    bool empty() const { return count == 0; }
    std::size_t size() const { return count; }
    const Compare &comparator() const { return comp; }

    /// Erases every element, keeping the memory for the nodes to come.
    void clear() {
        destroyValues();
        root = nullptr;
        count = 0;
        // Every node, live or already free, back onto the free list. The chunks
        // stay: a tree that is cleared is usually a tree about to be refilled.
        freeList = nullptr;
        for (const Chunk &chunk : chunks) {
            for (std::size_t i = 0; i < chunk.used; ++i) {
                chunk.nodes[i].down[0] = freeList;
                freeList = &chunk.nodes[i];
            }
        }
    }

    // ── Walking ─────────────────────────────────────────────────────────────

    /// The smallest element's node, or null when the tree is empty.
    Handle first() const { return root ? endmost(root, 0) : nullptr; }

    /// The largest element's node, or null when the tree is empty.
    Handle last() const { return root ? endmost(root, 1) : nullptr; }

    /// The node after @p h, or null when @p h is the last.
    static Handle next(Handle h) { return step(h, 1); }

    /// The node before @p h, or null when @p h is the first.
    static Handle prev(Handle h) { return step(h, 0); }

    // ── Searching ───────────────────────────────────────────────────────────

    // The three searches take anything the comparator can order against an
    // element, not only an element — the transparent lookup @c std::set spells
    // `is_transparent`, and here simply what compiles. A sweep line's status
    // holds edges and is asked where a point falls in it, and building an edge
    // to stand for that point is both awkward and slower than the comparison it
    // saves.

    /// The node holding an element equivalent to @p key, or null if none is.
    template <class Key>
    Handle find(const Key &key) const {
        Handle h = lowerBound(key);
        return h && !comp(key, h->value) ? h : nullptr;
    }

    /// The first node whose element does not compare less than @p key, or null.
    template <class Key>
    Handle lowerBound(const Key &key) const { return bound(key, false); }

    /// The first node whose element compares greater than @p key, or null.
    template <class Key>
    Handle upperBound(const Key &key) const { return bound(key, true); }

    // ── Inserting ───────────────────────────────────────────────────────────

    /**
     * @brief Inserts @p v where the order puts it.
     *
     * Returns the node holding it and whether it was inserted; an element
     * equivalent to one already present is not inserted, and its node is
     * returned instead.
     */
    template <class U>
    std::pair<Handle, bool> insert(U &&v) {
        if (!root) {
            return {attachRoot(std::forward<U>(v)), true};
        }
        Node *parent = root;
        int side;
        for (;;) {
            if (comp(v, parent->value)) {
                side = 0;
            } else if (comp(parent->value, v)) {
                side = 1;
            } else {
                return {parent, false};
            }
            if (!parent->down[side]) {
                break;
            }
            parent = parent->down[side];
        }
        return {attach(parent, side, std::forward<U>(v)), true};
    }

    /// Inserts @p v immediately before @p h, which must not be null.
    template <class U>
    Handle insertBefore(Handle h, U &&v) {
        assert(h && "insertBefore needs a node to insert before");
        assert(!prev(h) || !comp(v, prev(h)->value));
        assert(!comp(h->value, v));
        return insertBeside(h, 0, std::forward<U>(v));
    }

    /// Inserts @p v immediately after @p h, which must not be null.
    template <class U>
    Handle insertAfter(Handle h, U &&v) {
        assert(h && "insertAfter needs a node to insert after");
        assert(!next(h) || !comp(next(h)->value, v));
        assert(!comp(v, h->value));
        return insertBeside(h, 1, std::forward<U>(v));
    }

    /// Inserts @p v as the tree's smallest element.
    template <class U>
    Handle insertFirst(U &&v) {
        assert(!root || !comp(first()->value, v));
        return root ? insertBeside(first(), 0, std::forward<U>(v))
                    : attachRoot(std::forward<U>(v));
    }

    /// Inserts @p v as the tree's largest element.
    template <class U>
    Handle insertLast(U &&v) {
        assert(!root || !comp(v, last()->value));
        return root ? insertBeside(last(), 1, std::forward<U>(v))
                    : attachRoot(std::forward<U>(v));
    }

    // ── Removing ────────────────────────────────────────────────────────────

    /**
     * @brief Removes the node @p h, which must belong to this tree.
     *
     * Returns the node that followed it, so a run can be erased by walking it.
     * Every other handle stays valid; @p h does not.
     */
    Handle erase(Handle h) {
        assert(h && "erase needs a node");
        Handle after = next(h);
        unlink(h);
        recycle(h);
        --count;
        return after;
    }

    // ── Reordering ──────────────────────────────────────────────────────────

    /**
     * @brief Exchanges the positions of @p a and @p b in the order.
     *
     * The two nodes trade places in the tree's shape, keeping the colours of
     * the places rather than of themselves, so the tree's balance is exactly
     * what it was and no rebalancing is needed. Both handles stay valid and
     * keep their values: it is the sequence that changes, not the nodes.
     *
     * The caller is asserting that the exchange leaves the sequence sorted —
     * which is what a sweep line knows when the segments crossing at a point
     * reverse, and what the comparator would have to be asked O(log n) times to
     * rediscover.
     */
    void swap(Handle a, Handle b) {
        assert(a && b && "swap needs two nodes");
        if (a == b) {
            return;
        }
        const Place pa = placeOf(a);
        const Place pb = placeOf(b);
        // Both nodes are read out of the way before either is written, so the
        // case where one is the other's parent needs no separate path: a link
        // that pointed at the node being moved now points at the node that took
        // its place, which is the other one.
        occupy(a, pb, b);
        occupy(b, pa, a);
        // The parents' own links, last, and only where the parent is not the
        // other node — that link was just written by occupy().
        if (!pa.up) {
            root = b;
        } else if (pa.up != b) {
            pa.up->down[pa.side] = b;
        }
        if (!pb.up) {
            root = a;
        } else if (pb.up != a) {
            pb.up->down[pb.side] = a;
        }
    }

    // ── Iteration ───────────────────────────────────────────────────────────

    /// A forward-and-backward walk over the elements in order.
    class const_iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T *;
        using reference = const T &;

        const_iterator() = default;
        const_iterator(Handle node, const RedBlackTree *owner) : node(node), owner(owner) {}

        reference operator*() const { return node->value; }
        pointer operator->() const { return &node->value; }
        Handle handle() const { return node; }

        const_iterator &operator++() { node = next(node); return *this; }
        const_iterator operator++(int) { const_iterator t = *this; ++*this; return t; }
        const_iterator &operator--() { node = node ? prev(node) : owner->last(); return *this; }
        const_iterator operator--(int) { const_iterator t = *this; --*this; return t; }

        bool operator==(const const_iterator &other) const { return node == other.node; }

    private:
        Handle node = nullptr;
        const RedBlackTree *owner = nullptr;
    };

    const_iterator begin() const { return {first(), this}; }
    const_iterator end() const { return {nullptr, this}; }

    // ── Checking ────────────────────────────────────────────────────────────

    /**
     * @brief Whether the tree is a well-formed red-black tree: the parent links
     * agree with the child links, no red node has a red child, every path from
     * the root to a missing child passes the same number of black nodes, and
     * the node count is what @ref size reports.
     *
     * Says nothing about whether the elements are in the comparator's order —
     * a caller reordering nodes itself passes through states where they are
     * not. For tests and assertions; linear.
     */
    bool valid() const {
        if (!root) {
            return count == 0;
        }
        if (root->up || root->red) {
            return false;
        }
        int blackHeight = -1;
        std::size_t seen = 0;
        if (!validSubtree(root, 0, blackHeight, seen)) {
            return false;
        }
        return seen == count;
    }

private:
    // ── Storage ─────────────────────────────────────────────────────────────
    //
    // Nodes come from chunks the tree owns rather than one allocation each: a
    // sweep line inserts and erases a node per event, and the free list hands
    // an erased node straight back to the next insertion.

    struct Chunk {
        Node *nodes;
        std::size_t capacity;
        std::size_t used;
    };

    static constexpr std::size_t kFirstChunk = 32;
    static constexpr std::size_t kLargestChunk = 4096;

    std::vector<Chunk> chunks;
    Node *freeList = nullptr;
    Node *root = nullptr;
    std::size_t count = 0;
    [[no_unique_address]] Compare comp{};

    Node *takeNode() {
        if (freeList) {
            Node *n = freeList;
            freeList = n->down[0];
            return n;
        }
        if (chunks.empty() || chunks.back().used == chunks.back().capacity) {
            const std::size_t capacity =
                chunks.empty() ? kFirstChunk
                               : std::min(chunks.back().capacity * 2, kLargestChunk);
            chunks.push_back({std::allocator<Node>{}.allocate(capacity), capacity, 0});
        }
        Chunk &chunk = chunks.back();
        return std::construct_at(&chunk.nodes[chunk.used++]);
    }

    void recycle(Node *n) {
        std::destroy_at(std::addressof(n->value));
        n->down[0] = freeList;
        freeList = n;
    }

    void destroyValues() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (Node *n = first(); n;) {
                Node *after = next(n);
                std::destroy_at(std::addressof(n->value));
                n = after;
            }
        }
    }

    void release() {
        destroyValues();
        for (const Chunk &chunk : chunks) {
            std::destroy_n(chunk.nodes, chunk.used);
            std::allocator<Node>{}.deallocate(chunk.nodes, chunk.capacity);
        }
        chunks.clear();
        freeList = nullptr;
        root = nullptr;
        count = 0;
    }

    void steal(RedBlackTree &other) {
        chunks = std::move(other.chunks);
        freeList = other.freeList;
        root = other.root;
        count = other.count;
        comp = std::move(other.comp);
        other.chunks.clear();
        other.freeList = nullptr;
        other.root = nullptr;
        other.count = 0;
    }

    // ── Links ───────────────────────────────────────────────────────────────

    static int sideOf(const Node *n) { return n->up->down[1] == n ? 1 : 0; }

    static Handle endmost(Handle h, int side) {
        while (h->down[side]) {
            h = h->down[side];
        }
        return h;
    }

    // The node one position along in direction @p side (1 forward, 0 back).
    static Handle step(Handle h, int side) {
        if (h->down[side]) {
            return endmost(h->down[side], 1 - side);
        }
        while (h->up && sideOf(h) == side) {
            h = h->up;
        }
        return h->up;
    }

    template <class Key>
    Handle bound(const Key &key, bool strict) const {
        Handle found = nullptr;
        for (Handle h = root; h;) {
            // Going left keeps the node as the best candidate so far; going
            // right rules it and everything left of it out.
            const bool goLeft = strict ? comp(key, h->value) : !comp(h->value, key);
            if (goLeft) {
                found = h;
                h = h->down[0];
            } else {
                h = h->down[1];
            }
        }
        return found;
    }

    void rotate(Node *x, int side) {
        // The child on the far side comes up; `side` is the direction x drops.
        Node *y = x->down[1 - side];
        x->down[1 - side] = y->down[side];
        if (y->down[side]) {
            y->down[side]->up = x;
        }
        y->up = x->up;
        if (!x->up) {
            root = y;
        } else {
            x->up->down[sideOf(x)] = y;
        }
        y->down[side] = x;
        x->up = y;
    }

    template <class U>
    Handle attachRoot(U &&v) {
        Node *n = takeNode();
        ::new (static_cast<void *>(std::addressof(n->value))) T(std::forward<U>(v));
        n->up = nullptr;
        n->down[0] = n->down[1] = nullptr;
        n->red = false;
        root = n;
        ++count;
        return n;
    }

    // Hangs a fresh node holding @p v off @p parent on @p side, which must be
    // free, and restores the colouring.
    template <class U>
    Handle attach(Node *parent, int side, U &&v) {
        assert(!parent->down[side]);
        Node *n = takeNode();
        ::new (static_cast<void *>(std::addressof(n->value))) T(std::forward<U>(v));
        n->up = parent;
        n->down[0] = n->down[1] = nullptr;
        n->red = true;
        parent->down[side] = n;
        ++count;
        insertFixup(n);
        return n;
    }

    // The one place a positional insertion can go: either the neighbour slot of
    // @p h itself, when it is free, or the far slot of the node already there,
    // which is free because that node is h's neighbour in the order.
    template <class U>
    Handle insertBeside(Handle h, int side, U &&v) {
        if (!h->down[side]) {
            return attach(h, side, std::forward<U>(v));
        }
        return attach(endmost(h->down[side], 1 - side), 1 - side, std::forward<U>(v));
    }

    void insertFixup(Node *z) {
        while (z->up && z->up->red) {
            Node *parent = z->up;
            Node *grand = parent->up;  // exists: a red parent is not the root
            const int side = sideOf(parent);
            Node *uncle = grand->down[1 - side];
            if (uncle && uncle->red) {
                parent->red = false;
                uncle->red = false;
                grand->red = true;
                z = grand;
                continue;
            }
            if (z == parent->down[1 - side]) {
                z = parent;
                rotate(z, side);
                parent = z->up;
            }
            parent->red = false;
            grand->red = true;
            rotate(grand, 1 - side);
            break;
        }
        root->red = false;
    }

    // Puts @p v in @p u's place. @p v may be null.
    void transplant(Node *u, Node *v) {
        if (!u->up) {
            root = v;
        } else {
            u->up->down[sideOf(u)] = v;
        }
        if (v) {
            v->up = u->up;
        }
    }

    // Takes @p z out of the tree, leaving its own links stale.
    void unlink(Node *z) {
        Node *lost = z;          // the node that actually leaves its place
        Node *filler = nullptr;  // what moves into that place
        Node *fillerParent = nullptr;

        if (!z->down[0] || !z->down[1]) {
            const int side = z->down[0] ? 0 : 1;
            filler = z->down[side];
            fillerParent = z->up;
            transplant(z, filler);
        } else {
            lost = endmost(z->down[1], 0);  // z's successor: no left child
            filler = lost->down[1];
            if (lost->up == z) {
                fillerParent = lost;
            } else {
                fillerParent = lost->up;
                transplant(lost, lost->down[1]);
                lost->down[1] = z->down[1];
                lost->down[1]->up = lost;
            }
            transplant(z, lost);
            lost->down[0] = z->down[0];
            lost->down[0]->up = lost;
            const bool lostColour = lost->red;
            lost->red = z->red;
            // The place that lost a node is the successor's old one, so it is
            // the successor's old colour that decides whether black height fell.
            if (!lostColour) {
                eraseFixup(filler, fillerParent);
            }
            return;
        }

        if (!lost->red) {
            eraseFixup(filler, fillerParent);
        }
    }

    // Restores the black height along the path from @p x, whose subtree is one
    // black short. @p x may be null, which is why its parent comes along.
    void eraseFixup(Node *x, Node *parent) {
        while (x != root && (!x || !x->red)) {
            const int side = parent->down[1] == x ? 1 : 0;
            Node *sibling = parent->down[1 - side];
            // A missing sibling would mean the black heights already differed.
            assert(sibling);
            if (sibling->red) {
                sibling->red = false;
                parent->red = true;
                rotate(parent, side);
                sibling = parent->down[1 - side];
            }
            Node *inner = sibling->down[side];
            Node *outer = sibling->down[1 - side];
            if ((!inner || !inner->red) && (!outer || !outer->red)) {
                sibling->red = true;
                x = parent;
                parent = x->up;
                continue;
            }
            if (!outer || !outer->red) {
                inner->red = false;
                sibling->red = true;
                rotate(sibling, 1 - side);
                sibling = parent->down[1 - side];
                outer = sibling->down[1 - side];
            }
            sibling->red = parent->red;
            parent->red = false;
            outer->red = false;
            rotate(parent, side);
            x = root;
            break;
        }
        if (x) {
            x->red = false;
        }
    }

    // Where a node sits: everything about its position that the node itself
    // does not hold, plus everything it does.
    struct Place {
        Node *up;
        int side;
        Node *down[2];
        bool red;
    };

    static Place placeOf(Node *n) {
        return {n->up, n->up ? sideOf(n) : 0, {n->down[0], n->down[1]}, n->red};
    }

    // Moves @p n into the place @p p, which @p other has just left. A link in
    // @p p that named @p n is a link the two nodes shared, and now names
    // @p other.
    static void occupy(Node *n, const Place &p, Node *other) {
        n->red = p.red;
        n->up = p.up == n ? other : p.up;
        n->down[0] = p.down[0] == n ? other : p.down[0];
        n->down[1] = p.down[1] == n ? other : p.down[1];
        if (n->down[0]) {
            n->down[0]->up = n;
        }
        if (n->down[1]) {
            n->down[1]->up = n;
        }
    }

    bool validSubtree(Handle h, int blacks, int &blackHeight, std::size_t &seen) const {
        if (!h) {
            if (blackHeight < 0) {
                blackHeight = blacks;
            }
            return blacks == blackHeight;
        }
        ++seen;
        if (h->red && ((h->down[0] && h->down[0]->red) || (h->down[1] && h->down[1]->red))) {
            return false;
        }
        for (int side : {0, 1}) {
            if (h->down[side] && h->down[side]->up != h) {
                return false;
            }
        }
        const int below = blacks + (h->red ? 0 : 1);
        return validSubtree(h->down[0], below, blackHeight, seen) &&
               validSubtree(h->down[1], below, blackHeight, seen);
    }
};

}  // namespace pgl::detail
