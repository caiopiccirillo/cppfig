// Runs a fuzz target over files and directories of inputs without libFuzzer,
// so the corpus works as a regression test with any compiler.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <vector>

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) -> int;

namespace {

void RunFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    const std::vector<char> bytes { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): libFuzzer's interface takes bytes.
    LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

}  // namespace

auto main(int argc, char** argv) -> int
{
    std::size_t count = 0;
    for (int i = 1; i < argc; ++i) {
        const std::filesystem::path argument(argv[i]);
        if (std::filesystem::is_directory(argument)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(argument)) {
                if (entry.is_regular_file()) {
                    RunFile(entry.path());
                    ++count;
                }
            }
        }
        else {
            RunFile(argument);
            ++count;
        }
    }
    if (count == 0) {
        std::fprintf(stderr, "no inputs found\n");
        return 1;
    }
    std::printf("replayed %zu inputs\n", count);
    return 0;
}
