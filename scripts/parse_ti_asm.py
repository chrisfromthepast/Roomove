#!/usr/bin/env python3
import argparse
import pathlib
import re
import sys
from typing import List, Sequence, Tuple


def read_text(path: pathlib.Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"ERROR: unable to read '{path}': {exc}", file=sys.stderr)
        raise


def gather_asm_files(asm_files: Sequence[str], asm_dir: str) -> List[pathlib.Path]:
    collected: List[pathlib.Path] = []
    for item in asm_files:
        candidate = pathlib.Path(item).resolve()
        if candidate.is_file() and candidate.suffix.lower() == ".asm":
            collected.append(candidate)
    if asm_dir:
        root = pathlib.Path(asm_dir).resolve()
        if root.exists():
            try:
                collected.extend(sorted(path for path in root.rglob("*.asm") if path.is_file()))
            except OSError as exc:
                print(f"ERROR: unable to scan asm directory '{root}': {exc}", file=sys.stderr)
                raise
    unique = sorted(set(collected))
    return unique


def parse_bounds(text: str) -> List[Tuple[int, int, int]]:
    lcd_bounds = [int(match) for match in re.findall(r"Loop Carried Dependency Bound\s*\(\^\)\s*[:=]\s*(\d+)", text)]
    prb_bounds = [int(match) for match in re.findall(r"Partitioned Resource Bound\s*\(\*\)\s*[:=]\s*(\d+)", text)]
    pair_count = max(len(lcd_bounds), len(prb_bounds))
    triples: List[Tuple[int, int, int]] = []
    for index in range(pair_count):
        lcd_value = lcd_bounds[index] if index < len(lcd_bounds) else 0
        prb_value = prb_bounds[index] if index < len(prb_bounds) else 0
        minimum_cycles = max(lcd_value, prb_value)
        triples.append((lcd_value, prb_value, minimum_cycles))
    return triples


def check_asm_file(path: pathlib.Path) -> int:
    try:
        text = read_text(path)
    except OSError:
        return 2

    violations: List[str] = []
    for marker in ("CALL OCCURS", "BRANCH OCCURS", "Disqualified loop:"):
        if marker in text:
            violations.append(marker)

    bounds = parse_bounds(text)
    if not bounds:
        print(f"WARNING: no software-pipeline bounds found in '{path}'.")
    else:
        for index, (lcd, prb, minimum_cycles) in enumerate(bounds, start=1):
            print(
                f"{path}: loop {index} => Loop Carried Dependency Bound={lcd}, "
                f"Partitioned Resource Bound={prb}, minimum cycles/iteration={minimum_cycles}"
            )

    if violations:
        print(f"FAIL: TI assembly validation failed for '{path}'.", file=sys.stderr)
        for marker in violations:
            print(f"  - found '{marker}'", file=sys.stderr)
        return 1
    return 0


def check_restrict_rule(source_files: Sequence[str]) -> int:
    violation_count = 0
    signature_pattern = re.compile(
        r"(roomoveDspStateProcessAudioNoAlias|processRoomoveAudioNoAlias)\s*\(([^)]*)\)"
    )
    for source in source_files:
        path = pathlib.Path(source).resolve()
        try:
            text = read_text(path)
        except OSError:
            return 2

        matches = list(signature_pattern.finditer(text))
        if not matches:
            print(
                f"FAIL: no no-alias API found in '{path}' for const + AAX_RESTRICT enforcement.",
                file=sys.stderr,
            )
            violation_count += 1
            continue

        for match in matches:
            params = match.group(2)
            if "const float* AAX_RESTRICT inputBuffer" not in params:
                print(
                    f"FAIL: '{match.group(1)}' in '{path}' is missing "
                    "const float* AAX_RESTRICT inputBuffer",
                    file=sys.stderr,
                )
                violation_count += 1
            if "float* AAX_RESTRICT outputBuffer" not in params:
                print(
                    f"FAIL: '{match.group(1)}' in '{path}' is missing "
                    "float* AAX_RESTRICT outputBuffer",
                    file=sys.stderr,
                )
                violation_count += 1
    if violation_count == 0:
        print("PASS: const + AAX_RESTRICT static rule checks passed.")
        return 0
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Parse TI C6000 .asm files for pipeline health.")
    parser.add_argument("--asm-file", action="append", default=[], help="Explicit .asm file path (repeatable).")
    parser.add_argument("--asm-dir", default="", help="Directory to recursively scan for .asm files.")
    parser.add_argument(
        "--source-file",
        action="append",
        default=[],
        help="Source/header files for const + AAX_RESTRICT static checks.",
    )
    parser.add_argument(
        "--allow-missing-asm",
        action="store_true",
        help="Do not fail when no .asm files are found.",
    )
    args = parser.parse_args()

    asm_paths = gather_asm_files(args.asm_file, args.asm_dir)
    if not asm_paths:
        message = "No .asm files found. Compile with TI flags -k and -s to emit assembly."
        if args.allow_missing_asm:
            print(f"WARNING: {message}")
        else:
            print(f"FAIL: {message}", file=sys.stderr)
            return 1

    return_code = 0
    for asm_path in asm_paths:
        file_result = check_asm_file(asm_path)
        if file_result != 0:
            return_code = file_result

    if args.source_file:
        restrict_result = check_restrict_rule(args.source_file)
        if restrict_result != 0 and return_code == 0:
            return_code = restrict_result

    if return_code == 0:
        print("PASS: TI assembly parser checks completed.")
    return return_code


if __name__ == "__main__":
    sys.exit(main())
