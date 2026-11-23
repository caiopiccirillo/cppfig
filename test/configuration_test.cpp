#include <cppfig/cppfig.h>
#include <gtest/gtest.h>
#include <stdlib.h>  // NOLINT(modernize-deprecated-headers): setenv is POSIX, not in <cstdlib>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "test_settings.h"

namespace cppfig::test {
namespace {

    [[nodiscard]] auto CompleteDocument() -> std::string
    {
        return "# leading comment\n"
               "app.name = TestApp\n"
               "server.host = filehost\n"
               "server.port = 9000\n"
               "logging.enabled = false\n"
               "numeric.ratio = 2.5\n"
               "custom.point = 3,4\n"
               "aligned.value = 32\n"
               "# trailing comment\n";
    }

    struct EscapedSetting {
        static constexpr std::string_view path = "text.value";
        using value_type = std::string;
        static auto default_value() -> std::string { return "a#b\n"; }
    };

    struct InvalidDefault {
        static constexpr std::string_view path = "invalid.default";
        using value_type = int;
        static auto default_value() -> int { return 0; }
        static auto validator() { return Min(1); }
    };

    TEST(ConfigurationIOTest, MissingFileUsesDefaultsAndSaveCreatesIt)
    {
        const TempDirectory directory;
        const auto path = directory.File("app.conf");
        auto loaded = LoadFile<TestRegistry>(path.string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_FALSE(std::filesystem::exists(path));
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 8080);
        EXPECT_NE(loaded->document.GetText().find("app.name = MyApplication\n"), std::string_view::npos);

        ASSERT_TRUE(SaveFile(path.string(), loaded->document).ok());
        ASSERT_TRUE(std::filesystem::exists(path));
        EXPECT_EQ(directory.Read("app.conf"), loaded->document.GetText());

        auto reloaded = LoadFile<TestRegistry>(path.string());
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error().message();
        EXPECT_EQ(reloaded->document.GetText(), loaded->document.GetText());
    }

