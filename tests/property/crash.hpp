#pragma once

/**
 * @file crash.hpp
 * @brief Turning a fatal signal into an ordinary reported failure.
 *
 * A geometry library's preconditions are enforced by `assert`, and a randomized
 * harness reaches them — that is much of the point of running one. But a failed
 * assertion calls `abort()`, and an abort ends the process: without protection
 * the *first* reachable assertion stops the run, and everything the harness
 * would have found afterwards stays unfound. The same goes for a segfault from
 * an out-of-range access that an assertion was supposed to have caught.
 *
 * So each property evaluation runs under a jump target. `SIGABRT`, `SIGSEGV`,
 * `SIGFPE` and `SIGBUS` are caught, and the handler jumps back to the runner,
 * which records the case as a failure like any other — with the shrinker then
 * reducing it to a minimal crashing witness, which is exactly what one wants to
 * paste into a bug report.
 *
 * ## What this costs
 *
 * Jumping out of a signal handler abandons the stack between the jump target and
 * the fault without running destructors, so whatever the faulting call had
 * allocated leaks. That is acceptable here and nowhere else: the leak is bounded
 * by the number of distinct crashes found, and pgl keeps no global mutable state
 * for a half-finished call to corrupt — every shape is a value, and the next case
 * builds its own from scratch. A harness process that leaks a few kilobytes per
 * crash and keeps testing is worth more than a clean one that stops.
 *
 * Pass `--no-catch-crashes` to turn this off, which is what one wants when
 * running the reduced witness under a debugger: then the abort is a real abort,
 * and the stack is still there to look at.
 */

#include <csetjmp>
#include <csignal>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#define PGLPROP_HAVE_POSIX 1
#else
#define PGLPROP_HAVE_POSIX 0
#endif

namespace pglprop {

namespace detail {

/** @brief Jump target re-entered when a property faults. */
inline sigjmp_buf& crashJumpTarget() {
    static sigjmp_buf target;
    return target;
}

/** @brief Whether a jump target is currently armed. */
inline volatile std::sig_atomic_t& crashArmed() {
    static volatile std::sig_atomic_t armed = 0;
    return armed;
}

/** @brief Signal number of the fault last caught. */
inline volatile std::sig_atomic_t& crashSignal() {
    static volatile std::sig_atomic_t number = 0;
    return number;
}

/**
 * @brief Jumps back to the armed target.
 *
 * Async-signal-safe: it writes two `sig_atomic_t` flags and jumps. When no
 * target is armed it restores the default disposition and re-raises, so a fault
 * outside a property still crashes the way it would have.
 */
inline void crashHandler(int number) {
    if (crashArmed() == 0) {
        std::signal(number, SIG_DFL);
        std::raise(number);
        return;
    }
    crashArmed() = 0;
    crashSignal() = number;
    siglongjmp(crashJumpTarget(), 1);
}

/** @brief Installs the handlers once. */
inline void installCrashHandlers() {
    static bool installed = false;
    if (installed) {
        return;
    }
    installed = true;
    std::signal(SIGABRT, crashHandler);
    std::signal(SIGSEGV, crashHandler);
    std::signal(SIGFPE, crashHandler);
#ifdef SIGBUS
    std::signal(SIGBUS, crashHandler);
#endif
}

/** @brief Names a caught signal for the failure detail. */
inline std::string crashSignalName(int number) {
    switch (number) {
        case SIGABRT: return "SIGABRT (a failed assertion, most likely)";
        case SIGSEGV: return "SIGSEGV";
        case SIGFPE: return "SIGFPE";
#ifdef SIGBUS
        case SIGBUS: return "SIGBUS";
#endif
        default: return "signal " + std::to_string(number);
    }
}

/**
 * @brief Silences file descriptor 2 for its lifetime.
 *
 * A failed assertion prints to `stderr` before aborting, and the shrinker
 * re-runs a crashing case hundreds of times — so without this the report is
 * buried under thousands of copies of the same assertion message. The harness's
 * own output goes to `stdout` and is unaffected; the report is printed after the
 * silencer has been released, so a later diagnostic is still visible.
 */
class StderrSilencer {
public:
    /**
     * @brief Redirects `stderr` to the null device.
     *
     * @param active Pass `false` to leave `stderr` alone.
     */
    explicit StderrSilencer(bool active) {
#if PGLPROP_HAVE_POSIX
        if (!active) {
            return;
        }
        saved_ = ::dup(2);
        const int null = ::open("/dev/null", O_WRONLY);
        if (null >= 0) {
            ::dup2(null, 2);
            ::close(null);
        }
#else
        (void)active;
#endif
    }

    /** @brief Restores `stderr`. */
    ~StderrSilencer() {
#if PGLPROP_HAVE_POSIX
        if (saved_ >= 0) {
            ::dup2(saved_, 2);
            ::close(saved_);
        }
#endif
    }

    StderrSilencer(const StderrSilencer&) = delete;
    StderrSilencer& operator=(const StderrSilencer&) = delete;

private:
#if PGLPROP_HAVE_POSIX
    int saved_ = -1;
#endif
};

}  // namespace detail

}  // namespace pglprop
