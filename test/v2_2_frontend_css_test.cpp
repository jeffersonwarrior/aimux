// v2.2 Task 2: Frontend CSS Validation Tests
// Test suite for validating prettifier.css structure and content
// Expected: 10 test cases covering CSS file existence, classes, syntax, responsive breakpoints

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <algorithm>

// Helper class for CSS validation
class CSSValidator {
public:
    explicit CSSValidator(const std::string& css_file_path) {
        std::ifstream file(css_file_path);
        if (!file.is_open()) {
            file_exists_ = false;
            return;
        }

        file_exists_ = true;
        std::stringstream buffer;
        buffer << file.rdbuf();
        css_content_ = buffer.str();
        file.close();

        // Parse CSS classes
        parse_css_classes();
    }

    bool fileExists() const {
        return file_exists_;
    }

    bool hasClass(const std::string& class_name) const {
        return css_classes_.find(class_name) != css_classes_.end();
    }

    bool hasMediaQuery(const std::string& query) const {
        return css_content_.find(query) != std::string::npos;
    }

    bool hasProperty(const std::string& class_name, const std::string& property) const {
        // Find class definition
        std::string search_pattern = "\\." + class_name + "\\s*\\{";
        std::regex class_regex(search_pattern);
        std::smatch match;

        if (std::regex_search(css_content_, match, class_regex)) {
            size_t start_pos = match.position() + match.length();
            size_t end_pos = css_content_.find("}", start_pos);

            if (end_pos != std::string::npos) {
                std::string class_content = css_content_.substr(start_pos, end_pos - start_pos);
                return class_content.find(property) != std::string::npos;
            }
        }

        return false;
    }

    bool hasAnimation(const std::string& animation_name) const {
        std::string search = "@keyframes " + animation_name;
        return css_content_.find(search) != std::string::npos;
    }

    bool hasColorContrast() const {
        // Check for both light and dark colors (simplified check)
        bool has_light = css_content_.find("#fff") != std::string::npos ||
                        css_content_.find("white") != std::string::npos;
        bool has_dark = css_content_.find("#000") != std::string::npos ||
                       css_content_.find("black") != std::string::npos ||
                       css_content_.find("rgba(0, 0, 0") != std::string::npos;
        return has_light && has_dark;
    }

    size_t countClasses() const {
        return css_classes_.size();
    }

    const std::string& getContent() const {
        return css_content_;
    }

private:
    void parse_css_classes() {
        // Simple regex to find CSS class names
        std::regex class_regex("\\.([-\\w]+)\\s*(?:\\{|,)");
        std::smatch match;
        std::string::const_iterator searchStart(css_content_.cbegin());

        while (std::regex_search(searchStart, css_content_.cend(), match, class_regex)) {
            css_classes_.insert(match[1].str());
            searchStart = match.suffix().first;
        }
    }

    bool file_exists_;
    std::string css_content_;
    std::set<std::string> css_classes_;
};

// Test fixture
class FrontendCSSTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Path to CSS file (relative to build directory)
        css_file_path_ = "../webui/css/prettifier.css";
        validator_ = std::make_unique<CSSValidator>(css_file_path_);
    }

    std::string css_file_path_;
    std::unique_ptr<CSSValidator> validator_;
};

// Test 1: CSS file exists and loads
TEST_F(FrontendCSSTest, CSSFile_ExistsAndLoads) {
    EXPECT_TRUE(validator_->fileExists())
        << "prettifier.css file should exist at: " << css_file_path_;
}

// Test 2: Has required CSS classes for card container
TEST_F(FrontendCSSTest, CSSFile_HasRequiredCardClasses) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasClass("prettifier-status"))
        << "Should have .prettifier-status class";
    EXPECT_TRUE(validator_->hasClass("card-header"))
        << "Should have .card-header class";
}

// Test 3: Has status badge class
TEST_F(FrontendCSSTest, CSSFile_HasStatusBadgeClass) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasClass("status-badge"))
        << "Should have .status-badge class";
    EXPECT_TRUE(validator_->hasClass("disabled"))
        << "Should have .disabled class for status badge";
}

// Test 4: Has metrics section classes
TEST_F(FrontendCSSTest, CSSFile_HasMetricsClasses) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasClass("status-section"))
        << "Should have .status-section class";
    EXPECT_TRUE(validator_->hasClass("metric"))
        << "Should have .metric class";
}

// Test 5: Has format preferences classes
TEST_F(FrontendCSSTest, CSSFile_HasFormatClasses) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasClass("format-section"))
        << "Should have .format-section class";
    EXPECT_TRUE(validator_->hasClass("provider-formats"))
        << "Should have .provider-formats class";
    EXPECT_TRUE(validator_->hasClass("provider"))
        << "Should have .provider class";
}

// Test 6: Has configuration section classes
TEST_F(FrontendCSSTest, CSSFile_HasConfigClasses) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasClass("config-section"))
        << "Should have .config-section class";
    EXPECT_TRUE(validator_->hasClass("config-group"))
        << "Should have .config-group class";
}

