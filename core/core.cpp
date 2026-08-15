// core.cpp
//
// A small virtual CPU core: it loads a binary program produced by the
// Python assembler (see ../compiler) and an initial register state, then
// runs a fetch/decode/execute loop over 16 general-purpose 64-bit registers.
//
// Instruction encoding (32-bit words, big-endian on disk):
//
//   Data-processing:  [ unused:7 | flag:1 | opcode:4 | op1:4 | op2:4 | dest:4 | imm:8 ]
//                         31-25     24       23-20     19-16   15-12   11-8     7-0
//     `flag` selects whether `imm` (immediate) or register `op2` supplies
//     the second operand. Bits 25-31 are always 0 for these instructions.
//
//   Branch:           [ bcc:4 | sign:1 | offset:27 ]
//                        31-28   27       26-0
//     `bcc` is one of the Branch condition codes below (always >= 0x8),
//     which is how the core tells the two instruction formats apart.
//
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vcore {

namespace fs = std::filesystem;

constexpr int kRegisterCount = 16;
constexpr int kComparisonFlagCount = 6;
constexpr uint64_t kMax64 = UINT64_MAX;

enum class Opcode : uint8_t {
    And = 0x0,
    Orr = 0x1,
    Eor = 0x2,
    Add = 0x3,
    Adc = 0x4,  // add with carry
    Cmp = 0x5,
    Sub = 0x6,
    Sbc = 0x7,  // subtract with borrow
    Mov = 0x8,
    Lsh = 0x9,  // logical left shift
    Rsh = 0xa,  // logical right shift
};

enum class Branch : uint8_t {
    Always = 0x8,
    Equal = 0x9,
    NotEqual = 0xa,
    LowerEqual = 0xb,
    GreaterEqual = 0xc,
    Lower = 0xd,
    Greater = 0xe,
};

enum CompareFlag {
    kEqual = 0,
    kNotEqual = 1,
    kLowerEqual = 2,
    kGreaterEqual = 3,
    kLower = 4,
    kGreater = 5,
};

struct DecodedInstruction {
    bool immediate = false;
    uint8_t opcode = 0;
    uint8_t operand1 = 0;
    uint8_t operand2 = 0;
    uint8_t destination = 0;
    uint8_t immediate_value = 0;
};

struct CpuState {
    std::array<uint64_t, kRegisterCount> registers{};
    std::array<bool, kComparisonFlagCount> compare_flags{};
    uint32_t carry = 0;
    uint32_t pc = 0;
};

uint32_t extractBits(uint32_t value, int begin, int end) {
    const uint32_t mask = (1u << (end - begin + 1)) - 1;
    return (value >> begin) & mask;
}

// Reads a program file into 32-bit big-endian instruction words.
std::vector<uint32_t> readProgram(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open program file: " + path.string());

    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.empty() || bytes.size() % 4 != 0) {
        throw std::runtime_error("program file size must be a non-zero multiple of 4 bytes: " + path.string());
    }

    std::vector<uint32_t> program(bytes.size() / 4);
    for (size_t i = 0; i < program.size(); ++i) {
        const size_t base = i * 4;
        program[i] = (static_cast<uint32_t>(bytes[base]) << 24) |
                     (static_cast<uint32_t>(bytes[base + 1]) << 16) |
                     (static_cast<uint32_t>(bytes[base + 2]) << 8) |
                      static_cast<uint32_t>(bytes[base + 3]);
    }
    return program;
}

// Reads initial register values from lines of the form "rN=0x...".
// Unrecognized lines are ignored.
void readInitialState(const fs::path& path, std::array<uint64_t, kRegisterCount>& registers) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot open state file: " + path.string());

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || (line[0] != 'r' && line[0] != 'R')) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        int reg_index;
        try {
            reg_index = std::stoi(line.substr(1, eq - 1));
        } catch (const std::exception&) {
            continue;
        }
        if (reg_index < 0 || reg_index >= kRegisterCount) continue;

        try {
            registers[static_cast<size_t>(reg_index)] = std::stoull(line.substr(eq + 1), nullptr, 16);
        } catch (const std::exception&) {
            throw std::runtime_error("malformed value on state line: " + line);
        }
    }
}

