#include <cppfig/cppfig.h>
#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

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
        const auto path = directory.file("app.conf");
        auto loaded = TestConfiguration::Load(path.string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_FALSE(std::filesystem::exists(path));
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 8080);
        EXPECT_NE(loaded->document.GetSourceText().find("app.name = MyApplication\n"), std::string_view::npos);

        ASSERT_TRUE(loaded->document.Save().ok());
        ASSERT_TRUE(std::filesystem::exists(path));
        EXPECT_EQ(directory.read("app.conf"), loaded->document.GetSourceText());

        auto reloaded = TestConfiguration::Load(path.string());
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error().message();
        EXPECT_EQ(reloaded->document.GetSourceText(), loaded->document.GetSourceText());
    }

    TEST(ConfigurationIOTest, LoadsTypedFileValuesAndPreservesLayout)
    {
        const TempDirectory directory;
        directory.write("app.conf", CompleteDocument());
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();

        EXPECT_EQ(loaded->config.Get<settings::AppName>(), "TestApp");
        EXPECT_EQ(loaded->config.Get<settings::ServerHost>(), "filehost");
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 9000);
        EXPECT_FALSE(loaded->config.Get<settings::LoggingEnabled>());
        EXPECT_DOUBLE_EQ(loaded->config.Get<settings::Ratio>(), 2.5);
        EXPECT_EQ(loaded->config.Get<settings::Origin>(), (Point { 3, 4 }));
        EXPECT_EQ(loaded->config.Get<settings::Aligned>().value, 32);

        ASSERT_TRUE(loaded->document.Save().ok());
        EXPECT_EQ(directory.read("app.conf"), CompleteDocument());
    }

    TEST(ConfigurationIOTest, MigrationAppendsDefaultsWithoutMovingExistingContent)
    {
        const TempDirectory directory;
        const std::string original = "# keep first\napp.name = Existing\n# keep last\n";
        directory.write("app.conf", original);
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::AppName>(), "Existing");
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 8080);
        EXPECT_TRUE(loaded->document.GetSourceText().starts_with(original));
        EXPECT_NE(loaded->document.GetSourceText().find("# Added by cppfig\n"), std::string_view::npos);

        ASSERT_TRUE(loaded->document.Save().ok());
        auto migrated = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(migrated.has_value()) << migrated.error().message();
        EXPECT_EQ(migrated->document.GetSourceText(), loaded->document.GetSourceText());
        EXPECT_EQ(directory.read("app.conf"), original + "\n# Added by cppfig\n" + "server.host = localhost\nserver.port = 8080\nlogging.enabled = true\n" + "numeric.ratio = 1.5\ncustom.point = 1,2\naligned.value = 16\n");
    }

    TEST(ConfigurationIOTest, RejectsUnknownAndDuplicateKeys)
    {
        const TempDirectory directory;
        directory.write("unknown.conf", "server.unknown = 1\n");
        const auto unknown_path = directory.file("unknown.conf").string();
        auto unknown = TestConfiguration::Load(unknown_path);
        ASSERT_FALSE(unknown.has_value());
        EXPECT_TRUE(unknown.error().message().starts_with(unknown_path + ":1:1: ")) << unknown.error().message();
        EXPECT_NE(unknown.error().message().find("Unknown configuration setting 'server.unknown'"), std::string_view::npos);

        directory.write("duplicate.conf", "server.port = 1\n  server.port = 2\n");
        const auto duplicate_path = directory.file("duplicate.conf").string();
        auto duplicate = TestConfiguration::Load(duplicate_path);
        ASSERT_FALSE(duplicate.has_value());
        EXPECT_TRUE(duplicate.error().message().starts_with(duplicate_path + ":2:3: ")) << duplicate.error().message();
        EXPECT_NE(duplicate.error().message().find("Duplicate configuration setting 'server.port' (first set on line 1)"),
                  std::string_view::npos);
    }

    TEST(ConfigurationIOTest, PreservePolicyKeepsUnknownKeysAndReportsThem)
    {
        const TempDirectory directory;
        const std::string document = CompleteDocument() + "legacy.removed = 1\n# about the next key\nfuture.added = \"x\"\n";
        directory.write("app.conf", document);

        auto loaded = TestConfiguration::Load(directory.file("app.conf").string(), { .unknown_keys = UnknownKeyPolicy::kPreserve });
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 9000);
        EXPECT_FALSE(loaded->document.NeedsSave());

        const auto unknown = loaded->document.GetUnknownKeys();
        ASSERT_EQ(unknown.size(), 2U);
        EXPECT_EQ(unknown[0].path, "legacy.removed");
        EXPECT_EQ(unknown[0].line, 10U);
        EXPECT_EQ(unknown[1].path, "future.added");
        EXPECT_EQ(unknown[1].line, 12U);

        ASSERT_TRUE(loaded->document.Save().ok());
        EXPECT_EQ(directory.read("app.conf"), document);
    }

    TEST(ConfigurationIOTest, PreservePolicyStillRejectsDuplicateKnownKeys)
    {
        const TempDirectory directory;
        directory.write("app.conf", "server.port = 1\nserver.port = 2\n");
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string(), { .unknown_keys = UnknownKeyPolicy::kPreserve });
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().message().find("Duplicate configuration setting"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, RejectPolicyIsTheDefault)
    {
        const TempDirectory directory;
        directory.write("app.conf", "legacy.removed = 1\n");
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string(), { .unknown_keys = UnknownKeyPolicy::kReject });
        ASSERT_FALSE(loaded.has_value());
        EXPECT_EQ(LoadOptions {}.unknown_keys, UnknownKeyPolicy::kReject);
    }

    TEST(ConfigurationIOTest, RejectsMalformedSyntaxWithLineInformation)
    {
        const TempDirectory directory;
        directory.write("app.conf", "# comment\nserver.port 9000\n");
        const auto path = directory.file("app.conf").string();
        auto loaded = TestConfiguration::Load(path);
        ASSERT_FALSE(loaded.has_value());
        EXPECT_TRUE(loaded.error().message().starts_with(path + ":2:1: ")) << loaded.error().message();
        EXPECT_NE(loaded.error().message().find("missing '='"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, ReportsTheColumnOfABadValue)
    {
        const TempDirectory directory;
        directory.write("app.conf", "server.port =   abc\n");
        const auto path = directory.file("app.conf").string();
        auto loaded = TestConfiguration::Load(path);
        ASSERT_FALSE(loaded.has_value());
        EXPECT_TRUE(loaded.error().message().starts_with(path + ":1:17: ")) << loaded.error().message();
        EXPECT_NE(loaded.error().message().find("'server.port'"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, ReportsTheLineOfAnInvalidFileValue)
    {
        const TempDirectory directory;
        directory.write("app.conf", "# ports\n\nserver.port = 0\n");
        const auto path = directory.file("app.conf").string();
        auto loaded = TestConfiguration::Load(path);
        ASSERT_FALSE(loaded.has_value());
        EXPECT_TRUE(loaded.error().message().starts_with(path + ":3:15: ")) << loaded.error().message();
        EXPECT_NE(loaded.error().message().find("file value is invalid"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, ReportsTheColumnOfAnUnterminatedQuote)
    {
        const TempDirectory directory;
        directory.write("app.conf", "app.name = \"abc\n");
        const auto path = directory.file("app.conf").string();
        auto loaded = TestConfiguration::Load(path);
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
        directory.write("app.conf", document);
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 9000);
        EXPECT_EQ(loaded->config.Get<settings::AppName>(), "a # b");
        EXPECT_EQ(loaded->config.Get<settings::ServerHost>(), "example");
        EXPECT_TRUE(loaded->document.GetSourceText().starts_with(document));
    }

    TEST(ConfigurationIOTest, ParsesQuotedStringsAndEscapes)
    {
        const TempDirectory directory;
        std::string document = CompleteDocument();
        const std::string original = "app.name = TestApp";
        const std::string replacement = R"(app.name = "line 1\n\t\"name\"\\end")";
        document.replace(document.find(original), original.size(), replacement);
        directory.write("app.conf", document);

        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::AppName>(), "line 1\n\t\"name\"\\end");
        ASSERT_TRUE(loaded->document.Save().ok());
        EXPECT_EQ(directory.read("app.conf"), document);
    }

    TEST(ConfigurationIOTest, EscapesMigratedStringValues)
    {

        const TempDirectory directory;
        const auto path = directory.file("escaped.conf");
        auto loaded = Configuration<Registry<EscapedSetting>>::Load(path.string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->document.GetSourceText(), "text.value = \"a#b\\n\"\n");
        ASSERT_TRUE(loaded->document.Save().ok());

        auto reloaded = Configuration<Registry<EscapedSetting>>::Load(path.string());
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error().message();
        EXPECT_EQ(reloaded->config.Get<EscapedSetting>(), "a#b\n");
    }

    TEST(ConfigurationIOTest, CapturesEnvironmentOnceWithoutPersistingOverrides)
    {
        const TempDirectory directory;
        directory.write("app.conf", CompleteDocument());
        const ScopedEnvironment host("CPPFIG_TEST_SERVER_HOST", "environment-host");
        const ScopedEnvironment port("CPPFIG_TEST_SERVER_PORT", "10000");

        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->config.Get<settings::ServerHost>(), "environment-host");
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 10000);

        setenv("CPPFIG_TEST_SERVER_HOST", "changed-later", 1);
        setenv("CPPFIG_TEST_SERVER_PORT", "11000", 1);
        EXPECT_EQ(loaded->config.Get<settings::ServerHost>(), "environment-host");
        EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 10000);

        ASSERT_TRUE(loaded->document.Save().ok());
        EXPECT_EQ(directory.read("app.conf"), CompleteDocument());
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
        const TempDirectory directory;
        directory.write("app.conf", CompleteDocument());
        {
            const ScopedEnvironment port("CPPFIG_TEST_SERVER_PORT", "not-a-number");
            auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
            ASSERT_FALSE(loaded.has_value());
            EXPECT_NE(loaded.error().message().find("environment override"), std::string_view::npos);
        }
        {
            const ScopedEnvironment port("CPPFIG_TEST_SERVER_PORT", "0");
            auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
            ASSERT_FALSE(loaded.has_value());
            EXPECT_NE(loaded.error().message().find("environment override"), std::string_view::npos);
        }

        std::string invalid_file = CompleteDocument();
        const std::string value = "server.port = 9000";
        invalid_file.replace(invalid_file.find(value), value.size(), "server.port = 0");
        directory.write("invalid.conf", invalid_file);
        const ScopedEnvironment valid_override("CPPFIG_TEST_SERVER_PORT", "10000");
        auto loaded = TestConfiguration::Load(directory.file("invalid.conf").string());
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().message().find("file value"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, RejectsInvalidCustomType)
    {
        const TempDirectory directory;
        std::string document = CompleteDocument();
        const std::string value = "custom.point = 3,4";
        document.replace(document.find(value), value.size(), "custom.point = invalid");
        directory.write("app.conf", document);
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().message().find("custom.point"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, NeedsSaveOnlyWhenTheFileIsMissingOrIncomplete)
    {
        const TempDirectory directory;
        auto missing = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(missing.has_value()) << missing.error().message();
        EXPECT_TRUE(missing->document.NeedsSave());
        ASSERT_TRUE(missing->document.Save().ok());

        auto complete = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(complete.has_value()) << complete.error().message();
        EXPECT_FALSE(complete->document.NeedsSave());

        directory.write("partial.conf", "app.name = Partial\n");
        auto partial = TestConfiguration::Load(directory.file("partial.conf").string());
        ASSERT_TRUE(partial.has_value()) << partial.error().message();
        EXPECT_TRUE(partial->document.NeedsSave());
    }

    TEST(ConfigurationIOTest, LoadedConfigurationHoldsOnlyTypedValues)
    {
        static_assert(sizeof(TestConfiguration) == sizeof(TestRegistry::values_type));

        const TempDirectory directory;
        directory.write("app.conf", CompleteDocument());
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_EQ(loaded->document.GetFilePath(), directory.file("app.conf").string());

        // The configuration outlives the document it was loaded with.
        TestConfiguration config = std::move(loaded->config);
        loaded = InvalidArgumentError("discarded");
        EXPECT_EQ(config.Get<settings::ServerPort>(), 9000);
    }

    TEST(ConfigurationIOTest, SavePreservesExistingFilePermissions)
    {
        namespace fs = std::filesystem;
        const TempDirectory directory;
        directory.write("app.conf", "app.name = Secret\n");
        fs::permissions(directory.file("app.conf"), fs::perms::owner_read | fs::perms::owner_write);

        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        ASSERT_TRUE(loaded->document.NeedsSave());
        ASSERT_TRUE(loaded->document.Save().ok());

        EXPECT_EQ(fs::status(directory.file("app.conf")).permissions(), fs::perms::owner_read | fs::perms::owner_write);
    }

    TEST(ConfigurationIOTest, SaveWritesThroughSymlinkToItsTarget)
    {
        namespace fs = std::filesystem;
        const TempDirectory directory;
        fs::create_directories(directory.file("real"));
        directory.write("real/app.conf", "app.name = Linked\n");
        fs::create_symlink(directory.file("real/app.conf"), directory.file("app.conf"));

        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        ASSERT_TRUE(loaded->document.Save().ok());

        EXPECT_TRUE(fs::is_symlink(fs::symlink_status(directory.file("app.conf"))));
        EXPECT_EQ(directory.read("real/app.conf"), loaded->document.GetSourceText());
    }

    TEST(ConfigurationIOTest, SaveLeavesNoTemporaryFiles)
    {
        namespace fs = std::filesystem;
        const TempDirectory directory;
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        ASSERT_TRUE(loaded->document.Save().ok());
        ASSERT_TRUE(loaded->document.Save().ok());

        std::size_t entries = 0;
        for (const auto& entry : fs::directory_iterator(directory.path())) {
            EXPECT_EQ(entry.path().filename(), "app.conf");
            ++entries;
        }
        EXPECT_EQ(entries, 1U);
    }

    TEST(ConfigurationIOTest, ConcurrentSavesToOnePathNeverInterleave)
    {
        const TempDirectory directory;
        const auto path = directory.file("app.conf").string();
        // Two documents bound to the same destination with different contents.
        directory.write("app.conf", "app.name = First\n");
        auto first = TestConfiguration::Load(path);
        directory.write("app.conf", "app.name = Second\n");
        auto second = TestConfiguration::Load(path);
        ASSERT_TRUE(first.has_value() && second.has_value());
        const std::string first_text(first->document.GetSourceText());
        const std::string second_text(second->document.GetSourceText());

        std::atomic<bool> failed = false;
        auto save_repeatedly = [&failed](const ConfDocument& document) {
            for (int i = 0; i < 200; ++i) {
                if (!document.Save().ok()) {
                    failed = true;
                }
            }
        };
        std::thread writer_one(save_repeatedly, std::cref(first->document));
        std::thread writer_two(save_repeatedly, std::cref(second->document));
        writer_one.join();
        writer_two.join();

        EXPECT_FALSE(failed);
        const auto final_text = directory.read("app.conf");
        EXPECT_TRUE(final_text == first_text || final_text == second_text) << final_text;
    }

    TEST(ConfigurationIOTest, FailedSaveLeavesTheOriginalFileIntact)
    {
        namespace fs = std::filesystem;
        if (::geteuid() == 0) {
            GTEST_SKIP() << "permission checks do not apply to root";
        }
        const TempDirectory directory;
        fs::create_directories(directory.file("locked"));
        const std::string original = "app.name = Original\n";
        directory.write("locked/app.conf", original);

        auto loaded = TestConfiguration::Load(directory.file("locked/app.conf").string());
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        fs::permissions(directory.file("locked"), fs::perms::owner_read | fs::perms::owner_exec);
        const auto status = loaded->document.Save();
        fs::permissions(directory.file("locked"), fs::perms::owner_all);

        EXPECT_FALSE(status.ok());
        EXPECT_EQ(directory.read("locked/app.conf"), original);
    }

    TEST(ConfigurationIOTest, SaveCreatesParentDirectories)
    {
        const TempDirectory directory;
        const auto path = directory.file("one/two/app.conf");
        auto loaded = TestConfiguration::Load(path.string());
        ASSERT_TRUE(loaded.has_value());
        ASSERT_TRUE(loaded->document.Save().ok());
        EXPECT_TRUE(std::filesystem::exists(path));
    }

    TEST(ConfigurationIOTest, RuntimeOnlyInvalidDefaultReturnsStatus)
    {

        auto loaded = Configuration<Registry<InvalidDefault>>::FromDefaults();
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().message().find("invalid.default"), std::string_view::npos);
    }

}  // namespace
}  // namespace cppfig::test
