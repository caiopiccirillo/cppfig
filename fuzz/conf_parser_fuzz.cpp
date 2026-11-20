// Fuzzes the .conf parser and its writer.
//
// Properties:
// - parsing any input either succeeds or fails with a `source:line:column:`
//   error whose location lies inside the document;
// - every entry's line and columns point at its path and value in the input;
// - every value the parser reads, and the whole input taken as one value,
//   survives writing with AppendConfValue and reading back unchanged.

#include <cppfig/conf.h>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "fuzz_check.h"

namespace {

constexpr std::string_view kSource = "fuzz.conf";

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

auto ReadNumber(std::string_view& text) -> std::size_t
{
    std::size_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    CPPFIG_FUZZ_CHECK(result.ec == std::errc {});
    text.remove_prefix(static_cast<std::size_t>(result.ptr - text.data()));
    return value;
}

/// An error must name the source, then a line and column inside the document.
void CheckErrorLocation(std::string_view document, std::string_view message)
{
    CPPFIG_FUZZ_CHECK(message.starts_with(kSource));
    message.remove_prefix(kSource.size());
    CPPFIG_FUZZ_CHECK(message.starts_with(':'));
    message.remove_prefix(1);
    const std::size_t line = ReadNumber(message);
    CPPFIG_FUZZ_CHECK(message.starts_with(':'));
    message.remove_prefix(1);
    const std::size_t column = ReadNumber(message);
    CPPFIG_FUZZ_CHECK(message.starts_with(": "));

    CPPFIG_FUZZ_CHECK(line >= 1);
    CPPFIG_FUZZ_CHECK(column >= 1);
    CPPFIG_FUZZ_CHECK(column <= LineAt(document, line).size() + 1);
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
    const auto status = cppfig::ConfParser::ForEachEntry(document, kSource, [&entries](const cppfig::ConfEntry& entry) {
        entries.push_back(Entry { std::string(entry.path), std::string(entry.text) });
        return cppfig::OkStatus();
    });
    CPPFIG_FUZZ_CHECK(status.ok());
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
    const auto status = cppfig::ConfParser::ForEachEntry(document, kSource, [&](const cppfig::ConfEntry& entry) {
        CPPFIG_FUZZ_CHECK(entry.line > previous_line);
        previous_line = entry.line;

        const auto line = LineAt(document, entry.line);
        CPPFIG_FUZZ_CHECK(entry.column >= 1);
        CPPFIG_FUZZ_CHECK(line.substr(entry.column - 1).starts_with(entry.path));
        CPPFIG_FUZZ_CHECK(entry.value_column > entry.column + entry.path.size());
        CPPFIG_FUZZ_CHECK(entry.value_column <= line.size() + 1);

        entries.push_back(Entry { std::string(entry.path), std::string(entry.text) });
        return cppfig::OkStatus();
    });
    if (!status.ok()) {
        CheckErrorLocation(document, status.message());
    }

    for (const auto& entry : entries) {
        CheckRoundTrip(entry.path, entry.text);
    }
    CheckRoundTrip("fuzz.value", document);
    return 0;
}
