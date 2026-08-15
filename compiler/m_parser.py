"""Parses virtual-assembly source files into structured instruction tokens.

Two syntaxes are recognized:
  Data-processing:  OP dest, op1, op2      (e.g. "ADD r0, r0, r1")
  Comparison:       CMP op1, op2           (e.g. "CMP r0, 0x10")
  Move:             MOV dest, op2          (e.g. "MOV r0, 1")
  Branch:           BCC offset             (e.g. "BEQ -4")

Comments start with ';' and run to the end of the line. Blank lines are
ignored.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Union

AUTHORIZED_OPERATIONS = {"AND", "ADD", "CMP", "MOV", "ORR", "EOR", "ADC", "SUB", "SBC", "LSH", "RSH"}
AUTHORIZED_BRANCHES = {"B", "BEQ", "BNE", "BLE", "BGE", "BL", "BG"}


class AssemblySyntaxError(Exception):
    """Raised when a source line does not match the expected instruction syntax."""


@dataclass
class Instruction:
    """A data-processing / comparison / move instruction: `op dest, op1, op2`."""

    op: str
    dest: str
    op1: str
    op2: str


@dataclass
class BranchInstruction:
    """A conditional or unconditional branch: `bcc offset`."""

    op: str
    offset: str


ParsedLine = Union[Instruction, BranchInstruction]


def read_source(filename: str) -> list[str]:
    """Read a source file and return its raw lines."""
    with open(filename, "r") as f:
        return f.readlines()


def strip_comment_and_whitespace(line: str) -> str:
    """Remove trailing comments (after ';') and surrounding whitespace."""
    return line.split(";", 1)[0].strip()


def parse_line(line: str) -> Optional[ParsedLine]:
    """Parse a single cleaned source line, or return None for a blank line."""
    if not line:
        return None

    if "," in line:
        head, *rest = line.split(",")
        head_parts = head.split()
        if not head_parts:
            raise AssemblySyntaxError(f"missing operation: {line!r}")

        op = head_parts[0]
        if op.upper() not in AUTHORIZED_OPERATIONS:
            raise AssemblySyntaxError(f"unknown operation: {op!r}")
        if len(rest) >= 2:
            if len(head_parts) < 2:
                raise AssemblySyntaxError(f"missing destination register: {line!r}")
            dest, op1, op2 = head_parts[1], rest[0], rest[1]  # OP dest, op1, op2
        elif op.upper() == "CMP":
            if len(head_parts) < 2 or len(rest) < 1:
                raise AssemblySyntaxError(f"CMP expects 'CMP op1, op2': {line!r}")
            dest, op1, op2 = "0", head_parts[1], rest[0]  # dest is unused by CMP
        else:
            if len(head_parts) < 2 or len(rest) < 1:
                raise AssemblySyntaxError(f"expects 'OP dest, op2': {line!r}")
            dest, op1, op2 = head_parts[1], "0", rest[0]  # op1 is unused (e.g. MOV)

        return Instruction(op, dest.strip(), op1.strip(), op2.strip())

    tokens = line.split()
    if tokens and tokens[0].upper() in AUTHORIZED_BRANCHES:
        if len(tokens) != 2:
            raise AssemblySyntaxError(f"branch instruction expects one offset: {line!r}")
        return BranchInstruction(tokens[0], tokens[1])

    raise AssemblySyntaxError(f"unrecognized instruction: {line!r}")


def parse(filename: str) -> list[ParsedLine]:
    """Parse a source file into a list of instructions, skipping blank lines."""
    parsed: list[ParsedLine] = []
    for raw_line in read_source(filename):
        line = strip_comment_and_whitespace(raw_line)
        parsed_line = parse_line(line)
        if parsed_line is not None:
            parsed.append(parsed_line)
    return parsed
