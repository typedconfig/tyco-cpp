#include <gtest/gtest.h>
#include "tyco/parser.h"

class TycoParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<tyco::TycoParser>();
    }
    
    void TearDown() override {
        parser.reset();
    }
    
    std::unique_ptr<tyco::TycoParser> parser;
};

// Test TycoValue basic functionality
TEST(TycoValueTest, BasicTypes) {
    // Test null
    tyco::TycoValue null_val;
    EXPECT_TRUE(null_val.is_null());
    EXPECT_FALSE(null_val.is_string());
    
    // Test string
    tyco::TycoValue str_val(std::string("hello"));
    EXPECT_TRUE(str_val.is_string());
    EXPECT_EQ(str_val.as_string(), "hello");
    EXPECT_EQ(str_val.get_string(), "hello");
    EXPECT_EQ(str_val.get_string("default"), "hello");
    
    // Test integer
    tyco::TycoValue int_val(int64_t(42));
    EXPECT_TRUE(int_val.is_int());
    EXPECT_EQ(int_val.as_int(), 42);
    EXPECT_EQ(int_val.get_int(), 42);
    EXPECT_EQ(int_val.get_int(0), 42);
    
    // Test boolean
    tyco::TycoValue bool_val(true);
    EXPECT_TRUE(bool_val.is_bool());
    EXPECT_EQ(bool_val.as_bool(), true);
    EXPECT_EQ(bool_val.get_bool(), true);
    
    // Test float
    tyco::TycoValue float_val(3.14);
    EXPECT_TRUE(float_val.is_float());
    EXPECT_DOUBLE_EQ(float_val.as_float(), 3.14);
    EXPECT_DOUBLE_EQ(float_val.get_float(), 3.14);
}

TEST(TycoValueTest, SafeAccess) {
    tyco::TycoValue str_val(std::string("hello"));
    
    // Safe access with wrong type should return default
    EXPECT_EQ(str_val.get_int(999), 999);
    EXPECT_EQ(str_val.get_bool(true), true);
    EXPECT_DOUBLE_EQ(str_val.get_float(1.5), 1.5);
}

// Test TycoContext functionality
TEST(TycoContextTest, GlobalVariables) {
    tyco::TycoContext context;
    
    // Initially empty
    EXPECT_TRUE(context.empty());
    EXPECT_TRUE(context.get_global_names().empty());
    
    // Set some globals
    context.set_global("name", tyco::TycoValue(std::string("test")));
    context.set_global("count", tyco::TycoValue(int64_t(10)));
    
    EXPECT_FALSE(context.empty());
    EXPECT_EQ(context.get_global_names().size(), 2);
    
    // Access globals
    EXPECT_EQ(context.get_global("name").as_string(), "test");
    EXPECT_EQ(context.get_global("count").as_int(), 10);
    
    // Access via operator[]
    EXPECT_EQ(context["name"].as_string(), "test");
    EXPECT_EQ(context["count"].as_int(), 10);
}

TEST(TycoContextTest, Objects) {
    tyco::TycoContext context;
    
    // Create object
    tyco::TycoValue obj(std::unordered_map<std::string, tyco::TycoValue>{
        {"name", tyco::TycoValue(std::string("server1"))},
        {"port", tyco::TycoValue(int64_t(8080))}
    });
    
    context.add_object("Server", obj);
    
    // Check object types
    auto types = context.get_object_types();
    EXPECT_EQ(types.size(), 1);
    EXPECT_EQ(types[0], "Server");
    
    // Check objects
    const auto& servers = context.get_objects("Server");
    EXPECT_EQ(servers.size(), 1);
    EXPECT_EQ(servers[0]["name"].as_string(), "server1");
    EXPECT_EQ(servers[0]["port"].as_int(), 8080);
}

// Test basic parsing
TEST_F(TycoParserTest, BasicParsing) {
    std::string config = R"(str environment: production
int port: 8080
bool debug: true)";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_FALSE(parser->has_errors());
    EXPECT_FALSE(context.empty());
    
    // Check that we parsed the global variables
    EXPECT_EQ(context.get_global("environment").get_string(), "production");
    EXPECT_EQ(context.get_global("port").get_int(), 8080);
    EXPECT_EQ(context.get_global("debug").get_bool(), true);
}

TEST_F(TycoParserTest, EmptyContent) {
    tyco::TycoContext context = parser->parse_string("");
    
    EXPECT_FALSE(parser->has_errors());
    EXPECT_TRUE(context.empty());
}

// Test comprehensive parsing
TEST_F(TycoParserTest, StructParsing) {
    std::string config = R"(str environment: production

Server:
 *str name:
  int port:
  str host:
  - web1, 80, web1.example.com
  - api1, 3000, api1.example.com)";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_FALSE(parser->has_errors());
    
    // Check global
    EXPECT_EQ(context.get_global("environment").get_string(), "production");
    
    // Check struct objects
    const auto& servers = context.get_objects("Server");
    EXPECT_EQ(servers.size(), 2);
    
    // Check first server
    EXPECT_EQ(servers[0]["name"].get_string(), "web1");
    EXPECT_EQ(servers[0]["port"].get_int(), 80);
    EXPECT_EQ(servers[0]["host"].get_string(), "web1.example.com");
    
    // Check second server
    EXPECT_EQ(servers[1]["name"].get_string(), "api1");
    EXPECT_EQ(servers[1]["port"].get_int(), 3000);
    EXPECT_EQ(servers[1]["host"].get_string(), "api1.example.com");
}