    TEST(ConfigurationIOTest, LoadsTypedFileValuesAndPreservesLayout)
    {
        const TempDirectory directory;
        directory.Write("app.conf", CompleteDocument());
        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();

        EXPECT_EQ(loaded->config.Get<settings::AppName>(), "TestApp");
        EXPECT_EQ(loaded->config.Get<settings::ServerHost>(), "filehost");
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 9000);
        EXPECT_FALSE(loaded->config.Get<settings::LoggingEnabled>());
        EXPECT_DOUBLE_EQ(loaded->config.Get<settings::Ratio>(), 2.5);
        EXPECT_EQ(loaded->config.Get<settings::Origin>(), (Point { 3, 4 }));
        EXPECT_EQ(loaded->config.Get<settings::Aligned>().value, 32);

        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), loaded->document).ok());
        EXPECT_EQ(directory.Read("app.conf"), CompleteDocument());
    }

    TEST(ConfigurationIOTest, MigrationAppendsDefaultsWithoutMovingExistingContent)
    {
        const TempDirectory directory;
        const std::string original = "# keep first\napp.name = Existing\n# keep last\n";
        directory.Write("app.conf", original);
        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::AppName>(), "Existing");
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 8080);
        EXPECT_TRUE(loaded->document.GetText().starts_with(original));
        EXPECT_NE(loaded->document.GetText().find("# Added by cppfig\n"), std::string_view::npos);

        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), loaded->document).ok());
        auto migrated = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(migrated.has_value()) << migrated.error().message();
        EXPECT_EQ(migrated->document.GetText(), loaded->document.GetText());
        EXPECT_EQ(directory.Read("app.conf"), original + "\n# Added by cppfig\n" + "server.host = localhost\nserver.port = 8080\nlogging.enabled = true\n" + "numeric.ratio = 1.5\ncustom.point = 1,2\naligned.value = 16\n");
    }

    TEST(ConfigurationIOTest, RejectsUnknownAndDuplicateKeys)
    {
        const TempDirectory directory;
        directory.Write("unknown.conf", "server.unknown = 1\n");
        const auto unknown_path = directory.File("unknown.conf").string();
        auto unknown = LoadFile<TestRegistry>(unknown_path);
        ASSERT_FALSE(unknown.has_value());
        EXPECT_TRUE(unknown.error().message().starts_with(unknown_path + ":1:1: ")) << unknown.error().message();
        EXPECT_NE(unknown.error().message().find("unknown setting 'server.unknown'"), std::string_view::npos);

        directory.Write("duplicate.conf", "server.port = 1\n  server.port = 2\n");
        const auto duplicate_path = directory.File("duplicate.conf").string();
        auto duplicate = LoadFile<TestRegistry>(duplicate_path);
        ASSERT_FALSE(duplicate.has_value());
        EXPECT_TRUE(duplicate.error().message().starts_with(duplicate_path + ":2:3: ")) << duplicate.error().message();
        EXPECT_NE(duplicate.error().message().find("duplicate setting 'server.port' (first set on line 1)"),
                  std::string_view::npos);
    }

    TEST(ConfigurationIOTest, PreservePolicyKeepsUnknownKeysAndReportsThem)
    {
        const TempDirectory directory;
        const std::string document = CompleteDocument() + "legacy.removed = 1\n# about the next key\nfuture.added = \"x\"\n";
        directory.Write("app.conf", document);

        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string(), { .unknown_keys = UnknownKeyPolicy::kPreserve });
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 9000);
        EXPECT_FALSE(loaded->document.NeedsSave());

        const auto unknown = loaded->document.GetUnknownKeys();
        ASSERT_EQ(unknown.size(), 2U);
        EXPECT_EQ(unknown[0].path, "legacy.removed");
        EXPECT_EQ(unknown[0].line, 10U);
        EXPECT_EQ(unknown[1].path, "future.added");
        EXPECT_EQ(unknown[1].line, 12U);

        std::size_t visited = 0;
        for (const UnknownKey key : unknown) {
            EXPECT_EQ(key.path, unknown[visited].path);
            ++visited;
        }
        EXPECT_EQ(visited, 2U);

        // Keys are positions in the text, so they survive moving the document.
        const ConfDocument moved = std::move(loaded->document);
        EXPECT_EQ(moved.GetUnknownKeys()[1].path, "future.added");

        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), moved).ok());
        EXPECT_EQ(directory.Read("app.conf"), document);
    }

    TEST(ConfigurationIOTest, PreservePolicyStillRejectsDuplicateKnownKeys)
    {
        const TempDirectory directory;
        directory.Write("app.conf", "server.port = 1\nserver.port = 2\n");
        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string(), { .unknown_keys = UnknownKeyPolicy::kPreserve });
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().message().find("duplicate setting"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, RejectPolicyIsTheDefault)
    {
        const TempDirectory directory;
        directory.Write("app.conf", "legacy.removed = 1\n");
        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string(), { .unknown_keys = UnknownKeyPolicy::kReject });
        ASSERT_FALSE(loaded.has_value());
        EXPECT_EQ(LoadOptions {}.unknown_keys, UnknownKeyPolicy::kReject);
    }

    TEST(ConfigurationIOTest, RejectsMalformedSyntaxWithLineInformation)
    {
        const TempDirectory directory;
        directory.Write("app.conf", "# comment\nserver.port 9000\n");
        const auto path = directory.File("app.conf").string();
        auto loaded = LoadFile<TestRegistry>(path);
        ASSERT_FALSE(loaded.has_value());
        EXPECT_TRUE(loaded.error().message().starts_with(path + ":2:1: ")) << loaded.error().message();
        EXPECT_NE(loaded.error().message().find("missing '='"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, ReportsTheColumnOfABadValue)
    {
        const TempDirectory directory;
        directory.Write("app.conf", "server.port =   abc\n");
        const auto path = directory.File("app.conf").string();
        auto loaded = LoadFile<TestRegistry>(path);
        ASSERT_FALSE(loaded.has_value());
        EXPECT_TRUE(loaded.error().message().starts_with(path + ":1:17: ")) << loaded.error().message();
        EXPECT_NE(loaded.error().message().find("'server.port'"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, ReportsTheLineOfAnInvalidFileValue)
    {
        const TempDirectory directory;
        directory.Write("app.conf", "# ports\n\nserver.port = 0\n");
        const auto path = directory.File("app.conf").string();
        auto loaded = LoadFile<TestRegistry>(path);
        ASSERT_FALSE(loaded.has_value());
        EXPECT_TRUE(loaded.error().message().starts_with(path + ":3:15: ")) << loaded.error().message();
        EXPECT_NE(loaded.error().message().find("invalid value for 'server.port'"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, ReportsTheColumnOfAnUnterminatedQuote)
    {
        const TempDirectory directory;
        directory.Write("app.conf", "app.name = \"abc\n");
        const auto path = directory.File("app.conf").string();
        auto loaded = LoadFile<TestRegistry>(path);
        ASSERT_FALSE(loaded.has_value());
        EXPECT_TRUE(loaded.error().message().starts_with(path + ":1:12: ")) << loaded.error().message();
        EXPECT_NE(loaded.error().message().find("unterminated quoted value"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, HashOutsideQuotesStartsAComment)
    {
        const TempDirectory directory;
        const std::string document = "server.port = 9000 # http\n"
                                     "app.name = \"a # b\"  # quoted hash is data\n"
                                     "server.host = example#comment\n";
        directory.Write("app.conf", document);
        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 9000);
        EXPECT_EQ(loaded->config.Get<settings::AppName>(), "a # b");
        EXPECT_EQ(loaded->config.Get<settings::ServerHost>(), "example");
        EXPECT_TRUE(loaded->document.GetText().starts_with(document));
    }

    TEST(ConfigurationIOTest, ParsesQuotedStringsAndEscapes)
    {
        const TempDirectory directory;
        std::string document = CompleteDocument();
        const std::string original = "app.name = TestApp";
        const std::string replacement = R"(app.name = "line 1\n\t\"name\"\\end")";
        document.replace(document.find(original), original.size(), replacement);
        directory.Write("app.conf", document);

        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::AppName>(), "line 1\n\t\"name\"\\end");
        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), loaded->document).ok());
        EXPECT_EQ(directory.Read("app.conf"), document);
    }

    TEST(ConfigurationIOTest, EscapesMigratedStringValues)
    {

        const TempDirectory directory;
        const auto path = directory.File("escaped.conf");
        auto loaded = LoadFile<Registry<EscapedSetting>>(path.string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->document.GetText(), "text.value = \"a#b\\n\"\n");
        ASSERT_TRUE(SaveFile(path.string(), loaded->document).ok());

        auto reloaded = LoadFile<Registry<EscapedSetting>>(path.string());
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error().message();
        EXPECT_EQ(reloaded->config.Get<EscapedSetting>(), "a#b\n");
    }

    TEST(ConfigurationIOTest, CapturesEnvironmentOnceWithoutPersistingOverrides)
    {
        const TempDirectory directory;
        directory.Write("app.conf", CompleteDocument());
        const ScopedEnvironment host("CPPFIG_TEST_SERVER_HOST", "environment-host");
        const ScopedEnvironment port("CPPFIG_TEST_SERVER_PORT", "10000");

        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::ServerHost>(), "environment-host");
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 10000);

        setenv("CPPFIG_TEST_SERVER_HOST", "changed-later", 1);
        setenv("CPPFIG_TEST_SERVER_PORT", "11000", 1);
        EXPECT_EQ(loaded->config.Get<settings::ServerHost>(), "environment-host");
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 10000);

        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), loaded->document).ok());
        EXPECT_EQ(directory.Read("app.conf"), CompleteDocument());
    }

    TEST(ConfigurationIOTest, FromDefaultsDoesNotReadEnvironment)
    {
        const ScopedEnvironment host("CPPFIG_TEST_SERVER_HOST", "environment-host");
        auto loaded = TestConfiguration::FromDefaults();
        ASSERT_TRUE(loaded.has_value());
        EXPECT_EQ(loaded->Get<settings::ServerHost>(), "localhost");
    }

    TEST(ConfigurationIOTest, RejectsInvalidEnvironmentAndFileValues)
    {
        {
            const std::array<EnvironmentVariable, 1> variables { { { .name = "CPPFIG_TEST_SERVER_PORT", .value = "secret-token" } } };
            auto loaded = TestConfiguration::FromText(CompleteDocument(), "app.conf", { .environment = Environment::From(variables) });
            ASSERT_FALSE(loaded);
            // Environment variables often hold secrets, so the value is never quoted.
            EXPECT_EQ(loaded.error().message(),
                      "environment variable CPPFIG_TEST_SERVER_PORT: cannot parse the value for 'server.port'");
        }
        {
            const std::array<EnvironmentVariable, 1> variables { { { .name = "CPPFIG_TEST_SERVER_PORT", .value = "0" } } };
            auto loaded = TestConfiguration::FromText(CompleteDocument(), "app.conf", { .environment = Environment::From(variables) });
            ASSERT_FALSE(loaded);
            EXPECT_NE(loaded.error().message().find("environment variable CPPFIG_TEST_SERVER_PORT: "), std::string_view::npos);
        }

        std::string invalid_file = CompleteDocument();
        const std::string value = "server.port = 9000";
        invalid_file.replace(invalid_file.find(value), value.size(), "server.port = 0");
        const std::array<EnvironmentVariable, 1> valid_override { { { .name = "CPPFIG_TEST_SERVER_PORT", .value = "10000" } } };
        auto loaded = TestConfiguration::FromText(invalid_file, "invalid.conf", { .environment = Environment::From(valid_override) });
        ASSERT_FALSE(loaded);
        EXPECT_NE(loaded.error().message().find("invalid.conf:4:15: invalid value for 'server.port'"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, ReadsOverridesFromAGivenEnvironment)
    {
        const std::array<EnvironmentVariable, 4> variables { {
            { .name = "UNRELATED", .value = "ignored" },
            { .name = "CPPFIG_TEST_SERVER_PORT", .value = "10000" },
            { .name = "CPPFIG_TEST_SERVER_PORT", .value = "20000" },
            { .name = "cppfig_test_server_host", .value = "names are case-sensitive" },
        } };
        auto loaded = TestConfiguration::FromText(CompleteDocument(), "app.conf", { .environment = Environment::From(variables) });
        ASSERT_TRUE(loaded) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 10000);
        EXPECT_EQ(loaded->config.Get<settings::ServerHost>(), "filehost");
        EXPECT_EQ(loaded->document.GetText(), CompleteDocument());
    }

    TEST(ConfigurationIOTest, NoEnvironmentIgnoresTheProcessEnvironment)
    {
        const ScopedEnvironment port("CPPFIG_TEST_SERVER_PORT", "10000");
        auto loaded = TestConfiguration::FromText(CompleteDocument(), "app.conf", { .environment = Environment::None() });
        ASSERT_TRUE(loaded) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 9000);
    }

    TEST(ConfigurationIOTest, RejectsInvalidCustomType)
    {
        const TempDirectory directory;
        std::string document = CompleteDocument();
        const std::string value = "custom.point = 3,4";
        document.replace(document.find(value), value.size(), "custom.point = invalid");
        directory.Write("app.conf", document);
        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().message().find("custom.point"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, NeedsSaveOnlyWhenTheFileIsMissingOrIncomplete)
    {
        const TempDirectory directory;
        auto missing = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(missing.has_value()) << missing.error().message();
        EXPECT_TRUE(missing->document.NeedsSave());
        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), missing->document).ok());

        auto complete = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(complete.has_value()) << complete.error().message();
        EXPECT_FALSE(complete->document.NeedsSave());

        directory.Write("partial.conf", "app.name = Partial\n");
        auto partial = LoadFile<TestRegistry>(directory.File("partial.conf").string());
        ASSERT_TRUE(partial.has_value()) << partial.error().message();
        EXPECT_TRUE(partial->document.NeedsSave());
    }

    TEST(ConfigurationIOTest, LoadedConfigurationHoldsOnlyTypedValues)
    {
        static_assert(sizeof(TestConfiguration) == sizeof(TestRegistry::values_type));

        const TempDirectory directory;
        directory.Write("app.conf", CompleteDocument());
        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();

        // The configuration outlives the document it was loaded with.
        const TestConfiguration config = std::move(loaded->config);
        loaded = InvalidArgumentError("discarded");
        EXPECT_EQ(config.Get<settings::ServerPort>(), 9000);
    }

    TEST(ConfigurationIOTest, SavePreservesExistingFilePermissions)
    {
        namespace fs = std::filesystem;
        const TempDirectory directory;
        directory.Write("app.conf", "app.name = Secret\n");
        fs::permissions(directory.File("app.conf"), fs::perms::owner_read | fs::perms::owner_write);

        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        ASSERT_TRUE(loaded->document.NeedsSave());
        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), loaded->document).ok());

        EXPECT_EQ(fs::status(directory.File("app.conf")).permissions(), fs::perms::owner_read | fs::perms::owner_write);
    }

    TEST(ConfigurationIOTest, SaveWritesThroughSymlinkToItsTarget)
    {
        namespace fs = std::filesystem;
        const TempDirectory directory;
        fs::create_directories(directory.File("real"));
        directory.Write("real/app.conf", "app.name = Linked\n");
        fs::create_symlink(directory.File("real/app.conf"), directory.File("app.conf"));

        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), loaded->document).ok());

        EXPECT_TRUE(fs::is_symlink(fs::symlink_status(directory.File("app.conf"))));
        EXPECT_EQ(directory.Read("real/app.conf"), loaded->document.GetText());
    }

    TEST(ConfigurationIOTest, SaveLeavesNoTemporaryFiles)
    {
        namespace fs = std::filesystem;
        const TempDirectory directory;
        auto loaded = LoadFile<TestRegistry>(directory.File("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), loaded->document).ok());
        ASSERT_TRUE(SaveFile(directory.File("app.conf").string(), loaded->document).ok());

        std::size_t entries = 0;
        for (const auto& entry : fs::directory_iterator(directory.Path())) {
            EXPECT_EQ(entry.path().filename(), "app.conf");
            ++entries;
        }
        EXPECT_EQ(entries, 1U);
    }

    TEST(ConfigurationIOTest, ConcurrentSavesToOnePathNeverInterleave)
    {
        const TempDirectory directory;
        const auto path = directory.File("app.conf").string();
        // Two documents bound to the same destination with different contents.
        directory.Write("app.conf", "app.name = First\n");
        auto first = LoadFile<TestRegistry>(path);
        directory.Write("app.conf", "app.name = Second\n");
        auto second = LoadFile<TestRegistry>(path);
        ASSERT_TRUE(first.has_value() && second.has_value());
        const std::string first_text(first->document.GetText());
        const std::string second_text(second->document.GetText());

        std::atomic<bool> failed = false;
        auto save_repeatedly = [&failed, &path](const ConfDocument& document) {
            for (int i = 0; i < 200; ++i) {
                if (!SaveFile(path, document).ok()) {
                    failed = true;
                }
            }
        };
        std::thread writer_one(save_repeatedly, std::cref(first->document));
        std::thread writer_two(save_repeatedly, std::cref(second->document));
        writer_one.join();
        writer_two.join();

        EXPECT_FALSE(failed);
        const auto final_text = directory.Read("app.conf");
        EXPECT_TRUE(final_text == first_text || final_text == second_text) << final_text;
    }

    TEST(ConfigurationIOTest, FailedSaveLeavesTheOriginalFileIntact)
    {
        namespace fs = std::filesystem;
        if (::geteuid() == 0) {
            GTEST_SKIP() << "permission checks do not apply to root";
        }
        const TempDirectory directory;
        fs::create_directories(directory.File("locked"));
        const std::string original = "app.name = Original\n";
        directory.Write("locked/app.conf", original);

        auto loaded = LoadFile<TestRegistry>(directory.File("locked/app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        fs::permissions(directory.File("locked"), fs::perms::owner_read | fs::perms::owner_exec);
        const auto status = SaveFile(directory.File("locked/app.conf").string(), loaded->document);
        fs::permissions(directory.File("locked"), fs::perms::owner_all);

        EXPECT_FALSE(status.ok());
        EXPECT_EQ(directory.Read("locked/app.conf"), original);
    }

    TEST(ConfigurationIOTest, SaveCreatesParentDirectories)
    {
        const TempDirectory directory;
        const auto path = directory.File("one/two/app.conf");
        auto loaded = LoadFile<TestRegistry>(path.string());
        ASSERT_TRUE(loaded.has_value());
        ASSERT_TRUE(SaveFile(path.string(), loaded->document).ok());
        EXPECT_TRUE(std::filesystem::exists(path));
    }

    TEST(ConfigurationIOTest, LoadsFromTextWithoutAFile)
    {
        auto loaded = TestConfiguration::FromText(CompleteDocument(), "inline");
        ASSERT_TRUE(loaded) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 9000);
        EXPECT_EQ(loaded->document.GetText(), CompleteDocument());
        EXPECT_FALSE(loaded->document.NeedsSave());

        auto invalid = TestConfiguration::FromText("server.port = 0\n", "inline");
        ASSERT_FALSE(invalid);
        EXPECT_TRUE(invalid.error().message().starts_with("inline:1:15: ")) << invalid.error().message();
    }

    TEST(ConfigurationIOTest, ReadsAFileIntoOneExactlySizedBuffer)
    {
        const TempDirectory directory;
        std::string contents;
        for (int i = 0; i < 4096; ++i) {
            contents += "# line " + std::to_string(i) + "\n";
        }
        directory.Write("big.conf", contents);

        auto text = ReadConfFile(directory.File("big.conf"));
        ASSERT_TRUE(text) << text.error().message();
        EXPECT_EQ(*text, contents);
        EXPECT_EQ(text->capacity(), text->size());

        auto missing = ReadConfFile(directory.File("missing.conf"));
        ASSERT_TRUE(missing) << missing.error().message();
        EXPECT_TRUE(missing->empty());

        EXPECT_FALSE(ReadConfFile(directory.Path()));
    }

    TEST(ConfigurationIOTest, ReportsEveryErrorInOnePass)
    {
        const std::string text = "server.port = 0\n"
                                 "missing equals\n"
                                 "unknown.key = 1\n"
                                 "custom.point = nonsense\n"
                                 "server.port = 9000\n";
        const std::array<EnvironmentVariable, 1> variables { { { .name = "CPPFIG_TEST_SERVER_HOST", .value = "" } } };
        auto loaded = TestConfiguration::FromText(text, "app.conf", { .environment = Environment::From(variables) });
        ASSERT_FALSE(loaded);
        EXPECT_EQ(loaded.error().message(), "app.conf:1:15: invalid value for 'server.port': Value 0 is outside [1, 65535]\n"
                                            "app.conf:2:1: missing '=' after the setting path\n"
                                            "app.conf:3:1: unknown setting 'unknown.key'\n"
                                            "app.conf:4:16: cannot parse 'nonsense' as a value of 'custom.point'\n"
                                            "app.conf:5:1: duplicate setting 'server.port' (first set on line 1)");
    }

    TEST(ConfigurationIOTest, StopsListingErrorsAfterTwenty)
    {
        std::string text;
        for (int i = 0; i < 25; ++i) {
            text += "unknown.key" + std::to_string(i) + " = 1\n";
        }
        auto loaded = TestConfiguration::FromText(text, "app.conf", { .environment = Environment::None() });
        ASSERT_FALSE(loaded);
        const auto message = loaded.error().message();
        EXPECT_EQ(std::count(message.begin(), message.end(), '\n'), 20);
        EXPECT_TRUE(message.ends_with("\n(5 more errors not shown)")) << message;
    }

    TEST(ConfigurationIOTest, RuntimeOnlyInvalidDefaultReturnsStatus)
    {

        auto loaded = Configuration<Registry<InvalidDefault>>::FromDefaults();
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().message().find("invalid.default"), std::string_view::npos);
    }

}  // namespace
}  // namespace cppfig::test
