#include "tyco/parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <algorithm>

namespace tyco {

// Static members
TycoValue TycoValue::null_value_;
std::vector<TycoValue> TycoContext::empty_vector_;
TycoValue TycoContext::null_value_;

// TycoValue implementation
TycoValue::TycoValue() : value_(nullptr) {}

TycoValue::TycoValue(const TycoVariant& value) : value_(value) {}

TycoValue::TycoValue(const TycoValue& other) : value_(other.value_) {}

TycoValue& TycoValue::operator=(const TycoValue& other) {
    if (this != &other) {
        value_ = other.value_;
    }
    return *this;
}

bool TycoValue::is_null() const {
    return std::holds_alternative<std::nullptr_t>(value_);
}

bool TycoValue::is_bool() const {
    return std::holds_alternative<bool>(value_);
}

bool TycoValue::is_int() const {
    return std::holds_alternative<int64_t>(value_);
}

bool TycoValue::is_float() const {
    return std::holds_alternative<double>(value_);
}

bool TycoValue::is_string() const {
    return std::holds_alternative<std::string>(value_);
}

bool TycoValue::is_array() const {
    return std::holds_alternative<std::vector<TycoValue>>(value_);
}

bool TycoValue::is_object() const {
    return std::holds_alternative<std::unordered_map<std::string, TycoValue>>(value_);
}

bool TycoValue::as_bool() const {
    return std::get<bool>(value_);
}

int64_t TycoValue::as_int() const {
    return std::get<int64_t>(value_);
}

double TycoValue::as_float() const {
    return std::get<double>(value_);
}

const std::string& TycoValue::as_string() const {
    return std::get<std::string>(value_);
}

const std::vector<TycoValue>& TycoValue::as_array() const {
    return std::get<std::vector<TycoValue>>(value_);
}

const std::unordered_map<std::string, TycoValue>& TycoValue::as_object() const {
    return std::get<std::unordered_map<std::string, TycoValue>>(value_);
}

// Safe getters
bool TycoValue::get_bool(bool default_value) const {
    return is_bool() ? as_bool() : default_value;
}

int64_t TycoValue::get_int(int64_t default_value) const {
    return is_int() ? as_int() : default_value;
}

double TycoValue::get_float(double default_value) const {
    return is_float() ? as_float() : default_value;
}

std::string TycoValue::get_string(const std::string& default_value) const {
    return is_string() ? as_string() : default_value;
}

// Array/Object operators
TycoValue& TycoValue::operator[](const std::string& key) {
    if (!is_object()) {
        value_ = std::unordered_map<std::string, TycoValue>();
    }
    return std::get<std::unordered_map<std::string, TycoValue>>(value_)[key];
}

const TycoValue& TycoValue::operator[](const std::string& key) const {
    if (is_object()) {
        const auto& obj = as_object();
        auto it = obj.find(key);
        if (it != obj.end()) {
            return it->second;
        }
    }
    return null_value_;
}

TycoValue& TycoValue::operator[](size_t index) {
    if (!is_array()) {
        value_ = std::vector<TycoValue>();
    }
    auto& arr = std::get<std::vector<TycoValue>>(value_);
    if (index >= arr.size()) {
        arr.resize(index + 1);
    }
    return arr[index];
}

const TycoValue& TycoValue::operator[](size_t index) const {
    if (is_array() && index < as_array().size()) {
        return as_array()[index];
    }
    return null_value_;
}

size_t TycoValue::size() const {
    if (is_array()) {
        return as_array().size();
    } else if (is_object()) {
        return as_object().size();
    }
    return 0;
}

bool TycoValue::empty() const {
    return size() == 0;
}

std::string TycoValue::to_json() const {
    // TODO: Implement JSON serialization
    return "{}";
}

// TycoParser implementation
TycoParser::TycoParser() {}

TycoParser::~TycoParser() {}

TycoContext TycoParser::parse_string(const std::string& content) {
    TycoContext context;
    clear_errors();
    parse_content(content, context);
    return context;
}

TycoContext TycoParser::parse_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        errors_.push_back("Could not open file: " + filename);
        return TycoContext();
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_string(buffer.str());
}

bool TycoParser::has_errors() const {
    return !errors_.empty();
}

std::vector<std::string> TycoParser::get_errors() const {
    return errors_;
}

void TycoParser::clear_errors() {
    errors_.clear();
}

void TycoParser::parse_content(const std::string& content, TycoContext& context) {
    ParseState state;
    preprocess_lines(content, state);
    parse_all_lines(state, context);
}