TEST_F(TycoParserTest, ArrayParsing) {
    std::string config = R"(str[] environments: ["dev", "staging", "prod"]
int[] ports: [8080, 8081, 8082])";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_FALSE(parser->has_errors());
    
    // Check string array
    const auto& envs = context.get_global("environments").as_array();
    EXPECT_EQ(envs.size(), 3);
    EXPECT_EQ(envs[0].get_string(), "dev");
    EXPECT_EQ(envs[1].get_string(), "staging");
    EXPECT_EQ(envs[2].get_string(), "prod");
    
    // Check int array
    const auto& ports = context.get_global("ports").as_array();
    EXPECT_EQ(ports.size(), 3);
    EXPECT_EQ(ports[0].get_int(), 8080);
    EXPECT_EQ(ports[1].get_int(), 8081);
    EXPECT_EQ(ports[2].get_int(), 8082);
}

TEST_F(TycoParserTest, ErrorHandling) {
    std::string config = R"(invalid line format here
str valid_var: test)";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_TRUE(parser->has_errors());
    EXPECT_FALSE(parser->get_errors().empty());
    
    // Should still parse the valid line
    EXPECT_EQ(context.get_global("valid_var").get_string(), "test");
}

TEST_F(TycoParserTest, TemplateExpansion) {
    std::string config = R"(str app_name: myapp
str environment: prod
str domain: example.com
str full_domain: "{app_name}.{environment}.{domain}")";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_FALSE(parser->has_errors());
    EXPECT_EQ(context.get_global("full_domain").get_string(), "myapp.prod.example.com");
}

TEST_F(TycoParserTest, StructTemplateExpansion) {
    std::string config = R"(str domain: example.com

Server:
 *str name:
  str hostname:
  str url:
  - web1, {name}.{domain}, "https://{hostname}/health")";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_FALSE(parser->has_errors());
    
    const auto& servers = context.get_objects("Server");
    EXPECT_EQ(servers.size(), 1);
    
    EXPECT_EQ(servers[0]["name"].get_string(), "web1");
    EXPECT_EQ(servers[0]["hostname"].get_string(), "web1.example.com");
    EXPECT_EQ(servers[0]["url"].get_string(), "https://web1.example.com/health");
}

TEST_F(TycoParserTest, NestedTemplateExpansion) {
    std::string config = R"(str base_url: api.example.com
str full_url: "https://{base_url}"
str health_check: "{full_url}/health")";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_FALSE(parser->has_errors());
    EXPECT_EQ(context.get_global("health_check").get_string(), "https://api.example.com/health");
}

TEST_F(TycoParserTest, ExplicitGlobalAccess) {
    std::string config = R"(str app_name: GlobalApp
str environment: production

Server:
 *str name:
  str environment:
  str description:
  - web1, staging, "Server {name} for {global.app_name} in {global.environment}")";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_FALSE(parser->has_errors());
    
    const auto& servers = context.get_objects("Server");
    EXPECT_EQ(servers.size(), 1);
    
    // Should use local values for {name} and global values for {global.app_name} and {global.environment}
    EXPECT_EQ(servers[0]["description"].get_string(), "Server web1 for GlobalApp in production");
}

TEST_F(TycoParserTest, ScopeResolutionPriority) {
    std::string config = R"(str name: GlobalName
str value: GlobalValue

Test:
 *str name:
  str value:
  str local_ref:
  str global_ref:
  - LocalName, LocalValue, "{name}-{value}", "{global.name}-{global.value}")";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_FALSE(parser->has_errors());
    
    const auto& tests = context.get_objects("Test");
    EXPECT_EQ(tests.size(), 1);
    
    // Local scope should take priority for {name} and {value}
    EXPECT_EQ(tests[0]["local_ref"].get_string(), "LocalName-LocalValue");
    
    // Explicit global should access global scope
    EXPECT_EQ(tests[0]["global_ref"].get_string(), "GlobalName-GlobalValue");
}

TEST_F(TycoParserTest, UnresolvedVariables) {
    std::string config = R"(str known: value

Test:
 *str field:
  - "{known} {unknown} {global.missing} {..parent}")";
    
    tyco::TycoContext context = parser->parse_string(config);
    
    EXPECT_FALSE(parser->has_errors());
    
    const auto& tests = context.get_objects("Test");
    EXPECT_EQ(tests.size(), 1);
    
    // Should resolve known variables and leave unresolved ones as-is
    EXPECT_EQ(tests[0]["field"].get_string(), "value {unknown} {global.missing} {..parent}");
}

// Test convenience functions
TEST(TycoConvenienceTest, LoadsFunction) {
    std::string config = "str test: hello";
    
    tyco::TycoContext context = tyco::loads(config);
    
    EXPECT_EQ(context.get_global("test").get_string(), "hello");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}