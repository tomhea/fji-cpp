#include <iostream>
#include <fstream>
#include "include/parallel-hashmap/parallel_hashmap/phmap.h"
#include <cstdint>
#include <utility>
#include <vector>
#include <chrono>
#include <exception>
#include <iomanip>
#include <locale>


class comma_numpunct : public std::numpunct<char>
{
protected:
    char do_thousands_sep() const override { return ','; }
    std::string do_grouping() const override { return "\03"; }
};
template<class T>
std::string format_with_commas(T value)
{
    std::stringstream ss;
    std::locale comma_locale(std::locale(), new comma_numpunct());
    ss.imbue(comma_locale);
    ss << std::fixed << value;
    return ss.str();
}


typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


constexpr u16 FJ_MAGIC = 0x4a46;
constexpr u64 MAX_SUPPORTED_VERSION = 2;

#define readTo(file, var) file.read(reinterpret_cast<char*>(&(var)),sizeof(var))

#define assertRead(file, var) do { \
    if (!(file).read(reinterpret_cast<char*>(&(var)), sizeof(var))) { \
        std::cerr << "Error: unexpected end of file." << std::endl; \
        exit(1); \
    } \
} while(0)


struct Segment {
    u64 segmentStart, segmentLen, dataStart, dataLen;
};


class RunStatistics {
    std::chrono::time_point<std::chrono::high_resolution_clock> lastStart;
    u64 opCount;
    u64 totalTimeMilliSeconds;

public:

    RunStatistics() : opCount(0), totalTimeMilliSeconds(0) { startTimer(); }

    void count() {
        opCount++;
    }

    void startTimer() {
        lastStart = std::chrono::high_resolution_clock::now();
    }

    void stopTimer() {
        totalTimeMilliSeconds +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - lastStart).count();
    }

    void printStats() const {
        std::cout << "Finished after " << std::setprecision(4) << (static_cast<double>(totalTimeMilliSeconds)/1000000000.0)
        << "s (" << format_with_commas(opCount) << " FJ ops executed).";
    }
};


template <class W, bool ZeroInit, u16 Aligned>
class Mem {
public:
    Mem(std::ifstream& file, std::istream& input, std::ostream& output, u64 zerosFillThreshold = 1024)
            : mem(), zeroSegments(), input(input), output(output), outCurr(0), outLen(0), inCurr(0), inLen(0) {
        static_assert(Aligned != 0 && ((Aligned-1) & (Aligned)) == 0);
        static_assert(std::is_same<W, u8>() || std::is_same<W, u16>() || std::is_same<W, u32>() || std::is_same<W, u64>());

        u64 segmentNum, version, flags = 0;
        u32 reserved = 0;

        assertRead(file, version);
        assertRead(file, segmentNum);

        if (version > MAX_SUPPORTED_VERSION) {
            std::cerr << "Error: This interpreter only supports fjm versions 0-" << MAX_SUPPORTED_VERSION
            << ". It doesn't support version " << version << "." << std::endl;
            exit(1);
        }

        if (version > 0) {
            assertRead(file, flags);
            assertRead(file, reserved);
        }

        std::vector<Segment> segments;
        u64 segmentStart, segmentLen, dataStart, dataLen;
        for (u64 i = 0; i < segmentNum; i++) {
            assertRead(file, segmentStart);
            assertRead(file, segmentLen);
            assertRead(file, dataStart);
            assertRead(file, dataLen);
            segments.push_back({segmentStart, segmentLen, dataStart, dataLen});
        }

        std::vector<W> data;
        W datum;
        while (file.read(reinterpret_cast<char*>(&datum), sizeof(datum))) {
            data.push_back(datum);
        }

        // Fill segments.
        for (const Segment& seg : segments) {
            if (seg.segmentLen < seg.dataLen) {
                std::cerr << "Error: segment-length is smaller than data-length:  " << seg.segmentLen
                    << " < " << seg.dataLen << std::endl;
                exit(1);
            }

            // Guard against OOB into the data vector (also catches u64 overflow).
            if (seg.dataLen > data.size() ||
                (seg.dataLen > 0 && seg.dataStart > data.size() - seg.dataLen)) {
                std::cerr << "Error: segment data range [" << seg.dataStart << ", "
                    << seg.dataStart + seg.dataLen << ") exceeds data section size "
                    << data.size() << "." << std::endl;
                exit(1);
            }

            // Guard against addresses that don't fit in the word-width type W.
            if constexpr (!std::is_same_v<W, u64>) {
                constexpr u64 maxAddr = (u64{1} << (sizeof(W) * 8)) - 1;
                if (seg.segmentStart > maxAddr ||
                    seg.segmentLen > maxAddr ||
                    seg.segmentStart + seg.segmentLen > maxAddr + 1) {
                    std::cerr << "Error: segment address range exceeds "
                        << (sizeof(W) * 8) << "-bit address space." << std::endl;
                    exit(1);
                }
            }

            for (u64 i = 0; i < seg.dataLen; i++){
                W word = data[seg.dataStart + i];
                if ((2 == version) && ((seg.segmentStart + i) % 2 == 1)) {
                    word += (seg.segmentStart + i) * w;
                }
                mem[seg.segmentStart + i] = word;
            }

            if (seg.segmentLen > seg.dataLen) {
                const u64 start = seg.segmentStart + seg.dataLen;
                const u64 end = seg.segmentStart + seg.segmentLen;
                if (seg.segmentLen - seg.dataLen <= zerosFillThreshold) {
                    for (u64 i = start; i < end; i++) {
                        mem[i] = 0;
                    }
                } else {
                    zeroSegments.push_back({start, end});
                }
            }
        }
    }