void TycoParser::preprocess_lines(const std::string& content, ParseState& state) {
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        line = trim(line);
        if (!line.empty() && line[0] != '#') {  // Skip comments and empty lines
            state.lines.push_back(line);
        }
    }
}

void TycoParser::parse_all_lines(ParseState& state, TycoContext& context) {
    state.current_line = 0;
    
    while (state.current_line < state.lines.size()) {
        const std::string& line = state.lines[state.current_line];
        
        try {
            // Try parsing in order of complexity
            if (is_global_variable_line(line)) {
                parse_global_variable(line, context);
            } else if (is_struct_definition_line(line)) {
                parse_struct_definition(line, state);
            } else if (state.in_struct_definition && is_struct_field_line(line)) {
                parse_struct_field(line, state);
            } else if (is_struct_instance_line(line)) {
                parse_struct_instances(line, state, context);
            } else {
                add_error("Unrecognized line format", state.current_line + 1);
            }
        } catch (const std::exception& e) {
            add_error("Parse error: " + std::string(e.what()), state.current_line + 1);
        }
        
        ++state.current_line;
    }
}

bool TycoParser::parse_global_variable(const std::string& line, TycoContext& context) {
    std::regex global_pattern(R"(^\s*(\w+)(\[\])?\s+(\w+):\s*(.+)\s*$)");
    std::smatch match;
    
    if (std::regex_match(line, match, global_pattern)) {
        std::string base_type = match[1];
        std::string array_suffix = match[2];
        std::string name = match[3];
        std::string value_str = match[4];
        
        // Expand templates in global variables (can reference other globals)
        if (has_templates(value_str)) {
            value_str = expand_templates(value_str, context, nullptr, nullptr);
        }
        
        bool is_array = !array_suffix.empty();
        
        if (is_array) {
            // Parse array values
            std::vector<std::string> values = parse_array_values(value_str);
            std::vector<TycoValue> array;
            
            for (auto val : values) {
                // Expand templates in array elements too
                if (has_templates(val)) {
                    val = expand_templates(val, context, nullptr, nullptr);
                }
                array.push_back(parse_value(val, base_type));
            }
            
            context.set_global(name, TycoValue(array));
        } else {
            TycoValue value = parse_value(value_str, base_type);
            context.set_global(name, value);
        }
        
        return true;
    }
    
    return false;
}

bool TycoParser::parse_struct_definition(const std::string& line, ParseState& state) {
    std::regex struct_pattern(R"(^\s*(\w+):\s*$)");
    std::smatch match;
    
    if (std::regex_match(line, match, struct_pattern)) {
        // Finish previous struct if any
        if (state.in_struct_definition) {
            state.in_struct_definition = false;
        }
        
        // Start new struct
        state.current_struct_name = match[1];
        state.current_struct_fields.clear();
        state.current_struct_types.clear();
        state.in_struct_definition = true;
        state.in_struct_instances = false;
        
        return true;
    }
    
    return false;
}

bool TycoParser::parse_struct_field(const std::string& line, ParseState& state) {
    std::regex field_pattern(R"(^\s*\*?(\w+)(\[\])?\s+(\w+):\s*$)");
    std::smatch match;
    
    if (std::regex_match(line, match, field_pattern)) {
        std::string base_type = match[1];
        std::string array_suffix = match[2];
        std::string field_name = match[3];
        
        std::string full_type = base_type + array_suffix;
        
        state.current_struct_fields.push_back(field_name);
        state.current_struct_types.push_back(full_type);
        
        return true;
    }
    
    return false;
}

bool TycoParser::parse_struct_instances(const std::string& line, ParseState& state, TycoContext& context) {
    // Check if this is the start of struct instances (line with dashes)
    if (std::regex_match(line, std::regex(R"(^\s*-\s*.+)"))) {
        if (!state.in_struct_instances) {
            state.in_struct_instances = true;
            state.in_struct_definition = false;
        }
        
        // Parse comma-separated instance values
        std::string instance_data = line.substr(line.find('-') + 1);
        instance_data = trim(instance_data);
        
        std::vector<std::string> values = parse_array_values(instance_data);
        
        if (values.size() != state.current_struct_fields.size()) {
            add_error("Instance field count doesn't match struct definition", state.current_line + 1);
            return false;
        }
        
        // Create instance object with raw values first
        std::unordered_map<std::string, TycoValue> instance;
        
        for (size_t i = 0; i < values.size(); ++i) {
            const std::string& field_name = state.current_struct_fields[i];
            const std::string& field_type = state.current_struct_types[i];
            std::string value_str = values[i];
            
            // Expand templates with current instance scope and parent scope
            if (has_templates(value_str)) {
                value_str = expand_templates(value_str, context, &instance, &state.parent_scope);
            }
            
            instance[field_name] = parse_value(value_str, field_type);
        }
        
        context.add_object(state.current_struct_name, TycoValue(instance));
        return true;
    }
    
    return false;
}

