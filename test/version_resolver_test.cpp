#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <future>
#include <thread>

#include "aimux/distribution/version_resolver.hpp"

using namespace aimux::distribution;
using ::testing::Return;
using ::testing::AtLeast;
using ::testing::_;

class VersionResolverTest : public ::testing::Test {
protected:
    void SetUp() override {
        resolver = std::make_unique<VersionResolver>();
    }

    std::unique_ptr<VersionResolver> resolver;
};

// SemanticVersion tests
TEST_F(VersionResolverTest, SemanticVersionParsing) {
    SemanticVersion v1("1.2.3");
    EXPECT_EQ(v1.major, 1);
    EXPECT_EQ(v1.minor, 2);
    EXPECT_EQ(v1.patch, 3);
    EXPECT_TRUE(v1.is_valid());

    SemanticVersion v2("v2.0.0-alpha.1+build.123");
    EXPECT_EQ(v2.major, 2);
    EXPECT_EQ(v2.minor, 0);
    EXPECT_EQ(v2.patch, 0);
    EXPECT_EQ(v2.prerelease, "alpha.1");
    EXPECT_EQ(v2.build, "build.123");
    EXPECT_TRUE(v2.is_prerelease());
}

TEST_F(VersionResolverTest, SemanticVersionComparison) {
    SemanticVersion v1("1.2.3");
    SemanticVersion v2("1.2.4");
    SemanticVersion v3("2.0.0");
    SemanticVersion v4("1.2.3-alpha.1");

    EXPECT_TRUE(v1 < v2);
    EXPECT_TRUE(v2 < v3);
    EXPECT_TRUE(v4 < v1); // Prerelease < stable

    EXPECT_TRUE(v2 > v1);
    EXPECT_TRUE(v3 > v2);
    EXPECT_TRUE(v1 > v4);

    SemanticVersion v5("1.2.3");
    EXPECT_TRUE(v1 == v5);
    EXPECT_FALSE(v1 != v5);
}

TEST_F(VersionResolverTest, SemanticVersionCompatibility) {
    SemanticVersion required("1.2.3");

    SemanticVersion compatible1("1.2.4");
    SemanticVersion compatible2("1.5.0");
    SemanticVersion incompatible1("2.0.0");
    SemanticVersion incompatible2("1.1.0");

    EXPECT_TRUE(compatible1.is_compatible_with(required));
    EXPECT_TRUE(compatible2.is_compatible_with(required));
    EXPECT_FALSE(incompatible1.is_compatible_with(required));
    EXPECT_FALSE(incompatible2.is_compatible_with(required));

    // Zero major version - breaking changes always
    SemanticVersion zero_major_required("0.1.0");
    SemanticVersion zero_major_compatible("0.1.1");
    SemanticVersion zero_major_incompatible("0.2.0");

    EXPECT_TRUE(zero_major_compatible.is_compatible_with(zero_major_required));
    EXPECT_FALSE(zero_major_incompatible.is_compatible_with(zero_major_required));
}

TEST_F(VersionResolverTest, SemanticVersionStringConversion) {
    SemanticVersion v1(1, 2, 3);
    EXPECT_EQ(v1.to_string(), "1.2.3");

    SemanticVersion v2(2, 0, 0, "alpha.1", "build.123");
    EXPECT_EQ(v2.to_string(), "2.0.0-alpha.1+build.123");

    SemanticVersion v3(3, 1, 0, "beta");
    EXPECT_EQ(v3.to_string(), "3.1.0-beta");
}

TEST_F(VersionResolverTest, SemanticVersionValidation) {
    EXPECT_TRUE(SemanticVersion::is_valid_version_string("1.2.3"));
    EXPECT_TRUE(SemanticVersion::is_valid_version_string("v2.0.0"));
    EXPECT_TRUE(SemanticVersion::is_valid_version_string("1.0.0-alpha.1"));
    EXPECT_TRUE(SemanticVersion::is_valid_version_string("1.2.3+build.456"));

    EXPECT_FALSE(SemanticVersion::is_valid_version_string("1.2"));
    EXPECT_FALSE(SemanticVersion::is_valid_version_string("1.2.3.4"));
    EXPECT_FALSE(SemanticVersion::is_valid_version_string("invalid"));
    EXPECT_FALSE(SemanticVersion::is_valid_version_string(""));
}

// VersionConstraint tests
TEST_F(VersionResolverTest, VersionConstraintExact) {
    VersionConstraint constraint;
    constraint.op = VersionConstraint::Operator::EXACT;
    constraint.version = SemanticVersion::parse("1.2.3");

    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("1.2.3")));
    EXPECT_FALSE(constraint.accepts(SemanticVersion::parse("1.2.4")));
    EXPECT_FALSE(constraint.accepts(SemanticVersion::parse("1.3.0")));
}

TEST_F(VersionResolverTest, VersionConstraintGreaterThan) {
    VersionConstraint constraint;
    constraint.op = VersionConstraint::Operator::GREATER;
    constraint.version = SemanticVersion::parse("1.2.3");

    EXPECT_FALSE(constraint.accepts(SemanticVersion::parse("1.2.3")));
    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("1.2.4")));
    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("2.0.0")));
    EXPECT_FALSE(constraint.accepts(SemanticVersion::parse("1.2.2")));
}

