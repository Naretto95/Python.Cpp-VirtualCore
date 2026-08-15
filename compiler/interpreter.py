"""Encodes parsed instructions into the virtual core's 32-bit binary format.

Data-processing word:  IV:8 | dest:4 | op2:4 | op1:4 | opcode:4 | flag:1 | 0:3
                        (bits 0-7)  (8-11) (12-15) (16-19)  (20-23)  (24)  (25-31)
Branch word:            offset:27 | sign:1 | bcc:4
                        (bits 0-26)  (27)     (28-31)

Both are packed big-endian, matching what core/core.cpp expects on disk.
"""

from __future__ import annotations

import struct
from typing import Tuple

from m_parser import BranchInstruction, Instruction, ParsedLine

OPCODES = {
    "AND": 0, "ORR": 1, "EOR": 2, "ADD": 3, "ADC": 4,
    "CMP": 5, "SUB": 6, "SBC": 7, "MOV": 8, "LSH": 9, "RSH": 10,
}

BRANCH_CONDITIONS = {
    "B": 8, "BEQ": 9, "BNE": 10, "BLE": 11, "BGE": 12, "BL": 13, "BG": 14,
}

REGISTERS = {f"r{i}": i for i in range(16)}


class EncodingError(Exception):
    """Raised when an operand cannot be encoded into the instruction format."""


def parse_integer(token: str) -> int:
    """Parse a (possibly negative) decimal or 0x-prefixed hexadecimal integer literal."""
    negative = token.startswith("-")
    body = token[1:] if negative else token
    if body.lower().startswith("0x"):
        value = int(body, 16)
    elif body.isdigit():
        value = int(body)
    else:
        raise ValueError(token)
    return -value if negative else value


def resolve_register(token: str) -> int:
    """Resolve a register name (or an explicit numeric register index) to its 4-bit index."""
    if token.lower() in REGISTERS:
        return REGISTERS[token.lower()]
    try:
        return parse_integer(token)
    except ValueError:
        raise EncodingError(f"invalid register: {token!r}") from None


def resolve_second_operand(token: str) -> Tuple[bool, int]:
    """Resolve the second operand of a data-processing instruction.

    Returns (is_register, value): value is a register index if is_register,
    otherwise an immediate value.
    """
    if token.lower() in REGISTERS:
        return True, REGISTERS[token.lower()]
    try:
        return False, parse_integer(token)
    except ValueError:
        raise EncodingError(f"invalid register or immediate value: {token!r}") from None


def encode_data_instruction(instr: Instruction) -> bytes:
    opcode = OPCODES[instr.op.upper()]
    dest = resolve_register(instr.dest)
    op1 = resolve_register(instr.op1)
    is_register, value = resolve_second_operand(instr.op2)

    word = 0
    word |= (0 if is_register else (value & 0xFF)) << 0
    word |= dest << 8
    word |= (value if is_register else 0) << 12
    word |= op1 << 16
    word |= opcode << 20
    word |= (0 if is_register else 1) << 24
    return struct.pack(">I", word)


def encode_branch_instruction(instr: BranchInstruction) -> bytes:
    condition = BRANCH_CONDITIONS[instr.op.upper()]
    try:
        offset = parse_integer(instr.offset)
    except ValueError:
        raise EncodingError(f"invalid branch offset: {instr.offset!r}") from None

    sign = 1 if offset < 0 else 0
    word = abs(offset) << 0 | sign << 27 | condition << 28
    return struct.pack(">I", word)


def encode(instr: ParsedLine) -> bytes:
    if isinstance(instr, Instruction):
        return encode_data_instruction(instr)
    return encode_branch_instruction(instr)


def assemble(instructions: list[ParsedLine], output_path: str) -> None:
    """Encode a list of parsed instructions and write the resulting binary to output_path."""
    with open(output_path, "wb") as f:
        for instr in instructions:
            f.write(encode(instr))
