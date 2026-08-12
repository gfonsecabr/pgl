/**
 * @file main.cpp
 * @brief Command line driver for the property harness.
 *
 * Not a doctest binary and not picked up by `tests/run_tests.sh`: a randomized
 * search is a different kind of thing from the deterministic unit suite, and
 * mixing them would put an unbounded, seed-dependent workload in the path of
 * every commit. This one is run on demand — see `tests/property/README.md`.
 */

#include "framework.hpp"
#include "generators.hpp"
#include "properties_algorithms.hpp"
#include "properties_constructions.hpp"
#include "properties_invariance.hpp"
#include "properties_metric.hpp"
#include "properties_predicates.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

/** @brief Builds the registry of every property the harness knows. */
pglprop::Registry buildRegistry() {
    pglprop::Registry registry;
    pglprop::registerPredicateProperties(registry);
    pglprop::registerMetricProperties(registry);
    pglprop::registerInvarianceProperties(registry);
    pglprop::registerConstructionProperties(registry);
    pglprop::registerAlgorithmProperties(registry);
    return registry;
}

void printUsage(std::ostream& out) {
    out << "Usage: property [options]\n"
        << "\n"
        << "  --seed N            Seed the run; the same seed replays it exactly (default 1)\n"
        << "  --cases N           Draws per property family (default 2000)\n"
        << "  --grid N            Draw coordinates from [-N, N] (default 6)\n"
        << "  --group NAME        Only properties whose group contains NAME\n"
        << "  --property NAME     Only properties whose name contains NAME\n"
        << "  --generator NAME    Only generators whose name contains NAME\n"
        << "  --no-shrink         Report the case as drawn, without reducing it\n"
        << "  --no-catch-crashes  Let a failed assertion abort, instead of reporting it\n"
        << "  --shrink-budget N   Rebuild-and-recheck calls per failure (default 600)\n"
        << "  --baseline FILE     Signatures in FILE are reported but do not fail the run\n"
        << "  --update-baseline   Rewrite the baseline from this run's failures\n"
        << "  --max-report N      Witnesses to print per section (default 40)\n"
        << "  --verbose           Also print per-property hold/skip/fail counts\n"
        << "  --list              List the properties and generators, then exit\n"
        << "  --help              This message\n";
}

/** @brief Prints the registry and the generator table. */
void printList(const pglprop::Registry& registry) {
    std::cout << "Properties over one shape:\n";
    for (const pglprop::UnaryProperty& property : registry.unary) {
        std::cout << "  " << property.group << "/" << property.name << "\n";
    }
    std::cout << "\nProperties over a shape pair:\n";
    for (const pglprop::BinaryProperty& property : registry.binary) {
        std::cout << "  " << property.group << "/" << property.name << "\n";
    }
    std::cout << "\nProperties over a point set:\n";
    for (const pglprop::PointSetProperty& property : registry.pointSet) {
        std::cout << "  " << property.group << "/" << property.name << "\n";
    }
    std::cout << "\nGenerators:\n";
    for (const pglprop::Generator& generator : pglprop::generators()) {
        std::cout << "  " << generator.name << "  (" << generator.minPoints << "-"
                  << generator.maxPoints << " points)\n";
    }
}

/**
 * @brief Reads the value of an option, complaining if it is missing.
 *
 * @return `false` when the option was the last argument, so the caller can bail.
 */
bool takeValue(int argc, char** argv, int& index, const char* name, std::string& out) {
    if (index + 1 >= argc) {
        std::cerr << "Option " << name << " needs a value.\n";
        return false;
    }
    out = argv[++index];
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    pglprop::Options options;
    bool listOnly = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        std::string value;

        if (argument == "--help" || argument == "-h") {
            printUsage(std::cout);
            return 0;
        } else if (argument == "--list") {
            listOnly = true;
        } else if (argument == "--no-shrink") {
            options.shrink = false;
        } else if (argument == "--update-baseline") {
            options.updateBaseline = true;
        } else if (argument == "--verbose") {
            options.verbose = true;
        } else if (argument == "--no-catch-crashes") {
            options.catchCrashes = false;
        } else if (argument == "--seed") {
            if (!takeValue(argc, argv, i, "--seed", value)) return 2;
            options.seed = std::strtoull(value.c_str(), nullptr, 10);
        } else if (argument == "--cases") {
            if (!takeValue(argc, argv, i, "--cases", value)) return 2;
            options.cases = std::strtoull(value.c_str(), nullptr, 10);
        } else if (argument == "--grid") {
            if (!takeValue(argc, argv, i, "--grid", value)) return 2;
            options.grid = std::atoi(value.c_str());
        } else if (argument == "--group") {
            if (!takeValue(argc, argv, i, "--group", value)) return 2;
            options.group = value;
        } else if (argument == "--property") {
            if (!takeValue(argc, argv, i, "--property", value)) return 2;
            options.property = value;
        } else if (argument == "--generator") {
            if (!takeValue(argc, argv, i, "--generator", value)) return 2;
            options.generator = value;
        } else if (argument == "--shrink-budget") {
            if (!takeValue(argc, argv, i, "--shrink-budget", value)) return 2;
            options.shrinkBudget = std::atoi(value.c_str());
        } else if (argument == "--baseline") {
            if (!takeValue(argc, argv, i, "--baseline", value)) return 2;
            options.baseline = value;
        } else if (argument == "--max-report") {
            if (!takeValue(argc, argv, i, "--max-report", value)) return 2;
            options.maxReport = static_cast<unsigned>(std::atoi(value.c_str()));
        } else {
            std::cerr << "Unknown option '" << argument << "'.\n\n";
            printUsage(std::cerr);
            return 2;
        }
    }

    if (options.grid < 1) {
        std::cerr << "--grid must be at least 1.\n";
        return 2;
    }

    const pglprop::Registry registry = buildRegistry();

    if (listOnly) {
        printList(registry);
        return 0;
    }

    // A filter that selects nothing must not look like a clean run: checking zero
    // properties trivially finds zero failures, and exiting 0 on a typo in
    // `--property` is how a harness gets quietly switched off.
    std::size_t selected = 0;
    for (const pglprop::UnaryProperty& property : registry.unary) {
        selected += pglprop::detail::matches(options.group, property.group) &&
                    pglprop::detail::matches(options.property, property.name);
    }
    for (const pglprop::BinaryProperty& property : registry.binary) {
        selected += pglprop::detail::matches(options.group, property.group) &&
                    pglprop::detail::matches(options.property, property.name);
    }
    for (const pglprop::PointSetProperty& property : registry.pointSet) {
        selected += pglprop::detail::matches(options.group, property.group) &&
                    pglprop::detail::matches(options.property, property.name);
    }
    if (selected == 0) {
        std::cerr << "No property matches --group '" << options.group << "' and --property '"
                  << options.property << "'. Try --list.\n";
        return 2;
    }

    std::size_t drawable = 0;
    for (const pglprop::Generator& generator : pglprop::generators()) {
        drawable += pglprop::detail::matches(options.generator, generator.name);
    }
    if (drawable == 0) {
        std::cerr << "No generator matches --generator '" << options.generator
                  << "'. Try --list.\n";
        return 2;
    }

    const pglprop::RunReport result = pglprop::run(registry, options);
    return pglprop::report(result, options, std::cout);
}