// Test 7: Has action button classes
TEST_F(FrontendCSSTest, CSSFile_HasButtonClasses) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasClass("action-buttons"))
        << "Should have .action-buttons class";
    EXPECT_TRUE(validator_->hasClass("btn"))
        << "Should have .btn class";
    EXPECT_TRUE(validator_->hasClass("btn-primary"))
        << "Should have .btn-primary class";
    EXPECT_TRUE(validator_->hasClass("btn-secondary"))
        << "Should have .btn-secondary class";
    EXPECT_TRUE(validator_->hasClass("btn-tertiary"))
        << "Should have .btn-tertiary class";
}

// Test 8: Has message box classes
TEST_F(FrontendCSSTest, CSSFile_HasMessageClasses) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasClass("message-box"))
        << "Should have .message-box class";
    EXPECT_TRUE(validator_->hasClass("success"))
        << "Should have .success class for messages";
    EXPECT_TRUE(validator_->hasClass("error"))
        << "Should have .error class for messages";
}

// Test 9: Has responsive breakpoints
TEST_F(FrontendCSSTest, CSSFile_HasResponsiveBreakpoints) {
    ASSERT_TRUE(validator_->fileExists());

    // Check for tablet breakpoint
    EXPECT_TRUE(validator_->hasMediaQuery("@media (max-width: 768px)"))
        << "Should have tablet breakpoint at 768px";

    // Check for mobile breakpoint
    EXPECT_TRUE(validator_->hasMediaQuery("@media (max-width: 480px)"))
        << "Should have mobile breakpoint at 480px";
}

// Test 10: Has animations and transitions
TEST_F(FrontendCSSTest, CSSFile_HasAnimations) {
    ASSERT_TRUE(validator_->fileExists());

    // Check for slideIn animation
    EXPECT_TRUE(validator_->hasAnimation("slideIn"))
        << "Should have slideIn animation for message box";

    // Check for pulse animation
    EXPECT_TRUE(validator_->hasAnimation("pulse"))
        << "Should have pulse animation for status badge";

    // Check that classes use transitions
    const std::string& content = validator_->getContent();
    EXPECT_TRUE(content.find("transition:") != std::string::npos ||
                content.find("transition ") != std::string::npos)
        << "Should use CSS transitions for smooth effects";
}

// Test 11: Has accessibility features
TEST_F(FrontendCSSTest, CSSFile_HasAccessibilityFeatures) {
    ASSERT_TRUE(validator_->fileExists());

    const std::string& content = validator_->getContent();

    // Check for focus styles
    EXPECT_TRUE(content.find("focus") != std::string::npos)
        << "Should have focus styles for accessibility";

    // Check for prefers-reduced-motion
    EXPECT_TRUE(validator_->hasMediaQuery("@media (prefers-reduced-motion"))
        << "Should respect prefers-reduced-motion for accessibility";
}

// Test 12: Has proper color contrast
TEST_F(FrontendCSSTest, CSSFile_HasColorContrast) {
    ASSERT_TRUE(validator_->fileExists());

    EXPECT_TRUE(validator_->hasColorContrast())
        << "Should have both light and dark colors for proper contrast";
}

// Test 13: File is not empty and has reasonable size
TEST_F(FrontendCSSTest, CSSFile_HasReasonableSize) {
    ASSERT_TRUE(validator_->fileExists());

    const std::string& content = validator_->getContent();

    EXPECT_GT(content.length(), 1000)
        << "CSS file should have substantial content (>1000 chars)";
    EXPECT_LT(content.length(), 100000)
        << "CSS file should not be excessively large (<100KB)";
}

// Test 14: Contains gradient backgrounds
TEST_F(FrontendCSSTest, CSSFile_HasGradientBackgrounds) {
    ASSERT_TRUE(validator_->fileExists());

    const std::string& content = validator_->getContent();

    EXPECT_TRUE(content.find("linear-gradient") != std::string::npos)
        << "Should use linear gradients for visual appeal";
}

// Test 15: Has grid layout classes
TEST_F(FrontendCSSTest, CSSFile_HasGridLayout) {
    ASSERT_TRUE(validator_->fileExists());

    const std::string& content = validator_->getContent();

    EXPECT_TRUE(content.find("display: grid") != std::string::npos ||
                content.find("display:grid") != std::string::npos)
        << "Should use CSS Grid for responsive layout";

    EXPECT_TRUE(content.find("grid-template-columns") != std::string::npos)
        << "Should define grid template columns";
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "\n=== Frontend CSS Validation Tests ===" << std::endl;
    std::cout << "Testing: webui/css/prettifier.css" << std::endl;
    std::cout << "Expected: Professional responsive CSS with accessibility features" << std::endl;
    std::cout << "\n";

    return RUN_ALL_TESTS();
}