// Decides whether a branch is taken and returns the next program counter.
uint32_t fetchBranch(uint32_t pc, uint32_t instruction, const std::array<bool, kComparisonFlagCount>& flags, bool verbose) {
    const uint32_t bcc = extractBits(instruction, 28, 31);
    const uint32_t raw_offset = extractBits(instruction, 0, 26);
    const bool negative = extractBits(instruction, 27, 27) != 0;
    const int32_t offset = negative ? -static_cast<int32_t>(raw_offset) : static_cast<int32_t>(raw_offset);

    bool taken = false;
    switch (static_cast<Branch>(bcc)) {
        case Branch::Always:       taken = true; break;
        case Branch::Equal:        taken = flags[kEqual]; break;
        case Branch::NotEqual:     taken = flags[kNotEqual]; break;
        case Branch::LowerEqual:   taken = flags[kLowerEqual]; break;
        case Branch::GreaterEqual: taken = flags[kGreaterEqual]; break;
        case Branch::Lower:        taken = flags[kLower]; break;
        case Branch::Greater:      taken = flags[kGreater]; break;
    }

    const uint32_t next_pc = taken ? static_cast<uint32_t>(static_cast<int64_t>(pc) + 1 + offset) : pc + 1;
    if (verbose) {
        std::printf("instruction = %x , PC = %u , BCC = %x , offset = %x , taken = %d\n",
                    instruction, pc, bcc, raw_offset, taken);
    }
    return next_pc;
}

DecodedInstruction decode(uint32_t instruction, bool verbose) {
    DecodedInstruction d;
    d.immediate = extractBits(instruction, 24, 24) != 0;
    d.opcode = static_cast<uint8_t>(extractBits(instruction, 20, 23));
    d.operand1 = static_cast<uint8_t>(extractBits(instruction, 16, 19));
    d.operand2 = static_cast<uint8_t>(extractBits(instruction, 12, 15));
    d.destination = static_cast<uint8_t>(extractBits(instruction, 8, 11));
    d.immediate_value = static_cast<uint8_t>(extractBits(instruction, 0, 7));

    if (verbose) {
        std::printf("instruction = %x , opcode = %x , op1 = %x , op2 = %x , dest = %x , IV = %x , flag = %d\n",
                    instruction, d.opcode, d.operand1, d.operand2, d.destination, d.immediate_value, d.immediate);
    }
    return d;
}

void execute(CpuState& state, const DecodedInstruction& instr, bool verbose) {
    state.compare_flags.fill(false);

    const uint64_t op1 = state.registers[instr.operand1];
    const uint64_t value = instr.immediate ? instr.immediate_value : state.registers[instr.operand2];
    uint64_t& dest = state.registers[instr.destination];

    switch (static_cast<Opcode>(instr.opcode)) {
        case Opcode::And:
            dest = op1 & value;
            break;
        case Opcode::Orr:
            dest = op1 | value;
            break;
        case Opcode::Eor:
            dest = op1 ^ value;
            break;
        case Opcode::Add:
            if (value > 0 && op1 > kMax64 - value) state.carry = 1;
            dest = op1 + value;
            break;
        case Opcode::Adc: {
            const uint32_t carry_in = state.carry;
            state.carry = (op1 > kMax64 - value - carry_in) ? 1 : 0;
            dest = op1 + value + carry_in;
            break;
        }
        case Opcode::Cmp:
            state.compare_flags[kEqual] = (op1 == value);
            state.compare_flags[kNotEqual] = (op1 != value);
            state.compare_flags[kLowerEqual] = (op1 <= value);
            state.compare_flags[kGreaterEqual] = (op1 >= value);
            state.compare_flags[kLower] = (op1 < value);
            state.compare_flags[kGreater] = (op1 > value);
            break;
        case Opcode::Sub:
            if (value > 0 && op1 < value) state.carry = 1;
            dest = op1 - value;
            break;
        case Opcode::Sbc: {
            const uint32_t carry_in = state.carry;
            state.carry = (op1 < value - carry_in + 1) ? 1 : 0;
            dest = op1 - value + carry_in - 1;
            break;
        }
        case Opcode::Mov:
            dest = value;
            break;
        case Opcode::Lsh:
            dest = op1 << value;
            break;
        case Opcode::Rsh:
            dest = op1 >> value;
            break;
    }

    if (verbose) {
        std::printf("carry = %x , EQ = %d , NE = %d , LE = %d , GE = %d , LT = %d , GT = %d\n",
                    state.carry, state.compare_flags[0], state.compare_flags[1], state.compare_flags[2],
                    state.compare_flags[3], state.compare_flags[4], state.compare_flags[5]);
        for (int i = 0; i < kRegisterCount; ++i) {
            std::printf("R%d=%016llx ", i, static_cast<unsigned long long>(state.registers[i]));
        }
        std::printf("\n");
    }
}

