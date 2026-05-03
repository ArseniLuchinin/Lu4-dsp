#!/usr/bin/env python3
"""Benchmark runner for ctest-based tests.

Usage:
    python benchmark.py
    python benchmark.py --preset e2e --repeat 3
    python benchmark.py --tests SpectrogramE2E.* QpskMvpTest.MiniE2E.*

Results are appended to a CSV file with columns:
    timestamp, test_name, duration_ms, diff_from_best_ms
"""

import argparse
import csv
import os
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path


DEFAULT_PRESET = "e2e"
DEFAULT_REPEAT = 1


def parse_args():
    parser = argparse.ArgumentParser(description="Run benchmarks and save results to CSV.")
    parser.add_argument(
        "--preset",
        default=DEFAULT_PRESET,
        help=f"CTest preset to use (default: {DEFAULT_PRESET})",
    )
    parser.add_argument(
        "--repeat",
        type=int,
        default=DEFAULT_REPEAT,
        help=f"Repeat each test N times (default: {DEFAULT_REPEAT})",
    )
    parser.add_argument(
        "--csv",
        type=Path,
        help="Path to CSV results file (default: <script_dir>/benchmark_results.csv)",
    )
    parser.add_argument(
        "--tests",
        nargs="+",
        help="Regex patterns for test names to run (default: all tests in preset)",
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        help="Project root directory (default: parent of script directory)",
    )
    return parser.parse_args()


def discover_tests(project_root: Path, preset: str, patterns=None):
    cmd = ["ctest", "--preset", preset, "-N"]
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=project_root)
    if result.returncode != 0:
        print(f"Failed to discover tests: {result.stderr}", file=sys.stderr)
        sys.exit(1)

    tests = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if not line.startswith("Test #"):
            continue
        # Format: "Test #N: TestName"
        if ":" not in line:
            continue
        test_name = line.split(":", 1)[1].strip()
        if patterns:
            if any(re.search(p, test_name) for p in patterns):
                tests.append(test_name)
        else:
            tests.append(test_name)
    return tests


def run_single_test(project_root: Path, preset: str, test_name: str) -> float | None:
    with tempfile.NamedTemporaryFile(suffix=".xml", delete=False) as f:
        junit_path = f.name

    try:
        cmd = [
            "ctest",
            "--preset",
            preset,
            "-R",
            f"^{re.escape(test_name)}$",
            "--output-junit",
            junit_path,
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, cwd=project_root)
        if result.returncode != 0:
            print(f"Test {test_name} failed:\n{result.stderr}", file=sys.stderr)
            return None

        tree = ET.parse(junit_path)
        root = tree.getroot()
        testcase = root.find(".//testcase")
        if testcase is None:
            print(f"No testcase found in JUnit XML for {test_name}", file=sys.stderr)
            return None

        time_sec = float(testcase.get("time", "0"))
        return time_sec
    finally:
        try:
            os.unlink(junit_path)
        except OSError:
            pass


def read_existing_records(csv_path: Path):
    records = []
    if not csv_path.exists():
        return records

    with open(csv_path, "r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            records.append(dict(row))
    return records


def compute_best_times(records):
    best = {}
    for r in records:
        name = r.get("test_name")
        try:
            dur = float(r.get("duration_ms", 0))
        except (ValueError, TypeError):
            continue
        if name not in best or dur < best[name]:
            best[name] = dur
    return best


def write_records(csv_path: Path, records):
    fieldnames = ["timestamp", "test_name", "duration_ms", "diff_from_best_ms"]

    best = {}
    for r in records:
        name = r["test_name"]
        dur = float(r["duration_ms"])
        if name not in best or dur < best[name]:
            best[name] = dur

    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in records:
            diff = float(r["duration_ms"]) - best[r["test_name"]]
            writer.writerow({
                "timestamp": r["timestamp"],
                "test_name": r["test_name"],
                "duration_ms": r["duration_ms"],
                "diff_from_best_ms": f"{diff:.3f}",
            })


def main():
    args = parse_args()

    script_dir = Path(__file__).parent.resolve()
    project_root = args.project_root or script_dir.parent
    csv_path = args.csv or script_dir / "benchmark_results.csv"

    tests = discover_tests(project_root, args.preset, args.tests)
    if not tests:
        print("No tests found to benchmark.", file=sys.stderr)
        sys.exit(1)

    existing = read_existing_records(csv_path)
    new_records = []

    for test_name in tests:
        for i in range(args.repeat):
            run_label = f"{test_name} (run {i + 1}/{args.repeat})" if args.repeat > 1 else test_name
            print(f"Running benchmark: {run_label} ...")
            duration_sec = run_single_test(project_root, args.preset, test_name)
            if duration_sec is None:
                continue

            duration_ms = duration_sec * 1000.0
            timestamp = datetime.now(timezone.utc).isoformat()

            record = {
                "timestamp": timestamp,
                "test_name": test_name,
                "duration_ms": f"{duration_ms:.3f}",
                "diff_from_best_ms": "0.000",  # placeholder, recalculated later
            }
            new_records.append(record)
            print(f"  -> {duration_ms:.3f} ms")

    all_records = existing + new_records
    write_records(csv_path, all_records)
    print(f"\nResults saved to {csv_path} ({len(new_records)} new record(s))")


if __name__ == "__main__":
    main()
