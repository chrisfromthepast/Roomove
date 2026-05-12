#!/usr/bin/env python3
import argparse
import re
import sys
from dataclasses import dataclass
from typing import List, Optional, Tuple


@dataclass
class FunctionBody:
    name: str
    body: str
    start_line: int


FORBIDDEN_PATTERNS: Tuple[Tuple[str, str], ...] = (
    (r"\bnew\b", "dynamic allocation with new"),
    (r"\bdelete\b", "dynamic deallocation with delete"),
    (r"\bmalloc\s*\(", "dynamic allocation with malloc"),
    (r"\bcalloc\s*\(", "dynamic allocation with calloc"),
    (r"\brealloc\s*\(", "dynamic allocation with realloc"),
    (r"\bfree\s*\(", "dynamic deallocation with free"),
    (r"\.push_back\s*\(", "container growth with push_back"),
    (r"\.emplace_back\s*\(", "container growth with emplace_back"),
    (r"\.resize\s*\(", "container resize"),
    (r"\.reserve\s*\(", "container reserve"),
)


def _read_text(path: str) -> str:
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        print(f"ERROR: unable to read file '{path}': {exc}", file=sys.stderr)
        raise


def _find_matching_brace(text: str, open_index: int) -> Optional[int]:
    depth = 0
    for index in range(open_index, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


def _extract_function_body(text: str, signature_regex: str) -> Optional[FunctionBody]:
    match = re.search(signature_regex, text, flags=re.MULTILINE)
    if match is None:
        return None

    open_brace = text.find("{", match.end() - 1)
    if open_brace < 0:
        return None

    close_brace = _find_matching_brace(text, open_brace)
    if close_brace is None:
        return None

    start_line = text.count("\n", 0, open_brace) + 1
    body = text[open_brace + 1 : close_brace]
    return FunctionBody(name=match.group(1), body=body, start_line=start_line)


def _extract_named_functions(text: str) -> List[FunctionBody]:
    functions: List[FunctionBody] = []
    pattern = re.compile(
        r"^[\w:\<\>\s\*&]+?\b(\w+)\s*\([^;{}]*\)\s*(?:const\s*)?\{",
        flags=re.MULTILINE,
    )
    for match in pattern.finditer(text):
        name = match.group(1)
        open_brace = text.find("{", match.end() - 1)
        close_brace = _find_matching_brace(text, open_brace)
        if close_brace is None:
            continue
        start_line = text.count("\n", 0, open_brace) + 1
        body = text[open_brace + 1 : close_brace]
        functions.append(FunctionBody(name=name, body=body, start_line=start_line))
    return functions


def _scan_forbidden(function: FunctionBody, violations: List[str]) -> None:
    for expression, description in FORBIDDEN_PATTERNS:
        if re.search(expression, function.body):
            violations.append(
                f"{function.name} (line {function.start_line}) uses {description}"
            )


def run(plugin_processor_path: str) -> int:
    try:
        source = _read_text(plugin_processor_path)
    except OSError:
        return 2

    violations: List[str] = []
    process_block = _extract_function_body(
        source,
        r"^\s*void\s+[\w:]+::(\w+)\s*\(\s*juce::AudioBuffer<float>&\s*buffer\s*,\s*juce::MidiBuffer&\s*\)\s*",
    )
    if process_block is None:
        print("ERROR: processBlock was not found in PluginProcessor.cpp", file=sys.stderr)
        return 2

    if process_block.name != "processBlock":
        print(
            f"ERROR: expected processBlock function, found '{process_block.name}'",
            file=sys.stderr,
        )
        return 2

    _scan_forbidden(process_block, violations)
    if "juce::ScopedNoDenormals" not in process_block.body:
        violations.append(
            "processBlock does not contain juce::ScopedNoDenormals for denormal protection"
        )

    all_functions = _extract_named_functions(source)
    inference_like = [
        fn
        for fn in all_functions
        if re.search(r"(inference|background|thread)", fn.name, flags=re.IGNORECASE)
    ]
    for function in inference_like:
        _scan_forbidden(function, violations)

    if violations:
        print("FAIL: real-time memory hygiene checks found violations:", file=sys.stderr)
        for item in violations:
            print(f"  - {item}", file=sys.stderr)
        return 1

    print("PASS: real-time memory hygiene checks completed with no violations.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Detect forbidden allocations/contention patterns in real-time paths."
    )
    parser.add_argument("--plugin-processor", required=True, help="Path to PluginProcessor.cpp")
    args = parser.parse_args()
    return run(args.plugin_processor)


if __name__ == "__main__":
    sys.exit(main())
