// Fuzzes the .conf parser and its writer.
//
// Properties:
// - every line yields at most one entry or one error, reported in line order;
// - every error's line and column lie inside the document;
// - every entry's line and columns point at its path and value in the input;
// - every value the parser reads, and the whole input taken as one value,
//   survives writing with AppendConfValue and reading back unchanged.

#include <cppfig/conf.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "fuzz_check.h"

namespace {

auto LineAt(std::string_view document, std::size_t line_number) -> std::string_view
{
    std::size_t offset = 0;
    for (std::size_t line = 1; line < line_number; ++line) {
        const std::size_t newline = document.find('\n', offset);
        CPPFIG_FUZZ_CHECK(newline != std::string_view::npos);
        offset = newline + 1;
    }
    const std::size_t end = document.find('\n', offset);
    return document.substr(offset, end == std::string_view::npos ? std::string_view::npos : end - offset);
}

struct Entry {
    std::string path;
    std::string text;
};

/// Writes one entry and parses it back; the result must be the same entry.
void CheckRoundTrip(std::string_view path, std::string_view value)
{
    std::string document(path);
    document += " = ";
    cppfig::AppendConfValue(document, value);
    document += '\n';

    std::vector<Entry> entries;
    bool failed = false;
    cppfig::ConfParser::ForEachEntry(
        document, [&entries](const cppfig::ConfEntry& entry) { entries.push_back(Entry { std::string(entry.path), std::string(entry.text) }); },
        [&failed](const cppfig::ConfError&) { failed = true; });
    CPPFIG_FUZZ_CHECK(!failed);
    CPPFIG_FUZZ_CHECK(entries.size() == 1);
    CPPFIG_FUZZ_CHECK(entries[0].path == path);
    CPPFIG_FUZZ_CHECK(entries[0].text == value);
}

}  // namespace

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) -> int
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): libFuzzer's interface takes bytes.
    const std::string_view document(reinterpret_cast<const char*>(data), size);

    std::vector<Entry> entries;
    std::size_t previous_line = 0;
    const auto on_entry = [&](const cppfig::ConfEntry& entry) {
        CPPFIG_FUZZ_CHECK(entry.line > previous_line);
        previous_line = entry.line;

        const auto line = LineAt(document, entry.line);
        CPPFIG_FUZZ_CHECK(entry.column >= 1);
        CPPFIG_FUZZ_CHECK(line.substr(entry.column - 1).starts_with(entry.path));
        CPPFIG_FUZZ_CHECK(entry.value_column > entry.column + entry.path.size());
        CPPFIG_FUZZ_CHECK(entry.value_column <= line.size() + 1);

        entries.push_back(Entry { std::string(entry.path), std::string(entry.text) });
    };
    const auto on_error = [&](const cppfig::ConfError& error) {
        CPPFIG_FUZZ_CHECK(error.line > previous_line);
        previous_line = error.line;
        CPPFIG_FUZZ_CHECK(!error.message.empty());
        CPPFIG_FUZZ_CHECK(error.column >= 1);
        CPPFIG_FUZZ_CHECK(error.column <= LineAt(document, error.line).size() + 1);
    };
    cppfig::ConfParser::ForEachEntry(document, on_entry, on_error);

    for (const auto& entry : entries) {
        CheckRoundTrip(entry.path, entry.text);
    }
    CheckRoundTrip("fuzz.value", document);
    return 0;
}
