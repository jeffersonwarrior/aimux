// v2.2 Task 3: JavaScript API Client Tests
// Test suite for api-client.js validation through C++ (structure and content validation)
// Expected: 12 test cases covering JavaScript file structure, methods, error handling

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>

// Helper class for JavaScript validation
class JavaScriptValidator {
public:
    explicit JavaScriptValidator(const std::string& js_file_path) {
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

    bool fileExists() const {
        return file_exists_;
    }

    bool hasClass(const std::string& class_name) const {
        std::string pattern = "class\\s+" + class_name;
        std::regex class_regex(pattern);
        return std::regex_search(js_content_, class_regex);
    }

    bool hasMethod(const std::string& class_name, const std::string& method_name) const {
        // Look for method definition: async methodName() or methodName()
        std::vector<std::string> patterns = {
            class_name + ".*" + method_name + "\\s*\\(",
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

    bool hasErrorHandling() const {
        // Check for try-catch blocks
        return js_content_.find("try") != std::string::npos &&
               js_content_.find("catch") != std::string::npos;
    }

    bool hasTimeoutHandling() const {
        // Check for AbortController and timeout logic
        return js_content_.find("AbortController") != std::string::npos &&
               js_content_.find("setTimeout") != std::string::npos;
    }

    bool hasValidation() const {
        // Check for validation logic
        return js_content_.find("validate") != std::string::npos ||
               js_content_.find("Validate") != std::string::npos;
    }

    bool hasFetchCalls() const {
        return js_content_.find("fetch(") != std::string::npos;
    }

    bool hasJSONHandling() const {
        return js_content_.find("JSON.stringify") != std::string::npos ||
               js_content_.find("response.json()") != std::string::npos;
    }

    bool hasConstructor() const {
        return js_content_.find("constructor(") != std::string::npos;
    }

    bool hasComments() const {
        // Check for both single-line and multi-line comments
        return (js_content_.find("//") != std::string::npos ||
                js_content_.find("/*") != std::string::npos);
    }

    bool hasModuleExport() const {
        // Check for module.exports (for Node.js compatibility)
        return js_content_.find("module.exports") != std::string::npos;
    }

    size_t countLines() const {
        return std::count(js_content_.begin(), js_content_.end(), '\n') + 1;
    }

    const std::string& getContent() const {
        return js_content_;
    }

private:
    bool file_exists_;
    std::string js_content_;
};

// Test fixture
class JavaScriptAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        // Path to JS file (relative to build directory)
        js_file_path_ = "../webui/js/api-client.js";
        validator_ = std::make_unique<JavaScriptValidator>(js_file_path_);
    }

    std::string js_file_path_;
    std::unique_ptr<JavaScriptValidator> validator_;
};

// Test 1: JavaScript file exists and loads
TEST_F(JavaScriptAPITest, JSFile_ExistsAndLoads) {
    EXPECT_TRUE(validator_->fileExists())
        << "api-client.js file should exist at: " << js_file_path_;
}

// Test 2: Has PrettifierAPIClient class
TEST_F(JavaScriptAPITest, JSFile_HasAPIClientClass) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasClass("PrettifierAPIClient"))
        << "Should define PrettifierAPIClient class";
}

// Test 3: Has constructor method
TEST_F(JavaScriptAPITest, JSFile_HasConstructor) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasConstructor())
        << "Should have constructor method";
}

// Test 4: Has getStatus method
TEST_F(JavaScriptAPITest, JSFile_HasGetStatusMethod) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasMethod("PrettifierAPIClient", "getStatus"))
        << "Should have getStatus() method";
}

// Test 5: Has updateConfig method
TEST_F(JavaScriptAPITest, JSFile_HasUpdateConfigMethod) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasMethod("PrettifierAPIClient", "updateConfig"))
        << "Should have updateConfig() method";
}

// Test 6: Has error handling with try-catch
TEST_F(JavaScriptAPITest, JSFile_HasErrorHandling) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasErrorHandling())
        << "Should have try-catch error handling";
}

// Test 7: Has timeout handling with AbortController
TEST_F(JavaScriptAPITest, JSFile_HasTimeoutHandling) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasTimeoutHandling())
        << "Should have timeout handling with AbortController";
}

// Test 8: Has validation logic
TEST_F(JavaScriptAPITest, JSFile_HasValidation) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasValidation())
        << "Should have validation logic for config";
}

// Test 9: Uses fetch API for HTTP requests
TEST_F(JavaScriptAPITest, JSFile_UsesFetchAPI) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasFetchCalls())
        << "Should use fetch() for HTTP requests";
}

// Test 10: Has JSON handling
TEST_F(JavaScriptAPITest, JSFile_HasJSONHandling) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasJSONHandling())
        << "Should handle JSON parsing and stringification";
}

// Test 11: File has documentation comments
TEST_F(JavaScriptAPITest, JSFile_HasComments) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasComments())
        << "Should have documentation comments";
}

// Test 12: File is not empty and has reasonable size
TEST_F(JavaScriptAPITest, JSFile_HasReasonableSize) {
    ASSERT_TRUE(validator_->fileExists());

    size_t line_count = validator_->countLines();

    EXPECT_GT(line_count, 50)
        << "JS file should have substantial content (>50 lines)";
    EXPECT_LT(line_count, 1000)
        << "JS file should not be excessively large (<1000 lines)";
}

// Test 13: Has module export for compatibility
TEST_F(JavaScriptAPITest, JSFile_HasModuleExport) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasModuleExport())
        << "Should have module.exports for Node.js compatibility";
}

// Test 14: Has helper methods (setTimeout, setBaseUrl)
TEST_F(JavaScriptAPITest, JSFile_HasHelperMethods) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasMethod("PrettifierAPIClient", "setTimeout"))
        << "Should have setTimeout() helper method";
    EXPECT_TRUE(validator_->hasMethod("PrettifierAPIClient", "setBaseUrl"))
        << "Should have setBaseUrl() helper method";
}

// Test 15: File contains proper async/await patterns
TEST_F(JavaScriptAPITest, JSFile_UsesAsyncAwait) {
    ASSERT_TRUE(validator_->fileExists());

    const std::string& content = validator_->getContent();

    EXPECT_TRUE(content.find("async") != std::string::npos)
        << "Should use async functions";
    EXPECT_TRUE(content.find("await") != std::string::npos)
        << "Should use await for promises";
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\n=== JavaScript API Client Tests ===" << std::endl;
    std::cout << "Testing: webui/js/api-client.js" << std::endl;
    std::cout << "Expected: PrettifierAPIClient class with getStatus() and updateConfig() methods" << std::endl;
    std::cout << "\n";

    return RUN_ALL_TESTS();
}
