#include "fjmReader.h"


template <class W, bool ZeroInit, u16 Aligned, bool NoNullJump,
        bool AllowSelfModify, bool JumpBeforeFlip>
void run(std::ifstream& file, std::istream& input, std::ostream& output, bool silent) {

    // TODO print time to create mem, if not silent.
    Mem<W, ZeroInit, Aligned> mem(file, input, output);

    // TODO add const / constexpr on anything that you can.

    W ip = 0;
    constexpr W w = sizeof(W)*8;

    RunStatistics stats;

    while (true) {
        if (ip % Aligned) { //  [[unlikely]]
            std::cerr << "Error: read from an unaligned address (ip=0x" << std::hex << ip << ")." << std::endl;
            exit(1);
        }

        const W f = mem.read_word(ip);
        if constexpr (!AllowSelfModify) {
            if (ip <= f && f < ip + 2*w) {
                std::cerr << "Error: op tried to flip itself (ip=0x" << std::hex << ip << ", flip=0x" << std::hex << f
                << "), while the AllowSelfModify flag is off." << std::endl;
                exit(1);
            }
        }

        if constexpr ( JumpBeforeFlip) {
            mem.flip_bit(f, stats);
        }
        const W j = mem.read_word_check_input(ip + w, stats);
        if constexpr (!JumpBeforeFlip) {
            mem.flip_bit(f, stats);
        }

        if (ip == j && !(ip <= f && f < ip + 2*w))
            break;
        if constexpr (NoNullJump) {
            if (j < 2*w) {
                std::cerr << "Error: jump to address " << std::hex << j << " (while NoNullJump flag is on)." << std::endl;
                exit(1);
            }
        }
        ip = j;

        if (!silent) {
            stats.count();
        }
    }

    stats.stopTimer();
    if (!silent) {
        stats.printStats();
    }
}


void cpu(std::ifstream& file, bool silent) {
    //TODO use the silent field.

    u16 magic, w;
    readTo(file, magic);
    if (magic != FJ_MAGIC) {
        std::cerr << "Error: bad magic code (0x" << std::hex << magic << ", should be 0x" << std::hex << FJ_MAGIC << ")." << std::endl;
        exit(1);
    }
    readTo(file, w);

    //TODO read memory here and not in run?

    switch (w) {
        case 8:
            run<u8 , false, 2*8 , true,    true, true>(file, std::cin, std::cout, silent); break;
        case 16:
            run<u16, false, 2*16, true,    true, true>(file, std::cin, std::cout, silent); break;
        case 32:
            run<u32, false, 2*32, true,    true, true>(file, std::cin, std::cout, silent); break;
        case 64:
            run<u64, false, 2*64, true,    true, true>(file, std::cin, std::cout, silent); break;
        default:
            std::cerr << "Error: bad memory-width (" << w << " not in {8, 16, 32, 64})." << std::endl;
            exit(1);
    }
}
