"""CLI entry point: compiles a virtual-assembly source file into a core binary.

Usage:
    python compiler.py <source.s> [-o output]
"""

from __future__ import annotations

import argparse
import sys

from interpreter import EncodingError, assemble
from m_parser import AssemblySyntaxError, parse


def main() -> int:
    arg_parser = argparse.ArgumentParser(description="Compile virtual-assembly source into a core binary.")
    arg_parser.add_argument("source", help="path to the .s source file")
    arg_parser.add_argument("-o", "--output", default="bin", help="output binary file (default: %(default)s)")
    args = arg_parser.parse_args()

    try:
        instructions = parse(args.source)
        assemble(instructions, args.output)
    except FileNotFoundError:
        print(f"error: source file not found: {args.source}", file=sys.stderr)
        return 1
    except (AssemblySyntaxError, EncodingError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    print(f"compiled {args.source} -> {args.output} ({len(instructions)} instructions)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