void run(const fs::path& program_path, const fs::path& state_path, bool verbose) {
    CpuState state;
    const std::vector<uint32_t> program = readProgram(program_path);
    readInitialState(state_path, state.registers);

    if (verbose) std::printf("BEGIN PARSING\n----------------------------------------------------------------\n");

    while (state.pc < program.size()) {
        const uint32_t current_pc = state.pc;
        const uint32_t instruction = program[current_pc];
        const bool is_branch = extractBits(instruction, 28, 31) != 0;

        if (is_branch) {
            state.pc = fetchBranch(current_pc, instruction, state.compare_flags, verbose);
        } else {
            const DecodedInstruction decoded = decode(instruction, verbose);
            execute(state, decoded, verbose);
            state.pc = current_pc + 1;
        }
        if (verbose) std::printf("----------------------------------------------------------------\n");
    }

    if (verbose) std::printf("END PARSING\n");

    std::printf("Registers final state:\n");
    for (int i = 0; i < kRegisterCount; ++i) {
        std::printf("R%d=%016llx ", i, static_cast<unsigned long long>(state.registers[i]));
    }
    std::printf("\n");
}

void printUsage() {
    std::cout <<
        "core - a small virtual CPU core\n\n"
        "Usage:\n"
        "  core <binary-file> <state-file> [-v]\n\n"
        "Arguments:\n"
        "  binary-file   Compiled program produced by the Python assembler (compiler/compiler.py)\n"
        "  state-file    Initial register values, one \"rN=0x...\" line per register\n\n"
        "Options:\n"
        "  -v            Verbose mode: print every fetch/decode/execute step\n"
        "  -h            Show this help message\n\n"
        "See core/coreman for the full manual page (man ./coreman).\n";
}

}  // namespace vcore

int main(int argc, char* argv[]) {
    const std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty() || args[0] == "-h" || args[0] == "--help") {
        vcore::printUsage();
        return EXIT_SUCCESS;
    }
    if (args.size() < 2 || args.size() > 3 || (args.size() == 3 && args[2] != "-v")) {
        std::cerr << "Invalid usage. Try -h for help.\n";
        return EXIT_FAILURE;
    }

    const bool verbose = (args.size() == 3);
    const vcore::fs::path binary_path = args[0];
    const vcore::fs::path state_path = args[1];

    if (!vcore::fs::exists(binary_path) || !vcore::fs::exists(state_path)) {
        std::cerr << "One of the input files does not exist.\n"
                  << "  binary file: " << binary_path.string()
                  << (vcore::fs::exists(binary_path) ? " (found)\n" : " (missing)\n")
                  << "  state file : " << state_path.string()
                  << (vcore::fs::exists(state_path) ? " (found)\n" : " (missing)\n");
        return EXIT_FAILURE;
    }

    try {
        std::cout << "Starting core" << (verbose ? " (verbose)" : "") << "\n";
        vcore::run(binary_path, state_path, verbose);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
