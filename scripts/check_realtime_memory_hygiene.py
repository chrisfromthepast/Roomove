#!/usr/bin/env python3
import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple


@dataclass
class FunctionBody:
    name: str
    body: str
    start_line: int


FORBIDDEN_ALLOCATION_PATTERNS: Tuple[Tuple[str, str], ...] = (
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

FORBIDDEN_LOCK_PATTERNS: Tuple[Tuple[str, str], ...] = (
    (r"\bstd::mutex\b", "std::mutex"),
    (r"\bstd::recursive_mutex\b", "std::recursive_mutex"),
    (r"\bstd::lock_guard\b", "std::lock_guard"),
    (r"\bstd::unique_lock\b", "std::unique_lock"),
    (r"\bstd::scoped_lock\b", "std::scoped_lock"),
    (r"\bjuce::CriticalSection\b", "juce::CriticalSection"),
    (r"\bjuce::SpinLock\b", "juce::SpinLock"),
    (r"\bjuce::ScopedLock\b", "juce::ScopedLock"),
    (r"\bjuce::ScopedTryLock\b", "juce::ScopedTryLock"),
)

FORBIDDEN_BLOCKING_PATTERNS: Tuple[Tuple[str, str], ...] = (
    (r"\bsleep(_for|_until)?\s*\(", "sleep call"),
    (r"\bwait(_for|_until)?\s*\(", "wait call"),
    (r"\bjoin\s*\(", "thread join"),
    (r"\bget\s*\(\s*\)", "future-style blocking get"),
    (r"\bMessageManagerLock\b", "message thread lock"),
    (r"\breadEntire(?:StreamAsString|FileAsString)\s*\(", "blocking file read"),
)

FORBIDDEN_NONDETERMINISTIC_TEST_PATTERNS: Tuple[Tuple[str, str], ...] = (
    (r"\bstd::chrono\b", "wall-clock timing"),
    (r"\bstd::thread\b", "thread scheduling"),
    (r"\byield\s*\(", "scheduler-sensitive yield"),
    (r"\bsleep(_for|_until)?\s*\(", "sleep-based timing"),
    (r"\bwait(_for|_until)?\s*\(", "wait-based timing"),
    (r"\bjoin\s*\(", "join-based coordination"),
)


def _read_text(path: str) -> str:
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        print(f"ERROR: unable to read file '{path}': {exc}", file=sys.stderr)
        raise


def _strip_cpp_non_code(text: str) -> str:
    result: List[str] = []
    index = 0

    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ""

        if char == "/" and next_char == "/":
            result.append(" ")
            result.append(" ")
            index += 2
            while index < len(text) and text[index] != "\n":
                result.append(" ")
                index += 1
            continue

        if char == "/" and next_char == "*":
            result.append(" ")
            result.append(" ")
            index += 2
            while index < len(text):
                current = text[index]
                following = text[index + 1] if index + 1 < len(text) else ""
                if current == "*" and following == "/":
                    result.append(" ")
                    result.append(" ")
                    index += 2
                    break
                result.append("\n" if current == "\n" else " ")
                index += 1
            continue

        if char == "R" and next_char == '"':
            delimiter_start = index + 2
            delimiter_end = text.find("(", delimiter_start)
            if delimiter_end == -1:
                result.append(char)
                index += 1
                continue

            delimiter = text[delimiter_start:delimiter_end]
            raw_terminator = ")" + delimiter + '"'
            raw_end = text.find(raw_terminator, delimiter_end + 1)
            if raw_end == -1:
                result.append(char)
                index += 1
                continue

            for raw_char in text[index : raw_end + len(raw_terminator)]:
                result.append("\n" if raw_char == "\n" else " ")
            index = raw_end + len(raw_terminator)
            continue

        if char in ('"', "'"):
            quote_char = char
            result.append(" ")
            index += 1
            escape = False
            while index < len(text):
                current = text[index]
                if escape:
                    escape = False
                    result.append("\n" if current == "\n" else " ")
                    index += 1
                    continue
                if current == "\\":
                    escape = True
                    result.append(" ")
                    index += 1
                    continue
                result.append("\n" if current == "\n" else " ")
                index += 1
                if current == quote_char:
                    break
            continue

        result.append(char)
        index += 1

    return "".join(result)


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


def _extract_named_function(text: str, name: str) -> Optional[FunctionBody]:
    pattern = re.compile(
        rf"^[\w:<>\s\*&~]+?\b({re.escape(name)})\s*\([^;{{}}]*\)\s*(?:const\s*)?\{{",
        flags=re.MULTILINE,
    )
    match = pattern.search(text)
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


def _scan_forbidden(
    function: FunctionBody,
    violations: List[str],
    patterns: Tuple[Tuple[str, str], ...],
    category: str,
) -> None:
    for expression, description in patterns:
        if re.search(expression, function.body):
            violations.append(
                f"{function.name} (line {function.start_line}) uses {description} in {category}"
            )


def run(plugin_processor_path: str) -> int:
    plugin_processor = Path(plugin_processor_path).resolve()
    repo_root = plugin_processor.parent.parent
    dsp_source_path = repo_root / "dsp" / "RoomoveDSP.cpp"
    validation_tests_path = repo_root / "tests" / "RealtimeValidationTests.cpp"

    if not plugin_processor.exists():
        print(
            f"ERROR: plugin processor path does not exist: {plugin_processor}",
            file=sys.stderr,
        )
        return 2
    if not dsp_source_path.exists() or not validation_tests_path.exists():
        print(
            "ERROR: expected Roomove repository layout was not found next to PluginProcessor.cpp",
            file=sys.stderr,
        )
        return 2

    try:
        source = _read_text(str(plugin_processor))
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

    prepare_to_play = _extract_named_function(source, "ArmorAudioProcessor::prepareToPlay")
    if prepare_to_play is None:
        print("ERROR: prepareToPlay was not found in PluginProcessor.cpp", file=sys.stderr)
        return 2

    _scan_forbidden(
        process_block,
        violations,
        FORBIDDEN_ALLOCATION_PATTERNS,
        "processBlock",
    )
    _scan_forbidden(
        process_block,
        violations,
        FORBIDDEN_LOCK_PATTERNS,
        "processBlock",
    )
    _scan_forbidden(
        process_block,
        violations,
        FORBIDDEN_BLOCKING_PATTERNS,
        "processBlock",
    )
    if "juce::ScopedNoDenormals" not in process_block.body:
        violations.append(
            "processBlock does not contain juce::ScopedNoDenormals for denormal protection"
        )
    if "dspStates.resize" not in prepare_to_play.body:
        violations.append(
            "prepareToPlay should perform DSP state warmup before realtime processing"
        )

    try:
        dsp_source = _read_text(str(dsp_source_path))
        validation_tests_source = _read_text(str(validation_tests_path))
    except OSError:
        return 2
    stripped_validation_tests = _strip_cpp_non_code(validation_tests_source)

    for function_name in (
        "roomoveDspStateProcessAudioNoAlias",
        "roomoveDspStateProcessAudio",
    ):
        function = _extract_named_function(dsp_source, function_name)
        if function is None:
            print(
                f"ERROR: {function_name} was not found in RoomoveDSP.cpp",
                file=sys.stderr,
            )
            return 2
        _scan_forbidden(
            function,
            violations,
            FORBIDDEN_ALLOCATION_PATTERNS,
            function_name,
        )
        _scan_forbidden(
            function,
            violations,
            FORBIDDEN_LOCK_PATTERNS,
            function_name,
        )
        _scan_forbidden(
            function,
            violations,
            FORBIDDEN_BLOCKING_PATTERNS,
            function_name,
        )

    for expression, description in FORBIDDEN_NONDETERMINISTIC_TEST_PATTERNS:
        match = re.search(expression, stripped_validation_tests)
        if match is not None:
            line_number = stripped_validation_tests.count("\n", 0, match.start()) + 1
            violations.append(
                "RealtimeValidationTests.cpp "
                f"(line {line_number}) uses {description}, which makes CI assertions timing-sensitive"
            )

    if violations:
        print("FAIL: stage 1 realtime guardrail checks found violations:", file=sys.stderr)
        for item in violations:
            print(f"  - {item}", file=sys.stderr)
        return 1

    print("PASS: stage 1 realtime guardrail checks completed with no violations.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Detect stage 1 realtime guardrail violations in audio and DSP paths."
    )
    parser.add_argument("--plugin-processor", required=True, help="Path to PluginProcessor.cpp")
    args = parser.parse_args()
    return run(args.plugin_processor)


if __name__ == "__main__":
    sys.exit(main())