TEST_F(VersionResolverTest, VersionConstraintCaret) {
    VersionConstraint constraint;
    constraint.op = VersionConstraint::Operator::CARET;
    constraint.version = SemanticVersion::parse("1.2.3");

    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("1.2.3")));
    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("1.2.4")));
    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("1.3.0")));
    EXPECT_FALSE(constraint.accepts(SemanticVersion::parse("2.0.0")));

    // Caret with zero major version
    constraint.version = SemanticVersion::parse("0.2.3");
    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("0.2.4")));
    EXPECT_FALSE(constraint.accepts(SemanticVersion::parse("0.3.0")));
}

TEST_F(VersionResolverTest, VersionConstraintTilde) {
    VersionConstraint constraint;
    constraint.op = VersionConstraint::Operator::TILDE;
    constraint.version = SemanticVersion::parse("1.2.3");

    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("1.2.3")));
    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("1.2.4")));
    EXPECT_FALSE(constraint.accepts(SemanticVersion::parse("1.3.0")));

    // Tilde with minor only
    constraint.version = SemanticVersion::parse("1.2.0");
    EXPECT_TRUE(constraint.accepts(SemanticVersion::parse("1.2.3")));
    EXPECT_FALSE(constraint.accepts(SemanticVersion::parse("1.3.0")));
}

TEST_F(VersionResolverTest, VersionConstraintFromString) {
    auto constraint1 = VersionConstraint::from_string("1.2.3");
    EXPECT_EQ(constraint1.op, VersionConstraint::Operator::EXACT);
    EXPECT_TRUE(constraint1.accepts(SemanticVersion::parse("1.2.3")));

    auto constraint2 = VersionConstraint::from_string(">=1.0.0");
    EXPECT_EQ(constraint2.op, VersionConstraint::Operator::GREATER_EQUAL);

    auto constraint3 = VersionConstraint::from_string("^2.1.0");
    EXPECT_EQ(constraint3.op, VersionConstraint::Operator::CARET);

    auto constraint4 = VersionConstraint::from_string("~1.5.2");
    EXPECT_EQ(constraint4.op, VersionConstraint::Operator::TILDE);

    auto constraint5 = VersionConstraint::from_string("1.2.*");
    EXPECT_EQ(constraint5.op, VersionConstraint::Operator::WILDCARD);
}

// VersionResolver tests
TEST_F(VersionResolverTest, ResolutionWithNoDependencies) {
    // Create plugins with no dependencies
    std::vector<PluginPackage> plugins;
    PluginPackage plugin1;
    plugin1.id = "test/plugin1";
    plugin1.version = "1.0.0";
    plugin1.dependencies = {};
    plugins.push_back(plugin1);

    auto result_future = resolver->resolve_dependencies(plugins);
    auto result = result_future.get();

    EXPECT_TRUE(result.resolution_success);
    EXPECT_EQ(result.resolved_plugins.size(), 1);
    EXPECT_EQ(result.resolved_plugins[0].plugin_id, "test/plugin1");
    EXPECT_EQ(result.resolved_plugins[0].selected_version.to_string(), "1.0.0");
}

TEST_F(VersionResolverTest, SatisfiesConstraint) {
    SemanticVersion version("2.1.0");

    // Exact constraint
    VersionConstraint exact = VersionConstraint::from_string("2.1.0");
    EXPECT_TRUE(resolver->satisfies_constraint(version, exact));

    // Greater constraint
    VersionConstraint greater = VersionConstraint::from_string(">2.0.0");
    EXPECT_TRUE(resolver->satisfies_constraint(version, greater));

    // Caret constraint
    VersionConstraint caret = VersionConstraint::from_string("^2.0.0");
    EXPECT_TRUE(resolver->satisfies_constraint(version, caret));

    // Tilde constraint
    VersionConstraint tilde = VersionConstraint::from_string("~2.1.0");
    EXPECT_TRUE(resolver->satisfies_constraint(version, tilde));

    // Failing constraint
    VersionConstraint failing = VersionConstraint::from_string("^3.0.0");
    EXPECT_FALSE(resolver->satisfies_constraint(version, failing));
}

TEST_F(VersionResolverTest, ResolutionStatistics) {
    auto stats = resolver->get_resolution_statistics();

    EXPECT_TRUE(stats.contains("total_resolutions"));
    EXPECT_TRUE(stats.contains("successful_resolutions"));
    EXPECT_TRUE(stats.contains("failed_resolutions"));
    EXPECT_TRUE(stats.contains("cache_hits"));
    EXPECT_TRUE(stats.contains("cache_size"));

    // Initial stats should be zero
    EXPECT_EQ(stats["total_resolutions"], 0);
    EXPECT_EQ(stats["successful_resolutions"], 0);
    EXPECT_EQ(stats["failed_resolutions"], 0);
    EXPECT_EQ(stats["cache_hits"], 0);
}

