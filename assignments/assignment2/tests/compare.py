import re
import sys

FIRST_PATTERN = re.compile(r"FIRST\(([^)]+)\)")
FOLLOW_PATTERN = re.compile(r"FOLLOW\(([^)]+)\)")
SET_PATTERN = re.compile(r"\[(.*)\]")

def get_selected_pattern(mode: str) -> re.Pattern:
    return FIRST_PATTERN if mode == "FIRST" else FOLLOW_PATTERN


def parse_file(mode: str, path: str) -> dict[str, set[str]]:
    """
    Parses file into:
        {
            key: {elements}
        }
    Order of elements does not matter.
    """
    data = {}

    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line:
                continue

            key_match = get_selected_pattern(mode).search(line)
            if not key_match:
                continue

            key = key_match.group(1)

            set_match = SET_PATTERN.search(line)
            if not set_match:
                # Key exists but no set found
                print(f"[WARNING] No set found for {mode}({key}) in line: {line}")
                data[key] = set()
                continue

            elements_raw = set_match.group(1).strip()

            if elements_raw:
                elements = {e.strip() for e in elements_raw.split(",") if e.strip()}
            else:
                elements = set()

            data[key] = elements

    return data


def compare_files(mode: str, file1: str, file2: str) -> None:
    d1 = parse_file(mode, file1)
    d2 = parse_file(mode, file2)

    all_keys = set(d1) | set(d2)

    for key in sorted(all_keys):
        if key not in d1:
            print(f"[MISSING IN FILE1] {mode}({key})")
            continue
        if key not in d2:
            print(f"[MISSING IN FILE2] {mode}({key})")
            continue

        s1 = d1[key]
        s2 = d2[key]

        if s1 != s2:
            print(f"[DIFF] {mode}({key})")

            only_1 = s1 - s2
            only_2 = s2 - s1

            if only_1:
                print(f"  Only in file1: {sorted(only_1)}")
            if only_2:
                print(f"  Only in file2: {sorted(only_2)}")

            print()


if __name__ == "__main__":
    if len(sys.argv) != 4 or sys.argv[1] not in {"FIRST", "FOLLOW"}:
        print("Usage: python compare.py <mode=[FIRST|FOLLOW]> <file1> <file2>")
        sys.exit(1)

    mode = sys.argv[1]
    compare_files(mode, sys.argv[2], sys.argv[3])