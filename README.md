# Python.Cpp-VirtualCore

A small virtual CPU with a custom 32-bit instruction set, a fetch/decode/execute
core written in C++, and an assembler written in Python that compiles a
human-readable assembly syntax down to the core's binary format.

```
source.s  --[compiler/compiler.py]-->  bin  --[core/core]-->  register dump
```

## Project structure

```
core/           C++ virtual core (fetch / decode / execute loop)
compiler/       Python assembler (parser + instruction encoder + CLI)
add128/         Example program: 128-bit addition using two 64-bit registers
init/           Example program: building a 64-bit constant byte by byte
lshift64_128/   Example program: 128-bit left shift using 64-bit registers
```

Each example directory contains:
- `*.s` — assembly source
- `*_state` — initial register values fed to the core
- `*_test.bin` — the source pre-compiled to a binary, ready to run

## Instruction set

The core has 16 general-purpose 64-bit registers (`r0`-`r15`). Every
instruction is a 32-bit word, in one of two formats:

**Data-processing:** `OP dest, op1, op2`

| Mnemonic | Meaning                | Mnemonic | Meaning                  |
|----------|-------------------------|----------|---------------------------|
| `AND`    | bitwise AND              | `CMP`    | compare (sets flags)      |
| `ORR`    | bitwise OR               | `SUB`    | subtract                  |
| `EOR`    | bitwise XOR              | `SBC`    | subtract with borrow      |
| `ADD`    | add                      | `MOV`    | move                      |
| `ADC`    | add with carry           | `LSH`    | logical left shift        |
|          |                          | `RSH`    | logical right shift       |

`op2` may be a register (`r0`-`r15`) or an immediate (decimal, or hex with a
`0x` prefix). `CMP` takes only `op1, op2` (no destination); `MOV` takes only
`dest, op2` (no first operand).

**Branch:** `BCC offset`

| Mnemonic | Branches when...        |
|----------|---------------------------|
| `B`      | always                    |
| `BEQ`    | last comparison was `==`  |
| `BNE`    | last comparison was `!=`  |
| `BLE`    | last comparison was `<=`  |
| `BGE`    | last comparison was `>=`  |
| `BL`     | last comparison was `<`   |
| `BG`     | last comparison was `>`   |

`offset` is a signed, PC-relative instruction count (e.g. `BL -4` re-executes
the previous 4 instructions if the last `CMP` was "lower"). Comparison flags
are set by `CMP` and only remain valid for the single instruction that
follows it.

Comments start with `;` and run to the end of the line.

## Building the core

Requires a C++17 compiler.

```bash
cd core
make
```

This produces the `core` executable.

## Compiling a program

Requires Python 3.

```bash
python3 compiler/compiler.py <source.s> -o <output.bin>
```

## Running a program

```bash
./core/core <binary-file> <state-file> [-v]
```

- `binary-file` — a program compiled with `compiler.py` (or one of the
  `*_test.bin` examples)
- `state-file` — initial register values, one `rN=0x...` line per register
- `-v` — verbose mode, prints every fetch/decode/execute step
- `-h` — show usage

### Example

```bash
cd core
make
./core ../add128/add128_test.bin ../add128/add128_state
```

This adds two 128-bit numbers (split across `r0:r1` and `r2:r3`) and prints
the final register state.

## Contributing

Forks and pull requests are welcome — bug fixes, new instructions, better
tooling, or improved documentation.

## License

MIT — see [LICENSE](LICENSE).
