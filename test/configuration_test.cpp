#include <cppfig/cppfig.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

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
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_FALSE(std::filesystem::exists(path));
        EXPECT_EQ(loaded->Get<settings::ServerPort>(), 8080);
        EXPECT_NE(loaded->GetSourceText().find("app.name = MyApplication\n"), std::string_view::npos);

        ASSERT_TRUE(loaded->Save().ok());
        ASSERT_TRUE(std::filesystem::exists(path));
        EXPECT_EQ(directory.read("app.conf"), loaded->GetSourceText());

        auto reloaded = TestConfiguration::Load(path.string());
        ASSERT_TRUE(reloaded.ok()) << reloaded.status().message();
        EXPECT_EQ(reloaded->GetSourceText(), loaded->GetSourceText());
    }

    TEST(ConfigurationIOTest, LoadsTypedFileValuesAndPreservesLayout)
    {
        const TempDirectory directory;
        directory.write("app.conf", CompleteDocument());
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();

        EXPECT_EQ(loaded->Get<settings::AppName>(), "TestApp");
        EXPECT_EQ(loaded->Get<settings::ServerHost>(), "filehost");
        EXPECT_EQ(loaded->Get<settings::ServerPort>(), 9000);
        EXPECT_FALSE(loaded->Get<settings::LoggingEnabled>());
        EXPECT_DOUBLE_EQ(loaded->Get<settings::Ratio>(), 2.5);
        EXPECT_EQ(loaded->Get<settings::Origin>(), (Point { 3, 4 }));
        EXPECT_EQ(loaded->Get<settings::Aligned>().value, 32);

        ASSERT_TRUE(loaded->Save().ok());
        EXPECT_EQ(directory.read("app.conf"), CompleteDocument());
    }

    TEST(ConfigurationIOTest, MigrationAppendsDefaultsWithoutMovingExistingContent)
    {
        const TempDirectory directory;
        const std::string original = "# keep first\napp.name = Existing\n# keep last\n";
        directory.write("app.conf", original);
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_EQ(loaded->Get<settings::AppName>(), "Existing");
        EXPECT_EQ(loaded->Get<settings::ServerPort>(), 8080);
        EXPECT_TRUE(loaded->GetSourceText().starts_with(original));
        EXPECT_NE(loaded->GetSourceText().find("# Added by cppfig\n"), std::string_view::npos);

        ASSERT_TRUE(loaded->Save().ok());
        auto migrated = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(migrated.ok()) << migrated.status().message();
        EXPECT_EQ(migrated->GetSourceText(), loaded->GetSourceText());
        EXPECT_EQ(directory.read("app.conf"), original + "\n# Added by cppfig\n" +
                                                       "server.host = localhost\nserver.port = 8080\nlogging.enabled = true\n" +
                                                       "numeric.ratio = 1.5\ncustom.point = 1,2\naligned.value = 16\n");
    }

    TEST(ConfigurationIOTest, RejectsUnknownAndDuplicateKeys)
    {
        const TempDirectory directory;
        directory.write("unknown.conf", "server.unknown = 1\n");
        auto unknown = TestConfiguration::Load(directory.file("unknown.conf").string());
        ASSERT_FALSE(unknown.ok());
        EXPECT_NE(unknown.status().message().find("Unknown configuration setting"), std::string_view::npos);
        EXPECT_NE(unknown.status().message().find("line 1"), std::string_view::npos);

        directory.write("duplicate.conf", "server.port = 1\nserver.port = 2\n");
        auto duplicate = TestConfiguration::Load(directory.file("duplicate.conf").string());
        ASSERT_FALSE(duplicate.ok());
        EXPECT_NE(duplicate.status().message().find("Duplicate configuration setting"), std::string_view::npos);
        EXPECT_NE(duplicate.status().message().find("line 2"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, RejectsMalformedSyntaxWithLineInformation)
    {
        const TempDirectory directory;
        directory.write("app.conf", "# comment\nserver.port 9000\n");
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_FALSE(loaded.ok());
        EXPECT_NE(loaded.status().message().find("line 2"), std::string_view::npos);
        EXPECT_NE(loaded.status().message().find("missing '='"), std::string_view::npos);
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
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_EQ(loaded->Get<settings::AppName>(), "line 1\n\t\"name\"\\end");
        ASSERT_TRUE(loaded->Save().ok());
        EXPECT_EQ(directory.read("app.conf"), document);
    }

    TEST(ConfigurationIOTest, EscapesMigratedStringValues)
    {

        const TempDirectory directory;
        const auto path = directory.file("escaped.conf");
        auto loaded = Configuration<Registry<EscapedSetting>>::Load(path.string());
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_EQ(loaded->GetSourceText(), "text.value = \"a#b\\n\"\n");
        ASSERT_TRUE(loaded->Save().ok());

        auto reloaded = Configuration<Registry<EscapedSetting>>::Load(path.string());
        ASSERT_TRUE(reloaded.ok()) << reloaded.status().message();
        EXPECT_EQ(reloaded->Get<EscapedSetting>(), "a#b\n");
    }

    TEST(ConfigurationIOTest, CapturesEnvironmentOnceWithoutPersistingOverrides)
    {
        const TempDirectory directory;
        directory.write("app.conf", CompleteDocument());
        const ScopedEnvironment host("CPPFIG_TEST_SERVER_HOST", "environment-host");
        const ScopedEnvironment port("CPPFIG_TEST_SERVER_PORT", "10000");

        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_EQ(loaded->Get<settings::ServerHost>(), "environment-host");
        EXPECT_EQ(loaded->Get<settings::ServerPort>(), 10000);

        setenv("CPPFIG_TEST_SERVER_HOST", "changed-later", 1);
        setenv("CPPFIG_TEST_SERVER_PORT", "11000", 1);
        EXPECT_EQ(loaded->Get<settings::ServerHost>(), "environment-host");
        EXPECT_EQ(loaded->Get<settings::ServerPort>(), 10000);

        ASSERT_TRUE(loaded->Save().ok());
        EXPECT_EQ(directory.read("app.conf"), CompleteDocument());
    }

    TEST(ConfigurationIOTest, FromDefaultsDoesNotReadEnvironment)
    {
        const ScopedEnvironment host("CPPFIG_TEST_SERVER_HOST", "environment-host");
        auto loaded = TestConfiguration::FromDefaults();
        ASSERT_TRUE(loaded.ok());
        EXPECT_EQ(loaded->Get<settings::ServerHost>(), "localhost");
        EXPECT_FALSE(loaded->Save().ok());
    }

    TEST(ConfigurationIOTest, RejectsInvalidEnvironmentAndFileValues)
    {
        const TempDirectory directory;
        directory.write("app.conf", CompleteDocument());
        {
            const ScopedEnvironment port("CPPFIG_TEST_SERVER_PORT", "not-a-number");
            auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
            ASSERT_FALSE(loaded.ok());
            EXPECT_NE(loaded.status().message().find("environment override"), std::string_view::npos);
        }
        {
            const ScopedEnvironment port("CPPFIG_TEST_SERVER_PORT", "0");
            auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
            ASSERT_FALSE(loaded.ok());
            EXPECT_NE(loaded.status().message().find("environment override"), std::string_view::npos);
        }

        std::string invalid_file = CompleteDocument();
        const std::string value = "server.port = 9000";
        invalid_file.replace(invalid_file.find(value), value.size(), "server.port = 0");
        directory.write("invalid.conf", invalid_file);
        const ScopedEnvironment valid_override("CPPFIG_TEST_SERVER_PORT", "10000");
        auto loaded = TestConfiguration::Load(directory.file("invalid.conf").string());
        ASSERT_FALSE(loaded.ok());
        EXPECT_NE(loaded.status().message().find("file value"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, RejectsInvalidCustomType)
    {
        const TempDirectory directory;
        std::string document = CompleteDocument();
        const std::string value = "custom.point = 3,4";
        document.replace(document.find(value), value.size(), "custom.point = invalid");
        directory.write("app.conf", document);
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_FALSE(loaded.ok());
        EXPECT_NE(loaded.status().message().find("custom.point"), std::string_view::npos);
    }

    TEST(ConfigurationIOTest, SaveCreatesParentDirectories)
    {
        const TempDirectory directory;
        const auto path = directory.file("one/two/app.conf");
        auto loaded = TestConfiguration::Load(path.string());
        ASSERT_TRUE(loaded.ok());
        ASSERT_TRUE(loaded->Save().ok());
        EXPECT_TRUE(std::filesystem::exists(path));
    }

    TEST(ConfigurationIOTest, RuntimeOnlyInvalidDefaultReturnsStatus)
    {

        auto loaded = Configuration<Registry<InvalidDefault>>::FromDefaults();
        ASSERT_FALSE(loaded.ok());
        EXPECT_NE(loaded.status().message().find("invalid.default"), std::string_view::npos);
    }

}  // namespace
}  // namespace cppfig::test
