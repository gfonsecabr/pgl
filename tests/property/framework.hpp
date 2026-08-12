#pragma once

/**
 * @file framework.hpp
 * @brief Property registry, the case runner, and failure reporting.
 *
 * ## What a property is
 *
 * A property is a function from one or two generated shapes (or from a point
 * set) to one of three answers:
 *
 *  - **held** — the relation was checked and came out true;
 *  - **failed** — it came out false, with a message naming the values involved;
 *  - **skipped** — the relation says nothing about this input, so nothing was
 *    checked.
 *
 * The third answer is not an implementation detail to be tidied away. Most
 * relations here are conditional (`contains` implies `intersects` *when the
 * operand is non-empty*; the boolean identities hold *for region operands*), and
 * a property that quietly returns "held" for the inputs it cannot judge is
 * indistinguishable from one that works. Keeping skips separate lets the runner
 * report a property that never actually ran — see the vacuity warning in
 * @ref report — which is the failure mode a property harness is most prone to.
 *
 * ## Failure aggregation and the baseline
 *
 * One broken predicate pair fails on thousands of random cases. Reporting all of
 * them buries everything else, so failures are collapsed by *signature* — the
 * property plus the pair of shape alternatives involved — and each signature
 * keeps a count and the single smallest witness found for it.
 *
 * Signatures can then be listed in a baseline file (`known_failures.txt`).
 * Anything listed is reported but does not fail the run; anything unlisted does.
 * That is what makes the harness usable on a library with known open bugs: the
 * baseline is the inventory of what is already understood, and the exit code
 * tracks only what is new.
 */

#include "crash.hpp"
#include "generators.hpp"
#include "rng.hpp"
#include "shrink.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace pglprop {

/** @brief The three answers a property can give. */
enum class Outcome {
    /** @brief The relation was checked and holds. */
    kHeld,
    /** @brief The relation does not apply to this input; nothing was checked. */
    kSkipped,
    /** @brief The relation was checked and does not hold. */
    kFailed,
};

/** @brief A property's answer, with the detail needed to report a failure. */
struct Result {
    /** @brief What happened. */
    Outcome outcome = Outcome::kHeld;
    /** @brief For a failure, the values that violate the relation. */
    std::string detail;
};

/** @brief Builds a "held" result. */
inline Result held() { return Result{Outcome::kHeld, {}}; }

/** @brief Builds a "skipped" result. */
inline Result skipped() { return Result{Outcome::kSkipped, {}}; }

/**
 * @brief Builds a "failed" result.
 *
 * @param detail The concrete values, spelled out. This is the whole content of
 *        the eventual report, so it should say what was compared and what came
 *        back, not merely that something went wrong.
 */
inline Result failure(std::string detail) { return Result{Outcome::kFailed, std::move(detail)}; }

/**
 * @brief Fails the enclosing property unless @p condition holds.
 *
 * The stringified condition is appended to @p message, so the report shows both
 * the values and the relation they were supposed to satisfy.
 */