TycoValue TycoParser::parse_value(const std::string& value_str, const std::string& type_name) {
    std::string trimmed = trim(value_str);
    
    // Handle quoted strings
    if (trimmed.length() >= 2 && 
        ((trimmed[0] == '"' && trimmed.back() == '"') ||
         (trimmed[0] == '\'' && trimmed.back() == '\''))) {
        return TycoValue(trimmed.substr(1, trimmed.length() - 2));
    }
    
    // Type-specific parsing
    if (type_name == "bool" || type_name == "boolean") {
        if (trimmed == "true") return TycoValue(true);
        if (trimmed == "false") return TycoValue(false);
    } else if (type_name == "int" || type_name == "integer") {
        try {
            return TycoValue(std::stoll(trimmed));
        } catch (...) {
            add_error("Invalid integer value: " + trimmed, 0);
        }
    } else if (type_name == "float" || type_name == "double") {
        try {
            return TycoValue(std::stod(trimmed));
        } catch (...) {
            add_error("Invalid float value: " + trimmed, 0);
        }
    }
    
    // Auto-detect type if no type specified
    if (trimmed == "true") return TycoValue(true);
    if (trimmed == "false") return TycoValue(false);
    
    // Try integer
    try {
        int64_t int_val = std::stoll(trimmed);
        return TycoValue(int_val);
    } catch (...) {}
    
    // Try float
    try {
        double float_val = std::stod(trimmed);
        return TycoValue(float_val);
    } catch (...) {}
    
    // Default to string
    return TycoValue(trimmed);
}

std::vector<std::string> TycoParser::parse_array_values(const std::string& array_str) {
    std::vector<std::string> values;
    
    // Handle array format like ["val1", "val2", "val3"] or [val1, val2, val3]
    std::string str = trim(array_str);
    
    if (str.front() == '[' && str.back() == ']') {
        str = str.substr(1, str.length() - 2);
    }
    
    // Split by comma
    std::istringstream stream(str);
    std::string token;
    
    while (std::getline(stream, token, ',')) {
        values.push_back(trim(token));
    }
    
    return values;
}

std::string TycoParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

// TycoContext implementation
TycoContext::TycoContext() {}

const TycoValue& TycoContext::get_global(const std::string& name) const {
    auto it = globals_.find(name);
    return (it != globals_.end()) ? it->second : null_value_;
}

TycoValue& TycoContext::get_global(const std::string& name) {
    return globals_[name];
}

void TycoContext::set_global(const std::string& name, const TycoValue& value) {
    globals_[name] = value;
}

const std::vector<TycoValue>& TycoContext::get_objects(const std::string& type_name) const {
    auto it = objects_.find(type_name);
    return (it != objects_.end()) ? it->second : empty_vector_;
}

std::vector<TycoValue>& TycoContext::get_objects(const std::string& type_name) {
    return objects_[type_name];
}

void TycoContext::add_object(const std::string& type_name, const TycoValue& object) {
    objects_[type_name].push_back(object);
}

TycoValue& TycoContext::operator[](const std::string& key) {
    return globals_[key];
}

const TycoValue& TycoContext::operator[](const std::string& key) const {
    return get_global(key);
}

