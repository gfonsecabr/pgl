#pragma once

/**
 * @file windows_traps.hpp
 * @brief Reproduces, on POSIX compilers, the identifier minefield that
 *        `<windows.h>` lays down in every MSVC test build.
 *
 * Every test translation unit includes `doctest.h`, which — under MSVC only —
 * pulls in `<windows.h>` to install its structured-exception handler. That
 * happens at the top of the file, before `pgl.hpp` and before the test body, so
 * for the whole of the file a pile of Win32 macros and global function names are
 * in scope. Two of them bite this repository repeatedly:
 *
 *   - `windef.h` does `#define far` and `#define near` (empty, the vestigial
 *     16-bit segment qualifiers). A local named `far` vanishes, and the
 *     declaration collapses into a cascade of syntax errors far from the cause.
 *   - `wingdi.h` declares `Polygon`, `Rectangle`, `Polyline`, `Ellipse` and
 *     friends as global functions. A test-local `using Polygon = pgl::Polygon<…>`
 *     is then a redeclaration as a different kind of entity — and no amount of
 *     scoping or shadowing fixes it, only renaming the alias.
 *
 * g++ and clang see none of this, so the mistake is invisible locally and only
 * surfaces half an hour later in the MSVC job. Force-including this header into
 * every POSIX test build (see `tests/run_tests.sh`) moves that discovery to the
 * moment the test is first compiled.
 *
 * The set below is deliberately *exactly* what a doctest build sees, no more.
 * doctest defines both `WIN32_LEAN_AND_MEAN` and `NOMINMAX` before including
 * `<windows.h>`, which is why two otherwise notorious traps are absent here:
 * `min`/`max` are suppressed by `NOMINMAX`, and `rpcndr.h`'s `#define small char`
 * never arrives because lean-and-mean drops `rpc.h`. Adding either would reject
 * code that MSVC compiles happily. Keep this file calibrated to the real build:
 * a trap that CI does not actually have is a false alarm, and a false alarm here
 * costs more than the bug it imagines.
 *
 * On Windows this header is inert — the genuine article is already in scope.
 */

#if !defined(_WIN32)

// windef.h — the empty segment qualifiers, plus the SAL-era parameter
// annotations that are likewise defined away.
#define far
#define near
#define IN
#define OUT
#define OPTIONAL

// wingdi.h — `#define ERROR 0`, which collides with any enumerator, constant or
// variable of that name.
#define ERROR 0

// wingdi.h drawing entry points, as global names. The signatures are irrelevant:
// what matters is that the identifier is taken at global scope, exactly as it is
// on Windows. `extern "C"` keeps them un-overloadable, so a conflicting `using`
// alias or class is rejected the same way MSVC rejects it.
extern "C" {
int Arc(void *, int, int, int, int, int, int, int, int);
int Chord(void *, int, int, int, int, int, int, int, int);
int Ellipse(void *, int, int, int, int);
int LineTo(void *, int, int);
int Pie(void *, int, int, int, int, int, int, int, int);
int PolyBezier(void *, const void *, unsigned);
int PolyPolygon(void *, const void *, const int *, int);
int PolyPolyline(void *, const void *, const unsigned long *, unsigned long);
int Polygon(void *, const void *, int);
int Polyline(void *, const void *, int);
int Rectangle(void *, int, int, int, int);
int RoundRect(void *, int, int, int, int, int, int);
int SetPixel(void *, int, int, unsigned long);
int TextOutA(void *, int, int, const char *, int);
}

#endif  // !_WIN32