    inline W read_word_check_input(W addr, RunStatistics& stats) {
        W word = read_word(addr);

        if (addr <= input_bit && input_bit < addr + w) {
            if (inLen == 0) {
                stats.stopTimer();
                int c = input.get();
                stats.startTimer();
                inCurr = (c == std::char_traits<char>::eof()) ? u8{0} : static_cast<u8>(c);
                inLen = 8;
            }

            const bool is1 = inCurr & 1;
            inLen--;
            inCurr>>=1;

            if (is1) {
                return word | (static_cast<W>(1) << input_offset);
            } else {
                return word & (~(static_cast<W>(1) << input_offset));
            }
        }
        else {
            return word;
        }
    }


    inline W read_word(W addr) {
        // TODO allow unaligned read?

        const W wordAddr = addr/(sizeof(W)*8);
        W val;

        if (mem.if_contains_unsafe(wordAddr, [&val](auto& value) {val=value.second;}))
            return val;
        else
            return get_not_mapped_word(addr, wordAddr);
    }



    inline void flip_bit(W addr, RunStatistics& stats) {
        if (addr <= 2*w+1 && addr >= 2*w) {
            if (addr == 2*w+1)
                outCurr |= 1<<outLen;
            if (++outLen == 8) {
                stats.stopTimer();
                output.put(outCurr);
                output.flush();
                stats.startTimer();
                outCurr = outLen = 0;
            }
        } else {
            const W wordAddr = addr / w;
            const W bitMask = static_cast<W>(1) << (addr % w);
            if (!mem.modify_if(wordAddr, [bitMask](auto& val) {val.second ^= bitMask;})) {
                flip_not_mapped_word(addr, wordAddr);
            }
        }
    }


private:
    phmap::parallel_flat_hash_map<W, W> mem;
    std::vector<std::pair<W, W>> zeroSegments;
    std::istream& input;
    std::ostream& output;
    u8 outCurr, outLen, inCurr, inLen;

    static constexpr W w = sizeof(W) * 8;
    static constexpr W input_offset = (w == 8) ? 4 : ((w == 16) ? 5 : ((w == 32) ? 6 : 7));
    static constexpr W input_bit = 3 * w + input_offset;


    inline W get_not_mapped_word(W addr, W wordAddr) {
        if constexpr (ZeroInit) {
            mem[wordAddr] = 0;
            return 0;
        } else {
            //TODO maybe prefetch hot spots.
            for (const auto& zeroSeg : zeroSegments) {
                if (zeroSeg.first <= wordAddr && wordAddr < zeroSeg.second) {
                    mem[wordAddr] = 0;
                    return 0;
                }
            }
            std::cerr << "Error: read from an uninitialized address 0x" << std::hex << addr << "." << std::endl;
            exit(1);
        }
    }

    inline W flip_not_mapped_word(W addr, W wordAddr) {
        const W word_value = static_cast<W>(1) << (addr % w);

        if constexpr (ZeroInit) {
            mem[wordAddr] = word_value;
            return word_value;
        } else {
            //TODO maybe prefetch hot spots.
            for (const auto& zeroSeg : zeroSegments) {
                if (zeroSeg.first <= wordAddr && wordAddr < zeroSeg.second) {
                    mem[wordAddr] = word_value;
                    return word_value;
                }
            }
            std::cerr << "Error: flip an uninitialized address 0x" << std::hex << addr << "." << std::endl;
            exit(1);
        }
    }
};
