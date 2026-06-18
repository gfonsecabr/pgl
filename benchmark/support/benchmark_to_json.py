#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import statistics
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Measurement:
    suite: str
    operation: str
    number: str
    result: str
    value: float
    source: str
    compiler: str
    cxxflags: str
    cpu: str
    commit: str = "unknown"


def parse_metadata(line: str, metadata: dict[str, str]) -> bool:
    if not line.startswith("#"):
        return False

    key, separator, value = line[1:].partition(":")
    if separator:
        metadata[key.strip()] = value.strip()
    return True


def parse_measurement(line: str, metadata: dict[str, str]) -> Measurement | None:
    stripped = line.strip()
    if not stripped or stripped.startswith("Operation"):
        return None

    # Columns are tab-separated; benchmarks pad with runs of tabs for alignment,
    # so drop the empty fields those produce. Splitting on tabs (not arbitrary
    # whitespace) keeps operation and number labels that contain spaces intact,
    # e.g. "build (Delaunay)" or "Rational BigInt".
    parts = [field for field in stripped.split("\t") if field]
    if len(parts) < 4:
        return None

    try:
        value = float(parts[-1])
    except ValueError:
        return None

    suite = metadata.get("suite", "unknown")
    source = metadata.get("source", "unknown")
    compiler = metadata.get("compiler", "unknown")
    cxxflags = metadata.get("cxxflags", "unknown")
    cpu = metadata.get("cpu", "unknown")
    commit = metadata.get("commit", "unknown")
    return Measurement(
        suite=suite,
        operation=parts[0],
        number=" ".join(parts[1:-2]),
        result=parts[-2],
        value=value,
        source=source,
        compiler=compiler,
        cxxflags=cxxflags,
        cpu=cpu,
        commit=commit,
    )


def read_report(path: Path) -> list[Measurement]:
    metadata: dict[str, str] = {}
    measurements: list[Measurement] = []

    with path.open(encoding="utf-8") as report:
        for line in report:
            if parse_metadata(line, metadata):
                continue
            measurement = parse_measurement(line, metadata)
            if measurement is not None:
                measurements.append(measurement)

    return measurements


def aggregate(measurements: list[Measurement]) -> list[dict[str, object]]:
    grouped: dict[tuple[str, str, str], list[Measurement]] = {}
    for measurement in measurements:
        key = (measurement.suite, measurement.operation, measurement.number)
        grouped.setdefault(key, []).append(measurement)

    entries: list[dict[str, object]] = []
    for key in sorted(grouped):
        suite, operation, number = key
        samples = grouped[key]
        values = [sample.value for sample in samples]
        median = statistics.median(values)
        minimum = min(values)
        maximum = max(values)
        result_values = sorted({sample.result for sample in samples})
        sources = sorted({sample.source for sample in samples})
        compilers = sorted({sample.compiler for sample in samples})
        cxxflags_values = sorted({sample.cxxflags for sample in samples})
        cpus = sorted({sample.cpu for sample in samples})

        extra_lines = [
            f"Suite: {suite}",
            f"Operation: {operation}",
            f"Number type: {number}",
            f"Samples: {len(values)}",
            f"Median: {median:.6g} ns/pair",
            f"Min: {minimum:.6g} ns/pair",
            f"Max: {maximum:.6g} ns/pair",
            f"Result counter: {', '.join(result_values)}",
            f"Source: {', '.join(sources)}",
            f"Compiler: {', '.join(compilers)}",
            f"CXXFLAGS: {' | '.join(cxxflags_values)}",
            f"CPU: {', '.join(cpus)}",
        ]

        entry: dict[str, object] = {
            "name": f"{suite}: {operation} ({number})",
            "unit": "ns/pair",
            "value": round(median, 6),
            "extra": "\n".join(extra_lines),
        }
        if len(values) > 1:
            entry["range"] = f"{(maximum - minimum):.6g}"
        entries.append(entry)

    return entries


def write_summary(path: Path, entries: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as summary:
        summary.write("### PGL Benchmarks\n\n")
        summary.write(f"{len(entries)} benchmark series were collected.\n\n")
        summary.write("| Benchmark | Median | Range |\n")
        summary.write("| --- | ---: | ---: |\n")
        for entry in entries:
            value = entry["value"]
            value_text = f"{value:g}" if isinstance(value, float) else str(value)
            range_text = str(entry.get("range", "-"))
            summary.write(f"| {entry['name']} | {value_text} ns/pair | {range_text} |\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert PGL benchmark reports to github-action-benchmark JSON.")
    parser.add_argument("--reports", default="build/benchmark/current-reports", help="Directory containing benchmark report .txt files.")
    parser.add_argument("--output", default="build/benchmark/benchmark.json", help="JSON file to write.")
    parser.add_argument("--summary", default="build/benchmark/benchmark.md", help="Markdown summary file to write.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.reports)
    output_path = Path(args.output)
    summary_path = Path(args.summary)

    measurements: list[Measurement] = []
    for report_path in sorted(report_dir.glob("*.txt")):
        measurements.extend(read_report(report_path))

    if not measurements:
        raise SystemExit(f"No benchmark measurements found in {report_dir}")

    entries = aggregate(measurements)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as output:
        json.dump(entries, output, indent=2)
        output.write("\n")

    write_summary(summary_path, entries)
    print(f"Converted {len(measurements)} measurements into {len(entries)} benchmark series")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
