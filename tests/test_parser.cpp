#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "tyco/parser.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

struct TycoTestCase {
    std::string name;
    fs::path input_path;
    fs::path expected_path;
};

fs::path SharedSuiteRoot() {
#ifdef TYCO_TEST_SUITE_DIR
    static const fs::path root = fs::path(TYCO_TEST_SUITE_DIR);
#else
    static const fs::path root = fs::path(__FILE__).parent_path() / "shared";
#endif
    return root;
}

std::vector<TycoTestCase> DiscoverTestCases() {
    std::vector<TycoTestCase> cases;
    const fs::path inputs_dir = SharedSuiteRoot() / "inputs";
    const fs::path expected_dir = SharedSuiteRoot() / "expected";
    
    for (const auto& entry : fs::directory_iterator(inputs_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".tyco") {
            continue;
        }
        
        const std::string name = entry.path().stem().string();
        const fs::path expected_file = expected_dir / (name + ".json");
        
        if (!fs::exists(expected_file)) {
            continue;
        }
        
        cases.push_back(TycoTestCase{
            name,
            entry.path(),
            expected_file
        });
    }
    
    std::sort(cases.begin(), cases.end(), [](const TycoTestCase& a, const TycoTestCase& b) {
        return a.name < b.name;
    });
    
    return cases;
}

const std::vector<TycoTestCase>& AllTestCases() {
    static const std::vector<TycoTestCase> cases = DiscoverTestCases();
    return cases;
}

} // namespace

class TycoParserGoldenTest : public ::testing::TestWithParam<TycoTestCase> {};

TEST_P(TycoParserGoldenTest, ParsesCanonicalFiles) {
    const auto test_case = GetParam();
    
    tyco::TycoLexer lexer;
    auto context = lexer.parse_file(test_case.input_path.string());
    ASSERT_NE(context, nullptr) << "Failed to parse " << test_case.input_path;
    
    context->render();
    const std::string actual_json_str = context->dumps_json(2);
    
    json actual_json;
    ASSERT_NO_THROW(actual_json = json::parse(actual_json_str))
        << "Parser produced invalid JSON for " << test_case.name;
    
    std::ifstream expected_file(test_case.expected_path);
    ASSERT_TRUE(expected_file.good()) << "Missing expected file " << test_case.expected_path;
    json expected_json;
    expected_file >> expected_json;
    
    EXPECT_EQ(expected_json.dump(2), actual_json.dump(2))
        << "Mismatch for canonical test '" << test_case.name << "'";
}

INSTANTIATE_TEST_SUITE_P(
    CanonicalSuite,
    TycoParserGoldenTest,
    ::testing::ValuesIn(AllTestCases()),
    [](const ::testing::TestParamInfo<TycoTestCase>& info) {
        return info.param.name;
    });
