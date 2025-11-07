#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <memory>

namespace tyco {

// Forward declarations
class TycoValue;
class TycoContext;

// Type definitions for dynamic data storage (similar to XML parsing approach)
using TycoVariant = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<TycoValue>,
    std::unordered_map<std::string, TycoValue>
>;

/**
 * Represents a single value in the Tyco configuration
 * Uses variant to handle different data types dynamically
 */
class TycoValue {
public:
    TycoValue();
    TycoValue(const TycoVariant& value);
    TycoValue(const TycoValue& other);
    TycoValue& operator=(const TycoValue& other);
    
    // Type checking
    bool is_null() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_float() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;
    
    // Value access (throws if wrong type)
    bool as_bool() const;
    int64_t as_int() const;
    double as_float() const;
    const std::string& as_string() const;
    const std::vector<TycoValue>& as_array() const;
    const std::unordered_map<std::string, TycoValue>& as_object() const;
    
    // Safe value access (returns default if wrong type)
    bool get_bool(bool default_value = false) const;
    int64_t get_int(int64_t default_value = 0) const;
    double get_float(double default_value = 0.0) const;
    std::string get_string(const std::string& default_value = "") const;
    
    // Object/Array access operators
    TycoValue& operator[](const std::string& key);
    const TycoValue& operator[](const std::string& key) const;
    TycoValue& operator[](size_t index);
    const TycoValue& operator[](size_t index) const;
    
    // Utility
    size_t size() const;
    bool empty() const;
    std::string to_json() const;
    
private:
    TycoVariant value_;
    
    // Helper for accessing missing keys/indices
    static TycoValue null_value_;
};

/**
 * Main parser class for Tyco configuration files
 * Uses hash map approach similar to XML parsing libraries
 */
class TycoParser {
public:
    TycoParser();
    ~TycoParser();
    
    // Parse from string or file
    TycoContext parse_string(const std::string& content);
    TycoContext parse_file(const std::string& filename);
    
    // Error handling
    bool has_errors() const;
    std::vector<std::string> get_errors() const;
    void clear_errors();
    
private:
    std::vector<std::string> errors_;
    
    // Parsing state
    struct ParseState {
        std::vector<std::string> lines;
        size_t current_line;
        std::string current_struct_name;
        std::vector<std::string> current_struct_fields;
        std::vector<std::string> current_struct_types;
        bool in_struct_definition;
        bool in_struct_instances;
        
        // Parent scope tracking for {..variable} support
        std::unordered_map<std::string, TycoValue> parent_scope;
        
        ParseState() : current_line(0), in_struct_definition(false), in_struct_instances(false) {}
    };
    
    // Internal parsing methods
    void parse_content(const std::string& content, TycoContext& context);
    void preprocess_lines(const std::string& content, ParseState& state);
    void parse_all_lines(ParseState& state, TycoContext& context);
    
    // Line parsing methods
    bool parse_global_variable(const std::string& line, TycoContext& context);
    bool parse_struct_definition(const std::string& line, ParseState& state);
    bool parse_struct_field(const std::string& line, ParseState& state);
    bool parse_struct_instances(const std::string& line, ParseState& state, TycoContext& context);
    
    // Value parsing
    TycoValue parse_value(const std::string& value_str, const std::string& type_name = "");
    std::vector<std::string> parse_array_values(const std::string& array_str);
    std::string trim(const std::string& str);
    
    // Template expansion
    std::string expand_templates(const std::string& input, const TycoContext& context, 
                                const std::unordered_map<std::string, TycoValue>* local_scope = nullptr,
                                const std::unordered_map<std::string, TycoValue>* parent_scope = nullptr);
    std::string resolve_variable(const std::string& var_name, const TycoContext& context,
                                const std::unordered_map<std::string, TycoValue>* local_scope = nullptr,
                                const std::unordered_map<std::string, TycoValue>* parent_scope = nullptr);
    std::string value_to_string(const TycoValue& value);
    std::vector<std::string> extract_template_variables(const std::string& input);
    bool has_templates(const std::string& input);
    
    // Helper methods
    bool is_struct_definition_line(const std::string& line);
    bool is_struct_field_line(const std::string& line);
    bool is_struct_instance_line(const std::string& line);
    bool is_global_variable_line(const std::string& line);
    void add_error(const std::string& message, size_t line_number);
};

/**
 * Represents the parsed Tyco context with globals and objects
 */
class TycoContext {
public:
    TycoContext();
    
    // Global variable access (hash map based)
    const TycoValue& get_global(const std::string& name) const;
    TycoValue& get_global(const std::string& name);
    void set_global(const std::string& name, const TycoValue& value);
    
    // Object access
    const std::vector<TycoValue>& get_objects(const std::string& type_name) const;
    std::vector<TycoValue>& get_objects(const std::string& type_name);
    void add_object(const std::string& type_name, const TycoValue& object);
    
    // Convenience accessors (like XML parsers)
    TycoValue& operator[](const std::string& key);
    const TycoValue& operator[](const std::string& key) const;
    
    // Utility
    std::vector<std::string> get_global_names() const;
    std::vector<std::string> get_object_types() const;
    std::string to_json() const;
    bool empty() const;
    
private:
    // Hash maps for dynamic data storage
    std::unordered_map<std::string, TycoValue> globals_;
    std::unordered_map<std::string, std::vector<TycoValue>> objects_;
    
    // Helper for missing values
    static std::vector<TycoValue> empty_vector_;
    static TycoValue null_value_;
};

// Convenience functions
TycoContext load(const std::string& filename);
TycoContext loads(const std::string& content);

} // namespace tyco