std::vector<std::string> TycoContext::get_global_names() const {
    std::vector<std::string> names;
    for (const auto& pair : globals_) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> TycoContext::get_object_types() const {
    std::vector<std::string> types;
    for (const auto& pair : objects_) {
        types.push_back(pair.first);
    }
    return types;
}

std::string TycoContext::to_json() const {
    // TODO: Implement JSON serialization
    return "{}";
}

bool TycoContext::empty() const {
    return globals_.empty() && objects_.empty();
}

// Convenience functions
TycoContext load(const std::string& filename) {
    TycoParser parser;
    return parser.parse_file(filename);
}

TycoContext loads(const std::string& content) {
    TycoParser parser;
    return parser.parse_string(content);
}

// Helper methods
bool TycoParser::is_global_variable_line(const std::string& line) {
    return std::regex_match(line, std::regex(R"(^\s*(\w+)(\[\])?\s+(\w+):\s*.+\s*$)"));
}

bool TycoParser::is_struct_definition_line(const std::string& line) {
    return std::regex_match(line, std::regex(R"(^\s*(\w+):\s*$)"));
}

bool TycoParser::is_struct_field_line(const std::string& line) {
    return std::regex_match(line, std::regex(R"(^\s*\*?(\w+)(\[\])?\s+(\w+):\s*$)"));
}

bool TycoParser::is_struct_instance_line(const std::string& line) {
    return std::regex_match(line, std::regex(R"(^\s*-\s*.+)"));
}

void TycoParser::add_error(const std::string& message, size_t line_number) {
    std::ostringstream oss;
    oss << "Line " << line_number << ": " << message;
    errors_.push_back(oss.str());
}

// Template expansion methods
std::string TycoParser::expand_templates(const std::string& input, const TycoContext& context, 
                                        const std::unordered_map<std::string, TycoValue>* local_scope,
                                        const std::unordered_map<std::string, TycoValue>* parent_scope) {
    std::string result = input;
    std::regex template_pattern(R"(\{([^}]+)\})");
    std::smatch match;
    
    // Keep expanding until no more templates found (handles nested references)
    bool found_templates = true;
    int max_iterations = 10; // Prevent infinite loops
    int iterations = 0;
    
    while (found_templates && iterations < max_iterations) {
        found_templates = false;
        std::string temp_result;
        std::string::const_iterator start = result.cbegin();
        
        while (std::regex_search(start, result.cend(), match, template_pattern)) {
            // Add text before the match
            temp_result += std::string(start, start + match.prefix().length());
            
            // Get variable name and resolve it
            std::string var_name = match[1].str();
            std::string resolved_value = resolve_variable(var_name, context, local_scope, parent_scope);
            
            temp_result += resolved_value;
            found_templates = true;
            
            // Move past this match
            start = match.suffix().first;
        }
        
        // Add remaining text
        temp_result += std::string(start, result.cend());
        result = temp_result;
        iterations++;
    }
    
    return result;
}

std::string TycoParser::resolve_variable(const std::string& var_name, const TycoContext& context,
                                        const std::unordered_map<std::string, TycoValue>* local_scope,
                                        const std::unordered_map<std::string, TycoValue>* parent_scope) {
    // Handle explicit global access: {global.variable}
    if (var_name.length() > 7 && var_name.substr(0, 7) == "global.") {
        std::string global_var_name = var_name.substr(7);
        const auto& global_value = context.get_global(global_var_name);
        if (!global_value.is_null()) {
            return value_to_string(global_value);
        }
        return "{" + var_name + "}"; // Global variable not found
    }
    
    // Handle parent scope access: {..variable}
    if (var_name.length() > 2 && var_name.substr(0, 2) == "..") {
        std::string parent_var_name = var_name.substr(2);
        if (parent_scope) {
            auto it = parent_scope->find(parent_var_name);
            if (it != parent_scope->end()) {
                return value_to_string(it->second);
            }
        }
        return "{" + var_name + "}"; // Parent variable not found
    }
    
    // Standard scope resolution (local first, then global)
    
    // Check local scope first (current struct instance fields)
    if (local_scope) {
        auto it = local_scope->find(var_name);
        if (it != local_scope->end()) {
            return value_to_string(it->second);
        }
    }
    
    // Check global variables
    const auto& global_value = context.get_global(var_name);
    if (!global_value.is_null()) {
        return value_to_string(global_value);
    }
    
    // Variable not found - return as-is with braces (could be user error)
    return "{" + var_name + "}";
}

std::string TycoParser::value_to_string(const TycoValue& value) {
    if (value.is_string()) {
        return value.as_string();
    } else if (value.is_int()) {
        return std::to_string(value.as_int());
    } else if (value.is_float()) {
        return std::to_string(value.as_float());
    } else if (value.is_bool()) {
        return value.as_bool() ? "true" : "false";
    }
    
    return ""; // For arrays/objects, return empty string
}

std::vector<std::string> TycoParser::extract_template_variables(const std::string& input) {
    std::vector<std::string> variables;
    std::regex template_pattern(R"(\{([^}]+)\})");
    std::sregex_iterator iter(input.begin(), input.end(), template_pattern);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        variables.push_back((*iter)[1].str());
    }
    
    return variables;
}

bool TycoParser::has_templates(const std::string& input) {
    return input.find('{') != std::string::npos && input.find('}') != std::string::npos;
}

} // namespace tyco