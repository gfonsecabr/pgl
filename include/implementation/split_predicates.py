#!/usr/bin/env python3
from typing import List
from collections import Counter


def split_lines_by_empty_line(file_path: str) -> tuple[List[str], List[tuple[str]]]:
    """Read a file and split its lines into blocks separated by empty lines.

    Empty lines inside an unmatched opening '{' are ignored.
    """
    with open(file_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    blocks: List[tuple[str]] = []
    current_block: List[str] = []
    brace_depth = 0
    active = False
    header_lines: List[str] = []

    for line in lines:
        if active:
            stripped = line.strip()
            if stripped == "":
                if current_block and brace_depth > 0:
                    continue
                if current_block:
                    blocks.append(tuple(current_block))
                    current_block = []
                continue

            current_block.append(line.rstrip("\n"))
            brace_depth += line.count("{") - line.count("}")
        else:
            header_lines.append(line)
        if "namespace pgl {" in line:
            active = True

    if current_block:
        blocks.append(tuple(current_block))

    return header_lines, blocks


def find_between(text: str) -> str | None:
    s1 = "::"
    s2 = "("
    start = 0
    while True:
        pos = text.find(s1, start)
        if pos == -1:
            return None
        if pos >= 3 and text[pos-3:pos] == "std":
            start = pos + len(s1)
            continue
        else:
            start = pos
            break
    start += len(s1)
    end = text.find(s2, start)
    if end == -1:
        return None

    result = text[start:end]
    if s1 in result:
        return None

    return text[start:end]

def find_between_in_block(block: tuple[str]) -> str | None:
    for line in block:
        if line.find("constexpr") != 0:
            continue
        result = find_between(line)
        if result is not None:
            return result
    return None


if __name__ == "__main__":
    import sys

    if len(sys.argv) != 2:
        print("Usage: python split_predicates.py <file_path>")
        sys.exit(1)

    methods = Counter()
    file_path = sys.argv[1]
    tag = {}
    header, blocks = split_lines_by_empty_line(file_path)

    for block in blocks:
        predicate = find_between_in_block(block)
        tag[block] = predicate
        if predicate:
            methods[predicate] += 1
        # else:
        #     print("Could not find predicate in block:")
        #     for line in block:
        #         print(line)
        #     print()

    predicates = ['contains', 'intersects', 'interiorContains', 'crosses', 'separates', 'boundaryContains', 'interiorsIntersect', 'interiorDisjoint']

    for fn in predicates+["predicates"]:
        with open(f"_{fn.lower()}.hpp", "a", encoding="utf-8") as f:
            for line in header:
                f.write(line)
            f.write("\n")

    for method in methods:
        print(f"{method}: {methods[method]}")

    for block, predicate in tag.items():
        if predicate in predicates:
            with open(f"_{predicate.lower()}.hpp", "a", encoding="utf-8") as f:
                for line in block:
                    f.write(line + "\n")
                f.write("\n")
        else:
            for fn in predicates+["predicates"]:
                if predicate==None or fn == "predicates":
                    with open(f"_{fn.lower()}.hpp", "a", encoding="utf-8") as f:
                        for line in block:
                            f.write(line + "\n")
                        f.write("\n")

