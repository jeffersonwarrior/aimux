// v2.2 Task 5: HTML Structure Tests
// Test suite for validating index.html prettifier card integration

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>

class HTMLValidator {
public:
    explicit HTMLValidator(const std::string& html_file_path) {
        std::ifstream file(html_file_path);
        if (!file.is_open()) {
            file_exists_ = false;
            return;
        }
        file_exists_ = true;
        std::stringstream buffer;
        buffer << file.rdbuf();
        html_content_ = buffer.str();
        file.close();
    }

    bool fileExists() const { return file_exists_; }

    bool hasElementWithId(const std::string& id) const {
        std::string pattern = "id=\"" + id + "\"";
        return html_content_.find(pattern) != std::string::npos;
    }

    bool hasClass(const std::string& class_name) const {
        std::string pattern = "class=\"[^\"]*" + class_name + "[^\"]*\"";
        std::regex class_regex(pattern);
        return std::regex_search(html_content_, class_regex);
    }

    bool hasScriptTag(const std::string& src) const {
        std::string pattern = "<script[^>]*src=\"" + src + "\"";
        return html_content_.find(pattern) != std::string::npos;
    }

    bool hasStylesheet(const std::string& href) const {
        std::string pattern = "href=\"" + href + "\"";
        return html_content_.find(pattern) != std::string::npos;
    }

    const std::string& getContent() const { return html_content_; }

private:
    bool file_exists_;
    std::string html_content_;
};

class HTMLStructureTest : public ::testing::Test {
protected:
    void SetUp() override {
        html_file_path_ = "../webui/index.html";
        validator_ = std::make_unique<HTMLValidator>(html_file_path_);
    }

    std::string html_file_path_;
    std::unique_ptr<HTMLValidator> validator_;
};

// Test 1-4: Basic structure
TEST_F(HTMLStructureTest, HTMLFile_ExistsAndLoads) {
    EXPECT_TRUE(validator_->fileExists());
}

TEST_F(HTMLStructureTest, HTMLFile_HasPrettifierCard) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasElementWithId("prettifier-card"));
}

TEST_F(HTMLStructureTest, HTMLFile_HasStatusBadge) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasElementWithId("prettifier-status"));
}

TEST_F(HTMLStructureTest, HTMLFile_HasMetrics) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasElementWithId("fmt-speed"));
    EXPECT_TRUE(validator_->hasElementWithId("fmt-throughput"));
    EXPECT_TRUE(validator_->hasElementWithId("fmt-success"));
}

// Test 5-8: Format preferences
TEST_F(HTMLStructureTest, HTMLFile_HasFormatSelectors) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasElementWithId("anthropic-format"));
    EXPECT_TRUE(validator_->hasElementWithId("openai-format"));
    EXPECT_TRUE(validator_->hasElementWithId("cerebras-format"));
}

TEST_F(HTMLStructureTest, HTMLFile_HasConfigInputs) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasElementWithId("streaming-enabled"));
    EXPECT_TRUE(validator_->hasElementWithId("buffer-size"));
    EXPECT_TRUE(validator_->hasElementWithId("timeout-ms"));
}

TEST_F(HTMLStructureTest, HTMLFile_HasActionButtons) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasElementWithId("apply-config"));
    EXPECT_TRUE(validator_->hasElementWithId("reset-config"));
    EXPECT_TRUE(validator_->hasElementWithId("refresh-status"));
}

TEST_F(HTMLStructureTest, HTMLFile_HasMessageBox) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasElementWithId("config-message"));
}

// Test 9-12: CSS and JS includes
TEST_F(HTMLStructureTest, HTMLFile_IncludesPrettifierCSS) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasStylesheet("css/prettifier.css"));
}

TEST_F(HTMLStructureTest, HTMLFile_IncludesAPIClientJS) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasScriptTag("js/api-client.js"));
}

TEST_F(HTMLStructureTest, HTMLFile_IncludesPrettifierUIJS) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasScriptTag("js/prettifier-ui.js"));
}

TEST_F(HTMLStructureTest, HTMLFile_InitializesUI) {
    ASSERT_TRUE(validator_->fileExists());
    const std::string& content = validator_->getContent();
    EXPECT_TRUE(content.find("PrettifierAPIClient") != std::string::npos);
    EXPECT_TRUE(content.find("PrettifierUI") != std::string::npos);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << "\n=== HTML Structure Tests ===" << std::endl;
    return RUN_ALL_TESTS();
}
