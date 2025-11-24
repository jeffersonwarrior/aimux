// v2.2 Task 4: UI Controller Tests
// Test suite for prettifier-ui.js validation
// Expected: 18 test cases covering UI initialization, status loading, config application

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>

// Helper class for JavaScript UI validation
class UIJavaScriptValidator {
public:
    explicit UIJavaScriptValidator(const std::string& js_file_path) {
        std::ifstream file(js_file_path);
        if (!file.is_open()) {
            file_exists_ = false;
            return;
        }

        file_exists_ = true;
        std::stringstream buffer;
        buffer << file.rdbuf();
        js_content_ = buffer.str();
        file.close();
    }

    bool fileExists() const { return file_exists_; }

    bool hasClass(const std::string& class_name) const {
        std::string pattern = "class\\s+" + class_name;
        std::regex class_regex(pattern);
        return std::regex_search(js_content_, class_regex);
    }

    bool hasMethod(const std::string& method_name) const {
        std::vector<std::string> patterns = {
            "async\\s+" + method_name + "\\s*\\(",
            method_name + "\\s*\\([^)]*\\)\\s*\\{"
        };

        for (const auto& pattern : patterns) {
            std::regex method_regex(pattern);
            if (std::regex_search(js_content_, method_regex)) {
                return true;
            }
        }
        return false;
    }

    bool hasAutoRefresh() const {
        return js_content_.find("setInterval") != std::string::npos &&
               js_content_.find("10000") != std::string::npos; // 10 second interval
    }

    bool hasEventListeners() const {
        return js_content_.find("addEventListener") != std::string::npos;
    }

    bool hasValidation() const {
        return js_content_.find("validate") != std::string::npos;
    }

    const std::string& getContent() const { return js_content_; }

private:
    bool file_exists_;
    std::string js_content_;
};

// Test fixture
class UIControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        js_file_path_ = "../webui/js/prettifier-ui.js";
        validator_ = std::make_unique<UIJavaScriptValidator>(js_file_path_);
    }

    std::string js_file_path_;
    std::unique_ptr<UIJavaScriptValidator> validator_;
};

// Test 1-5: Core structure
TEST_F(UIControllerTest, JSFile_ExistsAndLoads) {
    EXPECT_TRUE(validator_->fileExists());
}

TEST_F(UIControllerTest, JSFile_HasPrettifierUIClass) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasClass("PrettifierUI"));
}

TEST_F(UIControllerTest, JSFile_HasInitMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("init"));
}

TEST_F(UIControllerTest, JSFile_HasLoadStatusMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("loadStatus"));
}

TEST_F(UIControllerTest, JSFile_HasUpdateUIMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("updateUI"));
}

// Test 6-10: Configuration and UI updates
TEST_F(UIControllerTest, JSFile_HasApplyConfigMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("applyConfig"));
}

TEST_F(UIControllerTest, JSFile_HasShowMessageMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("showMessage"));
}

TEST_F(UIControllerTest, JSFile_HasValidationLogic) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasValidation());
}

TEST_F(UIControllerTest, JSFile_HasEventListeners) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasEventListeners());
}

TEST_F(UIControllerTest, JSFile_HasAutoRefresh) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasAutoRefresh());
}

// Test 11-15: Helper methods
TEST_F(UIControllerTest, JSFile_HasCacheElementsMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("cacheElements"));
}

TEST_F(UIControllerTest, JSFile_HasCollectConfigMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("collectConfiguration"));
}

TEST_F(UIControllerTest, JSFile_HasManualRefreshMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("manualRefresh"));
}

TEST_F(UIControllerTest, JSFile_HasStopAutoRefreshMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("stopAutoRefresh"));
}

TEST_F(UIControllerTest, JSFile_HasCleanupMethod) {
    ASSERT_TRUE(validator_->fileExists());
    EXPECT_TRUE(validator_->hasMethod("cleanup"));
}

// Test 16-18: Code quality
TEST_F(UIControllerTest, JSFile_HasErrorHandling) {
    ASSERT_TRUE(validator_->fileExists());
    const std::string& content = validator_->getContent();
    EXPECT_TRUE(content.find("try") != std::string::npos &&
                content.find("catch") != std::string::npos);
}

TEST_F(UIControllerTest, JSFile_HasComments) {
    ASSERT_TRUE(validator_->fileExists());
    const std::string& content = validator_->getContent();
    EXPECT_TRUE(content.find("//") != std::string::npos ||
                content.find("/*") != std::string::npos);
}

TEST_F(UIControllerTest, JSFile_HasReasonableSize) {
    ASSERT_TRUE(validator_->fileExists());
    size_t line_count = std::count(validator_->getContent().begin(),
                                    validator_->getContent().end(), '\n') + 1;
    EXPECT_GT(line_count, 100);
    EXPECT_LT(line_count, 2000);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << "\n=== UI Controller Tests ===" << std::endl;
    std::cout << "Testing: webui/js/prettifier-ui.js" << std::endl << "\n";
    return RUN_ALL_TESTS();
}