// PluginDependency tests
TEST_F(VersionResolverTest, PluginDependencySerialization) {
    PluginDependency dependency;
    dependency.plugin_id = "test/plugin";
    dependency.display_name = "Test Plugin";
    dependency.version_constraint = VersionConstraint::from_string("^1.0.0");
    dependency.optional = false;
    dependency.reason = "Core functionality";

    auto json = dependency.to_json();
    auto restored = PluginDependency::from_json(json);

    EXPECT_EQ(restored.plugin_id, dependency.plugin_id);
    EXPECT_EQ(restored.display_name, dependency.display_name);
    EXPECT_EQ(restored.optional, dependency.optional);
    EXPECT_EQ(restored.reason, dependency.reason);
}

TEST_F(VersionResolverTest, PluginDependencyCompatibility) {
    PluginDependency dependency;
    dependency.version_constraint = VersionConstraint::from_string("^1.2.0");

    EXPECT_TRUE(dependency.is_compatible_with(SemanticVersion::parse("1.2.0")));
    EXPECT_TRUE(dependency.is_compatible_with(SemanticVersion::parse("1.3.4")));
    EXPECT_FALSE(dependency.is_compatible_with(SemanticVersion::parse("2.0.0")));
    EXPECT_FALSE(dependency.is_compatible_with(SemanticVersion::parse("1.1.9")));
}

// DependencyConflict tests
TEST_F(VersionResolverTest, DependencyConflictCreation) {
    DependencyConflict conflict;
    conflict.type = DependencyConflict::Type::VERSION_CONFLICT;
    conflict.dependency_id = "shared-lib";
    conflict.description = "Incompatible version requirements";

    std::vector<std::string> plugins = {"plugin-a", "plugin-b"};
    conflict.conflicting_plugins = plugins;

    std::vector<SemanticVersion> versions = {
        SemanticVersion::parse("1.0.0"),
        SemanticVersion::parse("2.0.0")
    };
    conflict.conflicting_versions = versions;

    EXPECT_EQ(conflict.type, DependencyConflict::Type::VERSION_CONFLICT);
    EXPECT_EQ(conflict.dependency_id, "shared-lib");
    EXPECT_EQ(conflict.conflicting_plugins, plugins);
    EXPECT_EQ(conflict.conflicting_versions, versions);
}

TEST_F(VersionResolverTest, DependencyConflictStringRepresentation) {
    DependencyConflict version_conflict;
    version_conflict.type = DependencyConflict::Type::VERSION_CONFLICT;
    version_conflict.dependency_id = "core-lib";
    version_conflict.conflicting_versions = {
        SemanticVersion::parse("1.0.0"),
        SemanticVersion::parse("2.0.0")
    };

    std::string description = version_conflict.to_string();
    EXPECT_TRUE(description.contains("Version conflict"));
    EXPECT_TRUE(description.contains("core-lib"));
    EXPECT_TRUE(description.contains("1.0.0"));
    EXPECT_TRUE(description.contains("2.0.0"));

    DependencyConflict circular_conflict;
    circular_conflict.type = DependencyConflict::Type::CIRCULAR_DEPENDENCY;
    circular_conflict.conflicting_plugins = {"plugin-a", "plugin-b", "plugin-a"};

    description = circular_conflict.to_string();
    EXPECT_TRUE(description.contains("Circular dependency"));
    EXPECT_TRUE(description.contains("plugin-a"));
    EXPECT_TRUE(description.contains("plugin-b"));
}

// DependencyNode tests
TEST_F(VersionResolverTest, DependencyNodeSerialization) {
    DependencyNode node;
    node.plugin_id = "test/node";
    node.selected_version = SemanticVersion::parse("1.2.3");
    node.depth = 2;
    node.is_optional = true;
    node.dependencies = {"dep1", "dep2"};

    auto json = node.to_json();
    auto restored = DependencyNode::from_json(json);

    EXPECT_EQ(restored.plugin_id, node.plugin_id);
    EXPECT_EQ(restored.selected_version, node.selected_version);
    EXPECT_EQ(restored.depth, node.depth);
    EXPECT_EQ(restored.is_optional, node.is_optional);
    EXPECT_EQ(restored.dependencies, node.dependencies);
}

// ResolutionResult tests
TEST_F(VersionResolverTest, ResolutionResultCreation) {
    std::vector<DependencyNode> nodes;
    DependencyNode node1;
    node1.plugin_id = "plugin1";
    node1.selected_version = SemanticVersion::parse("1.0.0");
    nodes.push_back(node1);

    auto success_result = ResolutionResult::success(nodes);
    EXPECT_TRUE(success_result.resolution_success);
    EXPECT_EQ(success_result.resolved_plugins, nodes);
    EXPECT_EQ(success_result.dependencies_resolved, 1);

    std::vector<DependencyConflict> conflicts;
    DependencyConflict conflict;
    conflict.type = DependencyConflict::Type::VERSION_CONFLICT;
    conflicts.push_back(conflict);

    auto failure_result = ResolutionResult::failure(conflicts);
    EXPECT_FALSE(failure_result.resolution_success);
    EXPECT_EQ(failure_result.conflicts, conflicts);
}