#define PGLPROP_CHECK(condition, message)                                            \
    do {                                                                             \
        if (!(condition)) {                                                          \
            return ::pglprop::failure(::pglprop::detail::joinCheck((message), #condition)); \
        }                                                                            \
    } while (false)

namespace detail {

/** @brief Formats a `PGLPROP_CHECK` failure detail. */
inline std::string joinCheck(const std::string& message, const char* condition) {
    return message + "  [violated: " + condition + "]";
}

/** @brief Renders any streamable value, for use in failure details. */
template <class T>
std::string show(const T& value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

/** @brief Renders a boolean as a word rather than as `1` or `0`. */
inline std::string show(bool value) { return value ? "true" : "false"; }

}  // namespace detail

/** @brief A property of a single shape. */
struct UnaryProperty {
    /** @brief Group name, used by `--group`. */
    const char* group;
    /** @brief Property name, used by `--property` and in reports. */
    const char* name;
    /** @brief Generator tags the operand must carry, or @ref kNoTag for any. */
    unsigned requiredTags;
    /** @brief The check. */
    Result (*run)(const AnyShape&);
};

/** @brief A property of an ordered pair of shapes. */
struct BinaryProperty {
    /** @brief Group name, used by `--group`. */
    const char* group;
    /** @brief Property name, used by `--property` and in reports. */
    const char* name;
    /** @brief Generator tags *both* operands must carry, or @ref kNoTag. */
    unsigned requiredTags;
    /** @brief The check. */
    Result (*run)(const AnyShape&, const AnyShape&);
};

/**
 * @brief A property of a raw point set.
 *
 * The algorithm-level properties take a point set rather than shapes: a convex
 * hull, a segment sweep, a triangulation and an arrangement are all naturally
 * driven by one, and it shrinks with the same machinery.
 */
struct PointSetProperty {
    /** @brief Group name, used by `--group`. */
    const char* group;
    /** @brief Property name, used by `--property` and in reports. */
    const char* name;
    /** @brief Fewest points the check needs. */
    std::size_t minPoints;
    /** @brief Most points to hand it. */
    std::size_t maxPoints;
    /** @brief The check. */
    Result (*run)(const std::vector<PointShape>&);
};

/** @brief Everything the runner knows how to check. */
struct Registry {
    /** @brief Registered single-shape properties. */
    std::vector<UnaryProperty> unary;
    /** @brief Registered shape-pair properties. */
    std::vector<BinaryProperty> binary;
    /** @brief Registered point-set properties. */
    std::vector<PointSetProperty> pointSet;
};

/** @brief How the run is configured; filled in from the command line. */
struct Options {
    /** @brief Seed for the whole run; `--seed`. */
    std::uint64_t seed = 1;
    /** @brief Number of shape-pair draws; `--cases`. */
    unsigned long long cases = 2000;
    /** @brief Coordinates are drawn from `[-grid, grid]`; `--grid`. */
    int grid = 6;
    /** @brief Substring filter on group names; empty accepts all. */
    std::string group;
    /** @brief Substring filter on property names; empty accepts all. */
    std::string property;
    /** @brief Substring filter on generator names; empty accepts all. */
    std::string generator;
    /** @brief Whether to reduce failing cases before reporting them. */
    bool shrink = true;
    /** @brief `stillFails` calls allowed per failure; `--shrink-budget`. */
    int shrinkBudget = 600;
    /** @brief Path of the known-failure baseline; empty disables it. */
    std::string baseline;
    /** @brief Rewrite the baseline from this run instead of checking it. */
    bool updateBaseline = false;
    /** @brief Print every signature's witness, not just the first few. */
    unsigned maxReport = 40;
    /** @brief Print per-property statistics even when everything holds. */
    bool verbose = false;
    /**
     * @brief Catch a fatal signal and report it instead of dying.
     *
     * On by default: a reachable `assert` inside the library would otherwise end
     * the run at the first one found. Turn it off to run a reduced witness under
     * a debugger. See `crash.hpp`.
     */
    bool catchCrashes = true;
};

/** @brief One collapsed failure: a signature, a count, and the best witness. */
struct FailureRecord {
    /** @brief `group/name [Alternative,Alternative]`. */
    std::string signature;
    /** @brief Property name alone, for the reproduction hint. */
    std::string property;
    /** @brief The failing values, from the property that rejected them. */
    std::string detail;
    /** @brief Reproduction recipes for the operands, smallest found. */
    std::vector<std::string> operands;
    /** @brief How many random cases hit this signature. */
    unsigned long long count = 0;
    /** @brief Size of the retained witness, so a smaller one can replace it. */
    Complexity measure;
};

/** @brief Hold/skip/fail tallies for one property. */
struct PropertyStats {
    /** @brief Times the relation was checked and held. */
    unsigned long long held = 0;
    /** @brief Times the input was outside the relation's domain. */
    unsigned long long skipped = 0;
    /** @brief Times the relation was violated. */
    unsigned long long failed = 0;
};

/**
 * @brief Accumulates everything a run produces.
 */
struct RunReport {
    /** @brief Collapsed failures, keyed by signature. */
    std::map<std::string, FailureRecord> failures;
    /** @brief Per-property tallies, keyed by `group/name`. */
    std::map<std::string, PropertyStats> stats;
    /** @brief Ordered alternative pairs the run actually produced. */
    std::set<std::string> pairsSeen;
    /** @brief Cases the generators refused, i.e. undefined or invalid draws. */
    unsigned long long rejectedDraws = 0;
};

namespace detail {

/** @brief Tests a substring filter; an empty filter accepts everything. */
inline bool matches(const std::string& filter, const char* text) {
    return filter.empty() || std::string(text).find(filter) != std::string::npos;
}

/** @brief Reads a baseline file into a set of signatures. */
inline std::set<std::string> readBaseline(const std::string& path) {
    std::set<std::string> signatures;
    std::ifstream input(path);
    if (!input) {
        return signatures;
    }
    std::string line;
    while (std::getline(input, line)) {
        // Trailing whitespace and comments are stripped so the file can be
        // annotated -- a bug inventory is worth annotating.
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) {
            line.erase(hash);
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.pop_back();
        }
        if (!line.empty()) {
            signatures.insert(line);
        }
    }
    return signatures;
}

/**
 * @brief Whether a signature is covered by the baseline.
 *
 * Matched two ways. An exact line `group/property [A,B]` accepts that one pair.
 * A line ending in `[*]` accepts every pair of that property, which is what a
 * *root cause* usually needs: one missing overload or one mishandled degenerate
 * shape fails the same property across dozens of pairs, and which pairs a given
 * seed happens to reach is an accident of the draw. Without the wildcard every
 * fresh seed reports a handful of new signatures that are not new problems, and
 * the exit code stops meaning anything.
 *
 * The choice between the two forms is a judgement about a property, recorded in
 * the baseline: wildcard where the pair is incidental to a triaged cause, exact
 * pairs where a new pair would more likely be a new bug.
 */
inline bool isKnownFailure(const std::set<std::string>& baseline, const std::string& signature) {
    if (baseline.count(signature) != 0) {
        return true;
    }
    const std::size_t bracket = signature.rfind(" [");
    if (bracket == std::string::npos) {
        return false;
    }
    return baseline.count(signature.substr(0, bracket) + " [*]") != 0;
}

/** @brief Records one failure into the report, keeping the smallest witness. */
inline void recordFailure(RunReport& report, const std::string& signature,
                          const std::string& property, const std::string& failureDetail,
                          const std::vector<std::string>& operands, const Complexity& measure) {
    FailureRecord& record = report.failures[signature];
    if (record.count == 0 || measure < record.measure) {
        record.signature = signature;
        record.property = property;
        record.detail = failureDetail;
        record.operands = operands;
        record.measure = measure;
    }
    ++record.count;
}

/** @brief Whether property evaluation runs under the fatal-signal jump target. */
inline bool& crashCatchingEnabled() {
    static bool enabled = true;
    return enabled;
}

/**
 * @brief Runs a property, turning any abnormal exit into a reported failure.
 *
 * Three things can go wrong, and all three become failures rather than ending
 * the run:
 *  - the property returns a failure, which is the ordinary case;
 *  - an operation throws where the property did not expect it to. A property that
 *    *expects* a throw catches it and skips (a documented throw is not a
 *    violation), so anything arriving here is a surprise and worth reporting;
 *  - the process takes a fatal signal, nearly always a failed `assert` inside the
 *    library. See `crash.hpp` for why that is caught and what it costs.
 */
template <class Invoke>
Result guarded(Invoke&& invoke) {
    if (crashCatchingEnabled()) {
        installCrashHandlers();
        if (sigsetjmp(crashJumpTarget(), 1) != 0) {
            // Re-entered from the signal handler: the call never returned.
            return failure("crashed with " + crashSignalName(crashSignal()));
        }
        crashArmed() = 1;
    }

    Result result;
    try {
        result = invoke();
    } catch (const std::exception& error) {
        crashArmed() = 0;
        return failure(std::string("threw ") + error.what());
    } catch (...) {
        crashArmed() = 0;
        return failure("threw a non-std exception");
    }
    crashArmed() = 0;
    return result;
}

}  // namespace detail

/**
 * @brief Draws cases and checks every applicable property against them.
 *
 * One pass draws a pair of operands and runs the unary properties on each and
 * the binary properties on the ordered pair. Drawing once and checking many
 * relations is what keeps the cost per relation low; drawing the pair
 * independently each time is what makes both operand orders show up without the
 * runner having to try each pair twice.
 *
 * @param registry Properties to check.
 * @param options Filters, counts and shrinking policy.
 * @return The accumulated report; the caller decides the exit status from it.
 */
inline RunReport run(const Registry& registry, const Options& options) {
    RunReport report;
    Rng rng(options.seed);

    detail::crashCatchingEnabled() = options.catchCrashes;
    // A failed assertion writes to stderr before aborting, and the shrinker will
    // re-run a crashing case hundreds of times. Released before the report is
    // printed, so later diagnostics still reach the terminal.
    const detail::StderrSilencer silencer(options.catchCrashes);

    // Resolve the generator filter once.
    std::vector<std::size_t> enabled;
    for (std::size_t i = 0; i < generators().size(); ++i) {
        if (detail::matches(options.generator, generators()[i].name)) {
            enabled.push_back(i);
        }
    }
    if (enabled.empty()) {
        std::cerr << "No generator matches '" << options.generator << "'.\n";
        return report;
    }

    // Select the properties once, so the per-case loop does no string work.
    std::vector<const UnaryProperty*> unary;
    for (const UnaryProperty& property : registry.unary) {
        if (detail::matches(options.group, property.group) &&
            detail::matches(options.property, property.name)) {
            unary.push_back(&property);
        }
    }
    std::vector<const BinaryProperty*> binary;
    for (const BinaryProperty& property : registry.binary) {
        if (detail::matches(options.group, property.group) &&
            detail::matches(options.property, property.name)) {
            binary.push_back(&property);
        }
    }
    std::vector<const PointSetProperty*> pointSet;
    for (const PointSetProperty& property : registry.pointSet) {
        if (detail::matches(options.group, property.group) &&
            detail::matches(options.property, property.name)) {
            pointSet.push_back(&property);
        }
    }

    const auto tally = [&report](const char* group, const char* name, Outcome outcome) {
        PropertyStats& stats = report.stats[std::string(group) + "/" + name];
        switch (outcome) {
            case Outcome::kHeld: ++stats.held; break;
            case Outcome::kSkipped: ++stats.skipped; break;
            case Outcome::kFailed: ++stats.failed; break;
        }
    };

    // ---------------------------------------------------------------- shapes
    if (!unary.empty() || !binary.empty()) {
        for (unsigned long long iteration = 0; iteration < options.cases; ++iteration) {
            const Operand operandA = drawOperand(rng, enabled[rng.index(enabled.size())], options.grid);
            const Operand operandB = drawOperand(rng, enabled[rng.index(enabled.size())], options.grid);

            AnyShape shapeA;
            AnyShape shapeB;
            if (!buildOperand(operandA, shapeA) || !buildOperand(operandB, shapeB)) {
                ++report.rejectedDraws;
                continue;
            }

            const std::string nameA = alternativeName(shapeA);
            const std::string nameB = alternativeName(shapeB);
            report.pairsSeen.insert(nameA + "," + nameB);

            // A failing property is re-run on shrunk candidates through one of
            // these, which rebuild the operands from the candidate point lists.
            // The property pointer is captured by value: the returned closure
            // outlives the factory call that produced it.
            const auto unaryStillFails = [&operandA, &operandB](const UnaryProperty* property,
                                                               int which) {
                return [property, which, &operandA, &operandB](const PointLists& lists) {
                    Operand candidate = which == 0 ? operandA : operandB;
                    candidate.points = lists[0];
                    AnyShape rebuilt;
                    if (!buildOperand(candidate, rebuilt)) {
                        return false;
                    }
                    return detail::guarded([&] { return property->run(rebuilt); }).outcome ==
                           Outcome::kFailed;
                };
            };
            const auto binaryStillFails = [&operandA, &operandB](const BinaryProperty* property) {
                return [property, &operandA, &operandB](const PointLists& lists) {
                    Operand candidateA = operandA;
                    Operand candidateB = operandB;
                    candidateA.points = lists[0];
                    candidateB.points = lists[1];
                    AnyShape rebuiltA;
                    AnyShape rebuiltB;
                    if (!buildOperand(candidateA, rebuiltA) || !buildOperand(candidateB, rebuiltB)) {
                        return false;
                    }
                    return detail::guarded([&] { return property->run(rebuiltA, rebuiltB); })
                               .outcome == Outcome::kFailed;
                };
            };

            for (const UnaryProperty* property : unary) {
                for (int which = 0; which < 2; ++which) {
                    const Operand& operand = which == 0 ? operandA : operandB;
                    const AnyShape& shape = which == 0 ? shapeA : shapeB;
                    const std::string& name = which == 0 ? nameA : nameB;

                    if ((generators()[operand.generator].tags & property->requiredTags) !=
                        property->requiredTags) {
                        continue;
                    }

                    const Result result = detail::guarded([&] { return property->run(shape); });
                    tally(property->group, property->name, result.outcome);
                    if (result.outcome != Outcome::kFailed) {
                        continue;
                    }

                    PointLists lists{operand.points};
                    const std::vector<std::size_t> minSizes{
                        generators()[operand.generator].minPoints};
                    if (options.shrink) {
                        lists = shrink(lists, minSizes, unaryStillFails(property, which),
                                       options.shrinkBudget);
                    }

                    // Re-run on the reduced case so the reported detail
                    // describes the witness being printed, not the original.
                    Operand reduced = operand;
                    reduced.points = lists[0];
                    AnyShape reducedShape;
                    std::string reducedDetail = result.detail;
                    if (buildOperand(reduced, reducedShape)) {
                        const Result again =
                            detail::guarded([&] { return property->run(reducedShape); });
                        if (again.outcome == Outcome::kFailed) {
                            reducedDetail = again.detail;
                        }
                    }

                    detail::recordFailure(
                        report, std::string(property->group) + "/" + property->name + " [" + name + "]",
                        property->name, reducedDetail, {describeOperand(reduced)},
                        complexityOf(lists));
                }
            }

            for (const BinaryProperty* property : binary) {
                const unsigned tagsA = generators()[operandA.generator].tags;
                const unsigned tagsB = generators()[operandB.generator].tags;
                if ((tagsA & property->requiredTags) != property->requiredTags ||
                    (tagsB & property->requiredTags) != property->requiredTags) {
                    continue;
                }

                const Result result =
                    detail::guarded([&] { return property->run(shapeA, shapeB); });
                tally(property->group, property->name, result.outcome);
                if (result.outcome != Outcome::kFailed) {
                    continue;
                }

                PointLists lists{operandA.points, operandB.points};
                const std::vector<std::size_t> minSizes{
                    generators()[operandA.generator].minPoints,
                    generators()[operandB.generator].minPoints};
                if (options.shrink) {
                    lists = shrink(lists, minSizes, binaryStillFails(property), options.shrinkBudget);
                }

                Operand reducedA = operandA;
                Operand reducedB = operandB;
                reducedA.points = lists[0];
                reducedB.points = lists[1];
                AnyShape reducedShapeA;
                AnyShape reducedShapeB;
                std::string reducedDetail = result.detail;
                if (buildOperand(reducedA, reducedShapeA) && buildOperand(reducedB, reducedShapeB)) {
                    const Result again = detail::guarded(
                        [&] { return property->run(reducedShapeA, reducedShapeB); });
                    if (again.outcome == Outcome::kFailed) {
                        reducedDetail = again.detail;
                    }
                }

                detail::recordFailure(report,
                                      std::string(property->group) + "/" + property->name + " [" +
                                          nameA + "," + nameB + "]",
                                      property->name, reducedDetail,
                                      {describeOperand(reducedA), describeOperand(reducedB)},
                                      complexityOf(lists));
            }
        }
    }

    // ------------------------------------------------------------ point sets
    for (const PointSetProperty* property : pointSet) {
        for (unsigned long long iteration = 0; iteration < options.cases; ++iteration) {
            const std::size_t count = property->minPoints +
                rng.index(property->maxPoints - property->minPoints + 1);
            std::vector<PointShape> points;
            points.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                // Named locals, not two arguments of one call: see drawOperand.
                const int x = rng.inRange(-options.grid, options.grid);
                const int y = rng.inRange(-options.grid, options.grid);
                points.emplace_back(x, y);
            }

            const Result result = detail::guarded([&] { return property->run(points); });
            tally(property->group, property->name, result.outcome);
            if (result.outcome != Outcome::kFailed) {
                continue;
            }

            PointLists lists{points};
            if (options.shrink) {
                lists = shrink(
                    lists, {property->minPoints},
                    [property](const PointLists& candidate) {
                        const Result again =
                            detail::guarded([&] { return property->run(candidate[0]); });
                        return again.outcome == Outcome::kFailed;
                    },
                    options.shrinkBudget);
            }

            std::string reducedDetail = result.detail;
            const Result again = detail::guarded([&] { return property->run(lists[0]); });
            if (again.outcome == Outcome::kFailed) {
                reducedDetail = again.detail;
            }

            std::string recipe = "Points{";
            for (std::size_t i = 0; i < lists[0].size(); ++i) {
                if (i > 0) {
                    recipe += ", ";
                }
                recipe += "(" + std::to_string(lists[0][i].x()) + "," +
                          std::to_string(lists[0][i].y()) + ")";
            }
            recipe += "}";

            detail::recordFailure(report,
                                  std::string(property->group) + "/" + property->name,
                                  property->name, reducedDetail, {recipe}, complexityOf(lists));
        }
    }

    return report;
}

/**
 * @brief Prints the report and returns the process exit status.
 *
 * New failures — signatures absent from the baseline — are what set the status.
 * Known ones are still printed, because a known bug that changes its witness or
 * its frequency is worth seeing, but they do not fail the run.
 *
 * @param report What the run produced.
 * @param options Baseline path and reporting limits.
 * @param out Stream to print to.
 * @return `0` when nothing new failed, `1` otherwise.
 */
inline int report(const RunReport& report, const Options& options, std::ostream& out) {
    const std::set<std::string> baseline =
        options.baseline.empty() ? std::set<std::string>{} : detail::readBaseline(options.baseline);

    std::vector<const FailureRecord*> newFailures;
    std::vector<const FailureRecord*> knownFailures;
    for (const auto& [signature, record] : report.failures) {
        if (detail::isKnownFailure(baseline, signature)) {
            knownFailures.push_back(&record);
        } else {
            newFailures.push_back(&record);
        }
    }

    const auto printRecords = [&](const char* heading,
                                  const std::vector<const FailureRecord*>& records) {
        if (records.empty()) {
            return;
        }
        out << "\n" << heading << " (" << records.size() << " signature"
            << (records.size() == 1 ? "" : "s") << ")\n";
        unsigned printed = 0;
        for (const FailureRecord* record : records) {
            if (printed++ >= options.maxReport) {
                out << "  ... " << (records.size() - printed + 1) << " more suppressed"
                    << " (raise --max-report to see them)\n";
                break;
            }
            out << "\n  " << record->signature << "  (" << record->count << " case"
                << (record->count == 1 ? "" : "s") << ")\n";
            for (const std::string& operand : record->operands) {
                out << "      " << operand << "\n";
            }
            out << "      " << record->detail << "\n";
            out << "      reproduce: --seed " << options.seed << " --property \""
                << record->property << "\"\n";
        }
    };

    printRecords("NEW FAILURES", newFailures);
    printRecords("KNOWN FAILURES (listed in the baseline)", knownFailures);

    // A property that only ever skipped checked nothing at all. That is a bug in
    // the property or in the generator mix, and it is invisible unless said out
    // loud -- an all-skip property is otherwise indistinguishable from a passing
    // one.
    std::vector<std::string> vacuous;
    for (const auto& [name, stats] : report.stats) {
        if (stats.held == 0 && stats.failed == 0 && stats.skipped > 0) {
            vacuous.push_back(name);
        }
    }
    if (!vacuous.empty()) {
        out << "\nVACUOUS (skipped every case, so nothing was checked)\n";
        for (const std::string& name : vacuous) {
            out << "  " << name << "\n";
        }
    }

    if (options.verbose) {
        out << "\nPER-PROPERTY COUNTS (held / skipped / failed)\n";
        for (const auto& [name, stats] : report.stats) {
            out << "  " << stats.held << " / " << stats.skipped << " / " << stats.failed << "  "
                << name << "\n";
        }
    }

    unsigned long long held = 0;
    unsigned long long skippedCount = 0;
    unsigned long long failedCount = 0;
    for (const auto& [name, stats] : report.stats) {
        held += stats.held;
        skippedCount += stats.skipped;
        failedCount += stats.failed;
    }

    out << "\nSummary: " << report.stats.size() << " properties, " << held << " held, "
        << skippedCount << " skipped, " << failedCount << " violated across "
        << report.failures.size() << " signature" << (report.failures.size() == 1 ? "" : "s")
        << " (" << newFailures.size() << " new, " << knownFailures.size() << " known)\n";
    out << "         " << report.pairsSeen.size() << " ordered alternative pairs exercised, "
        << report.rejectedDraws << " draws rejected as undefined or invalid\n";
    out << "         seed " << options.seed << ", " << options.cases << " cases, grid ±"
        << options.grid << "\n";

    if (options.updateBaseline && !options.baseline.empty()) {
        std::ofstream output(options.baseline);
        if (!output) {
            out << "Could not write baseline " << options.baseline << "\n";
            return 1;
        }
        output << "# Known property violations, one signature per line.\n"
               << "# Regenerate with: sh tests/property/run.sh --cases 20000 --update-baseline\n"
               << "# A signature matched here is reported but does not fail the run.\n"
               << "# Exact pairs only: any `[*]` wildcards from the previous file are gone,\n"
               << "# and have to be reapplied by hand. See tests/property/README.md.\n";
        for (const auto& [signature, record] : report.failures) {
            output << signature << "\n";
        }
        out << "Wrote " << report.failures.size() << " signatures to " << options.baseline << "\n";
        return 0;
    }

    return newFailures.empty() ? 0 : 1;
}

}  // namespace pglprop
