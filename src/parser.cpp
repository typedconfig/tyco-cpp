#include "tyco/parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace tyco {

using json = nlohmann::json;

namespace {

std::string build_error_message(const std::string& message, const SourceLocation& location) {
    std::ostringstream oss;
    bool has_path = !location.source.empty();
    bool has_line = location.line > 0;

    if (has_path) {
        oss << location.source;
    }
    if (has_line) {
        if (has_path) {
            oss << ":";
        }
        oss << location.line;
        if (location.column > 0) {
            oss << ":" << location.column;
        }
    }
    if (has_path || has_line) {
        oss << " - ";
    }
    oss << message;
    if (!location.line_text.empty()) {
        oss << "\n    " << location.line_text;
    }
    return oss.str();
}

[[noreturn]] void throw_parse_error(const std::string& message, const SourceLocation& location) {
    throw TycoParseError(message, location);
}

} // namespace

TycoParseError::TycoParseError(const std::string& message, const SourceLocation& location)
    : std::runtime_error(build_error_message(message, location)),
      location_(location) {}

// Helper functions for string processing
static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

static bool starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

static bool has_template(const std::string& str) {
    return str.find('{') != std::string::npos && str.find('}') != std::string::npos;
}

// Normalize datetime to ISO 8601 format
// Converts: "2025-01-15 14:30:00Z" -> "2025-01-15T14:30:00+00:00"
// Converts: "2025-01-15 14:30:00-08:00" -> "2025-01-15T14:30:00-08:00"
static std::string normalize_datetime(const std::string& dt) {
    std::string result = dt;
    
    // Replace space with T
    size_t space_pos = result.find(' ');
    if (space_pos != std::string::npos) {
        result[space_pos] = 'T';
    }
    
    // Replace Z with +00:00
    if (result.back() == 'Z') {
        result.pop_back();
        result += "+00:00";
    }
    
    // Normalize fractional seconds to 6 digits
    // Find the decimal point in the time portion
    size_t dot_pos = result.find('.');
    if (dot_pos != std::string::npos) {
        // Find the end of fractional part (before timezone or end)
        size_t tz_start = result.find_first_of("+-", dot_pos);
        if (tz_start == std::string::npos) {
            tz_start = result.length();
        }
        
        std::string fractional = result.substr(dot_pos + 1, tz_start - dot_pos - 1);
        std::string tz_part = result.substr(tz_start);
        
        // Pad or truncate to 6 digits
        if (fractional.length() < 6) {
            fractional.append(6 - fractional.length(), '0');
        } else if (fractional.length() > 6) {
            fractional = fractional.substr(0, 6);
        }
        
        result = result.substr(0, dot_pos + 1) + fractional + tz_part;
    }
    
    return result;
}

// Normalize time to 6 decimal places for fractional seconds
// Converts: "14:30:00.123" -> "14:30:00.123000"
static std::string normalize_time(const std::string& t) {
    std::string result = t;
    
    // Find decimal point
    size_t dot_pos = result.find('.');
    if (dot_pos != std::string::npos) {
        std::string fractional = result.substr(dot_pos + 1);
        
        // Pad to 6 digits
        if (fractional.length() < 6) {
            fractional.append(6 - fractional.length(), '0');
        } else if (fractional.length() > 6) {
            fractional = fractional.substr(0, 6);
        }
        
        result = result.substr(0, dot_pos + 1) + fractional;
    }
    
    return result;
}

// Process escape sequences in strings
// Handles: \n, \t, \r, \b, \f, \", \\, \uXXXX, \UXXXXXXXX
static std::string process_escape_sequences(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            char next = str[i + 1];
            switch (next) {
                case 'n':  result += '\n'; i++; break;
                case 't':  result += '\t'; i++; break;
                case 'r':  result += '\r'; i++; break;
                case 'b':  result += '\b'; i++; break;
                case 'f':  result += '\f'; i++; break;
                case '"':  result += '"';  i++; break;
                case '\\': result += '\\'; i++; break;
                case 'u':  // Unicode \uXXXX
                    if (i + 5 < str.length()) {
                        std::string hex = str.substr(i + 2, 4);
                        try {
                            int codepoint = std::stoi(hex, nullptr, 16);
                            // Simple UTF-8 encoding for BMP characters
                            if (codepoint < 0x80) {
                                result += static_cast<char>(codepoint);
                            } else if (codepoint < 0x800) {
                                result += static_cast<char>(0xC0 | (codepoint >> 6));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else {
                                result += static_cast<char>(0xE0 | (codepoint >> 12));
                                result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                            i += 5;
                        } catch (...) {
                            result += str[i];  // Keep backslash if invalid
                        }
                    } else {
                        result += str[i];
                    }
                    break;
                case 'U':  // Unicode \UXXXXXXXX
                    if (i + 9 < str.length()) {
                        std::string hex = str.substr(i + 2, 8);
                        try {
                            int codepoint = std::stoi(hex, nullptr, 16);
                            // Very simplified UTF-8 encoding
                            if (codepoint < 0x80) {
                                result += static_cast<char>(codepoint);
                            } else if (codepoint < 0x800) {
                                result += static_cast<char>(0xC0 | (codepoint >> 6));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else if (codepoint < 0x10000) {
                                result += static_cast<char>(0xE0 | (codepoint >> 12));
                                result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else {
                                result += static_cast<char>(0xF0 | (codepoint >> 18));
                                result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                                result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                            i += 9;
                        } catch (...) {
                            result += str[i];
                        }
                    } else {
                        result += str[i];
                    }
                    break;
                default:
                    result += str[i];  // Keep backslash for unknown escapes
                    break;
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

// Strip inline comments (everything after #)
static std::string strip_comment(const std::string& str) {
    size_t pos = str.find('#');
    if (pos != std::string::npos) {
        // Make sure it's not inside quotes
        bool in_quotes = false;
        char quote_char = 0;
        for (size_t i = 0; i < pos; ++i) {
            if (!in_quotes && (str[i] == '"' || str[i] == '\'')) {
                in_quotes = true;
                quote_char = str[i];
            } else if (in_quotes && str[i] == quote_char && (i == 0 || str[i-1] != '\\')) {
                in_quotes = false;
            }
        }
        if (!in_quotes) {
            return trim(str.substr(0, pos));
        }
    }
    return str;
}

// Parse string literal with proper quote handling
static std::string parse_string_literal(const std::string& token, const SourceLocation& location) {
    std::string trimmed = trim(token);
    
    // Check if it's quoted at all
    if (trimmed.empty()) {
        return "";
    }
    
    char first_char = trimmed.front();
    
    // Triple double-quoted string (""")
    if (starts_with(trimmed, "\"\"\"")) {
        size_t end_pos = trimmed.find("\"\"\"", 3);
        if (end_pos == std::string::npos) {
            throw_parse_error("Unclosed triple-quoted string", location);
        }
        std::string content = trimmed.substr(3, end_pos - 3);
        // Strip leading newline if present (common pattern for readability)
        if (!content.empty() && content[0] == '\n') {
            content = content.substr(1);
        }
        // Process escape sequences in triple-quoted strings
        return process_escape_sequences(content);
    }
    
    // Triple single-quoted string (''')
    if (starts_with(trimmed, "'''")) {
        size_t end_pos = trimmed.find("'''", 3);
        if (end_pos == std::string::npos) {
            throw_parse_error("Unclosed triple-quoted string", location);
        }
        // Return content as-is (literal, no escape processing)
        return trimmed.substr(3, end_pos - 3);
    }
    
    // Basic string (")
    if (first_char == '"' && trimmed.back() == '"' && trimmed.length() >= 2) {
        std::string content = trimmed.substr(1, trimmed.length() - 2);
        return process_escape_sequences(content);
    }
    
    // Literal string (')
    if (first_char == '\'' && trimmed.back() == '\'' && trimmed.length() >= 2) {
        return trimmed.substr(1, trimmed.length() - 2);
    }
    
    // Bareword (unquoted string) - just return as-is
    // This is valid for string types in Tyco
    return trimmed;
}

// Parse integer with different bases
static int64_t parse_integer(const std::string& token, const SourceLocation& location) {
    std::string trimmed = trim(token);
    
    try {
        // Handle negative numbers
        bool is_negative = false;
        if (trimmed.front() == '-') {
            is_negative = true;
            trimmed = trimmed.substr(1);
        }
        
        int64_t result;
        
        // Hex: 0xABC
        if (starts_with(trimmed, "0x") || starts_with(trimmed, "0X")) {
            result = std::stoll(trimmed.substr(2), nullptr, 16);
        }
        // Octal: 0o777
        else if (starts_with(trimmed, "0o") || starts_with(trimmed, "0O")) {
            result = std::stoll(trimmed.substr(2), nullptr, 8);
        }
        // Binary: 0b1010
        else if (starts_with(trimmed, "0b") || starts_with(trimmed, "0B")) {
            result = std::stoll(trimmed.substr(2), nullptr, 2);
        }
        // Decimal
        else {
            result = std::stoll(trimmed);
        }
        
        return is_negative ? -result : result;
        
    } catch (const std::exception& e) {
        throw_parse_error("Failed to parse integer '" + token + "': " + e.what(), location);
    }
}

// TycoTime constructor - normalizes fractional seconds to 6 digits
TycoTime::TycoTime(const std::string& v) : value(normalize_time(v)) {}

// TycoDateTime constructor - normalizes to ISO 8601 format
TycoDateTime::TycoDateTime(const std::string& v) : value(normalize_datetime(v)) {}

// TycoString template rendering
void TycoString::render_templates(TycoContext* context, TycoInstance* parent) {
    if (!has_template()) return;
    
    std::string result = value;
    std::regex template_regex(R"(\{([a-zA-Z_][a-zA-Z0-9_\.]*)\})");
    std::smatch match;
    
    while (std::regex_search(result, match, template_regex)) {
        std::string var_path = match[1].str();
        std::string replacement;
        
        // Split by dots for nested access
        auto parts = split(var_path, '.');
        std::shared_ptr<TycoValue> current;
        
        // Start with parent instance or global
        if (parent && parent->get_attribute(parts[0])) {
            current = parent->get_attribute(parts[0]);
        } else {
            current = context->get_global(parts[0]);
        }
        
        // Navigate nested fields
        for (size_t i = 1; i < parts.size() && current; ++i) {
            if (current->type() == TycoType::Instance) {
                auto inst = std::dynamic_pointer_cast<TycoInstance>(current);
                current = inst->get_attribute(parts[i]);
            } else if (current->type() == TycoType::Reference) {
                auto ref = std::dynamic_pointer_cast<TycoReference>(current);
                current = ref->get_resolved();
                if (current && current->type() == TycoType::Instance) {
                    auto inst = std::dynamic_pointer_cast<TycoInstance>(current);
                    current = inst->get_attribute(parts[i]);
                }
            } else {
                current = nullptr;
            }
        }
        
        if (current) {
            replacement = current->to_string();
        } else {
            replacement = "{" + var_path + "}";  // Keep unresolved
        }
        
        result.replace(match.position(), match.length(), replacement);
    }
    
    // Process escape sequences in the expanded template
    // (the template itself had escapes processed at parse time, but substituted
    // content may contain escape sequences that need processing)
    value = process_escape_sequences(result);
    is_template = false;
}

// TycoArray rendering
std::string TycoArray::to_string() const {
    std::string result = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) result += ", ";
        result += items[i]->to_string();
    }
    result += "]";
    return result;
}

std::shared_ptr<TycoValue> TycoArray::clone() const {
    std::vector<std::shared_ptr<TycoValue>> cloned;
    for (const auto& item : items) {
        cloned.push_back(item->clone());
    }
    return std::make_shared<TycoArray>(cloned);
}

void TycoArray::render_templates(TycoContext* context, TycoInstance* parent) {
    for (auto& item : items) {
        item->render_templates(context, parent);
    }
}

// TycoInstance rendering
std::string TycoInstance::to_string() const {
    std::string result = struct_name + "{";
    bool first = true;
    for (const auto& [key, val] : attributes) {
        if (!first) result += ", ";
        result += key + ": " + val->to_string();
        first = false;
    }
    result += "}";
    return result;
}

std::shared_ptr<TycoValue> TycoInstance::clone() const {
    auto cloned = std::make_shared<TycoInstance>(struct_name);
    for (const auto& [key, val] : attributes) {
        cloned->set_attribute(key, val->clone());
    }
    return cloned;
}

void TycoInstance::render_templates(TycoContext* context, TycoInstance* parent) {
    for (auto& [key, val] : attributes) {
        val->render_templates(context, this);
    }
}

// TycoReference resolution
void TycoReference::resolve(TycoContext* context) {
    if (resolved_instance) return;
    
    auto struct_def = context->get_struct(struct_name);
    if (!struct_def) {
        throw_parse_error("Unknown struct: " + struct_name, location);
    }
    
    resolved_instance = struct_def->find_by_primary_key(primary_key_value);
    if (!resolved_instance) {
        throw_parse_error("Cannot find " + struct_name + " with primary key: " + primary_key_value, location);
    }
}

// TycoStruct methods
void TycoStruct::build_primary_key_map() {
    if (primary_key_field.empty()) return;
    
    for (const auto& inst : instances) {
        auto pk_value = inst->get_attribute(primary_key_field);
        if (pk_value) {
            std::string key = pk_value->to_string();
            mapped_instances[key] = inst;
        }
    }
}

std::shared_ptr<TycoInstance> TycoStruct::find_by_primary_key(const std::string& key) const {
    auto it = mapped_instances.find(key);
    return it != mapped_instances.end() ? it->second : nullptr;
}

// TycoContext - resolve inline instances
void TycoContext::resolve_inline_instances() {
    std::function<void(std::shared_ptr<TycoValue>, const std::string&)> resolve_instance_args;
    resolve_instance_args = [&](std::shared_ptr<TycoValue> val, const std::string& expected_type) {
        if (!val) return;
        
        if (val->type() == TycoType::Instance) {
            auto inst = std::dynamic_pointer_cast<TycoInstance>(val);
            auto attrs = inst->get_attributes();
            
            // Check if this instance has _argN fields that need resolution
            std::vector<std::string> arg_keys;
            for (const auto& [key, attr_val] : attrs) {
                if (key.substr(0, 4) == "_arg") {
                    arg_keys.push_back(key);
                }
            }
            
            if (!arg_keys.empty()) {
                // This instance needs resolution - get struct definition
                std::string struct_name = inst->get_struct_name();
                auto struct_def = get_struct(struct_name);
                
                if (struct_def) {
                    const auto& fields = struct_def->get_fields();
                    
                    // Sort arg keys by number (_arg0, _arg1, etc.)
                    std::sort(arg_keys.begin(), arg_keys.end(), [](const std::string& a, const std::string& b) {
                        int num_a = std::stoi(a.substr(4));
                        int num_b = std::stoi(b.substr(4));
                        return num_a < num_b;
                    });
                    
                    // Map _argN to actual field names  
                    for (size_t i = 0; i < arg_keys.size() && i < fields.size(); ++i) {
                        const std::string& arg_key = arg_keys[i];
                        const std::string& field_name = fields[i].name;
                        const std::string& field_type = fields[i].type_name;
                        auto arg_val = inst->get_attribute(arg_key);
                        
                        // The arg_val is currently a string - convert to proper type
                        if (arg_val && arg_val->type() == TycoType::String) {
                            std::string str_val = arg_val->as_string();
                            std::shared_ptr<TycoValue> typed_val;
                            
                            // Parse based on field type
                            if (field_type == "int") {
                                typed_val = std::make_shared<TycoInt>(std::stoll(str_val));
                            } else if (field_type == "float") {
                                typed_val = std::make_shared<TycoFloat>(std::stod(str_val));
                            } else if (field_type == "bool") {
                                typed_val = std::make_shared<TycoBool>(str_val == "true");
                            } else {
                                // Keep as string or other types
                                typed_val = arg_val;
                            }
                            
                            inst->set_attribute(field_name, typed_val);
                        } else {
                            inst->set_attribute(field_name, arg_val);
                        }
                    }
                    
                    // Remove _argN keys
                    for (const std::string& arg_key : arg_keys) {
                        inst->remove_attribute(arg_key);
                    }
                }
            }
            
            // Recursively resolve nested instances
            for (const auto& [key, attr_val] : inst->get_attributes()) {
                resolve_instance_args(attr_val, "");
            }
        } else if (val->type() == TycoType::Array) {
            auto arr = std::dynamic_pointer_cast<TycoArray>(val);
            for (size_t i = 0; i < arr->size(); ++i) {
                resolve_instance_args(arr->get(i), "");
            }
        }
    };
    
    // Resolve in globals
    for (auto& [name, val] : globals) {
        resolve_instance_args(val, "");
    }
    
    // Resolve in struct instances
    for (auto& [name, struct_def] : structs) {
        for (const auto& inst : struct_def->get_instances()) {
            resolve_instance_args(std::static_pointer_cast<TycoValue>(inst), "");
        }
    }
}

// TycoContext rendering pipeline
void TycoContext::render() {
    // Step 0: Resolve inline instances with positional args
    resolve_inline_instances();
    
    // Step 1: Build primary key maps
    for (auto& [name, struct_def] : structs) {
        struct_def->build_primary_key_map();
    }
    
    // Step 2: Resolve all references
    std::function<void(std::shared_ptr<TycoValue>)> resolve_references;
    resolve_references = [&](std::shared_ptr<TycoValue> val) {
        if (!val) return;
        
        switch (val->type()) {
            case TycoType::Reference: {
                auto ref = std::dynamic_pointer_cast<TycoReference>(val);
                ref->resolve(this);
                break;
            }
            case TycoType::Array: {
                auto arr = std::dynamic_pointer_cast<TycoArray>(val);
                for (size_t i = 0; i < arr->size(); ++i) {
                    resolve_references(arr->get(i));
                }
                break;
            }
            case TycoType::Instance: {
                auto inst = std::dynamic_pointer_cast<TycoInstance>(val);
                for (const auto& [key, attr_val] : inst->get_attributes()) {
                    resolve_references(attr_val);
                }
                break;
            }
            default:
                break;
        }
    };
    
    // Resolve in globals
    for (auto& [name, val] : globals) {
        resolve_references(val);
    }
    
    // Resolve in struct instances
    for (auto& [name, struct_def] : structs) {
        for (const auto& inst : struct_def->get_instances()) {
            resolve_references(std::static_pointer_cast<TycoValue>(inst));
        }
    }
    
    // Step 3: Render templates
    for (auto& [name, val] : globals) {
        val->render_templates(this, nullptr);
    }
    
    for (auto& [name, struct_def] : structs) {
        for (const auto& inst : struct_def->get_instances()) {
            inst->render_templates(this, nullptr);
        }
    }
}

// Convert to JSON
static json value_to_json(const std::shared_ptr<TycoValue>& val) {
    switch (val->type()) {
        case TycoType::Null:
            return nullptr;
        case TycoType::Bool:
            return val->as_bool();
        case TycoType::Int:
            return val->as_int();
        case TycoType::Float:
            return val->as_float();
        case TycoType::String:
        case TycoType::Date:
        case TycoType::Time:
        case TycoType::DateTime:
            return val->as_string();
        case TycoType::Array: {
            auto arr = std::dynamic_pointer_cast<TycoArray>(val);
            json j_arr = json::array();
            for (size_t i = 0; i < arr->size(); ++i) {
                j_arr.push_back(value_to_json(arr->get(i)));
            }
            return j_arr;
        }
        case TycoType::Instance: {
            auto inst = std::dynamic_pointer_cast<TycoInstance>(val);
            json j_obj = json::object();
            // Use field order to preserve schema order in output
            for (const auto& key : inst->get_field_order()) {
                auto attr = inst->get_attribute(key);
                if (attr) {
                    j_obj[key] = value_to_json(attr);
                }
            }
            return j_obj;
        }
        case TycoType::Reference: {
            auto ref = std::dynamic_pointer_cast<TycoReference>(val);
            auto resolved = ref->get_resolved();
            if (resolved) {
                return value_to_json(resolved);
            }
            return nullptr;
        }
    }
    return nullptr;
}

json TycoContext::to_object() const {
    json result = json::object();
    
    // Add globals in order
    for (const auto& name : get_global_order()) {
        auto val = get_global(name);
        if (val) {
            result[name] = value_to_json(val);
        }
    }
    
    // Add struct instances in order
    for (const auto& name : get_struct_order()) {
        auto struct_def = get_struct(name);
        if (struct_def && !struct_def->get_primary_key_field().empty()) {
            // Only serialize structs with primary keys (skip inline-only structs)
            json instances = json::array();
            for (const auto& inst : struct_def->get_instances()) {
                instances.push_back(value_to_json(inst));
            }
            result[name] = instances;
        }
    }
    
    return result;
}

std::string TycoContext::to_json() const {
    return to_object().dump(2);
}

// TycoLexer implementation
std::vector<SourceLine> TycoLexer::read_file_with_includes(const std::string& filepath) {
    // Canonicalize path
    std::filesystem::path abs_path = std::filesystem::absolute(filepath);
    std::string canonical = abs_path.string();
    
    if (included_files.count(canonical)) {
        return {};  // Already included
    }
    included_files.insert(canonical);
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        SourceLocation loc(canonical, 0, 1, "");
        throw_parse_error("Cannot open file: " + filepath, loc);
    }
    
    std::vector<SourceLine> lines;
    std::string line;
    std::filesystem::path dir = abs_path.parent_path();
    size_t row = 0;
    
    while (std::getline(file, line)) {
        ++row;
        SourceLocation loc(canonical, row, 1, line);
        std::string trimmed = trim(line);
        
        // Handle #include directive
        if (starts_with(trimmed, "#include")) {
            std::regex include_regex(R"(#include\s+(.+))");
            std::smatch match;
            if (std::regex_match(trimmed, match, include_regex)) {
                std::string included_file = trim(match[1].str());
                
                // Remove quotes if present
                if ((included_file.front() == '"' && included_file.back() == '"') ||
                    (included_file.front() == '\'' && included_file.back() == '\'')) {
                    included_file = included_file.substr(1, included_file.length() - 2);
                }
                
                std::filesystem::path include_path = dir / included_file;
                auto included_lines = read_file_with_includes(include_path.string());
                lines.insert(lines.end(), included_lines.begin(), included_lines.end());
            }
        } else {
            lines.emplace_back(line, loc);
        }
    }
    
    return lines;
}

std::shared_ptr<TycoValue> TycoLexer::parse_value(const std::string& token, const std::string& type_name, const SourceLocation& location) {
    std::string trimmed = trim(strip_comment(token));
    
    // Null
    if (trimmed == "null") {
        return std::make_shared<TycoNull>();
    }
    
    // Boolean
    if (type_name == "bool") {
        if (trimmed == "true" || trimmed == "false") {
            return std::make_shared<TycoBool>(trimmed == "true");
        }
    }
    
    // Integer
    if (type_name == "int") {
        return std::make_shared<TycoInt>(parse_integer(trimmed, location));
    }
    
    // Float
    if (type_name == "float") {
        return std::make_shared<TycoFloat>(std::stod(trimmed));
    }
    
    // Date
    if (type_name == "date") {
        return std::make_shared<TycoDate>(parse_string_literal(trimmed, location));
    }
    
    // Time
    if (type_name == "time") {
        return std::make_shared<TycoTime>(parse_string_literal(trimmed, location));
    }
    
    // DateTime
    if (type_name == "datetime") {
        return std::make_shared<TycoDateTime>(parse_string_literal(trimmed, location));
    }
    
        // String
    if (type_name == "str") {
        // Check if it's a literal string BEFORE parsing
        bool is_literal = starts_with(trimmed, "'");  // ' or '''
        std::string str_val = parse_string_literal(trimmed, location);
        bool is_template = !is_literal && has_template(str_val);
        return std::make_shared<TycoString>(str_val, is_template);
    }
    
    // Struct instance: StructName(...) - can be reference or inline instance
    std::regex struct_call_regex(R"(([A-Z][a-zA-Z0-9_]*)\(([^)]*)\))");
    std::smatch match;
    if (std::regex_match(trimmed, match, struct_call_regex)) {
        std::string struct_name = match[1].str();
        std::string args_str = match[2].str();
        
        // If args contain comma, it's an inline instance
        if (args_str.find(',') != std::string::npos) {
            return parse_inline_instance(struct_name + "(" + args_str + ")", struct_name, location);
        } else {
            // Single argument - it's a reference by primary key
            std::string pk_value = parse_string_literal(args_str, location);
            return std::make_shared<TycoReference>(struct_name, pk_value, location);
        }
    }
    
    // Inline instance: {field1: value1, field2: value2}
    if (trimmed.front() == '{' && trimmed.back() == '}') {
        return parse_inline_instance(trimmed, type_name, location);
    }
    
    // Array: [item1, item2, ...]
    if (trimmed.front() == '[' && trimmed.back() == ']') {
        auto arr = std::make_shared<TycoArray>();
        std::string content = trimmed.substr(1, trimmed.length() - 2);
        
        // Simple comma split (TODO: handle nested brackets properly)
        int depth = 0;
        std::string current;
        for (char c : content) {
            if (c == '[' || c == '{' || c == '(') depth++;
            else if (c == ']' || c == '}' || c == ')') depth--;
            else if (c == ',' && depth == 0) {
                if (!trim(current).empty()) {
                    // Determine element type from array type
                    std::string elem_type = type_name;
                    if (elem_type.back() == ']' && elem_type.find('[') != std::string::npos) {
                        elem_type = elem_type.substr(0, elem_type.find('['));
                    }
                    arr->add(parse_value(current, elem_type, location));
                }
                current.clear();
                continue;
            }
            current += c;
        }
        
        if (!trim(current).empty()) {
            std::string elem_type = type_name;
            if (elem_type.back() == ']' && elem_type.find('[') != std::string::npos) {
                elem_type = elem_type.substr(0, elem_type.find('['));
            }
            arr->add(parse_value(current, elem_type, location));
        }
        
        return arr;
    }
    
    throw_parse_error("Cannot parse value: " + token + " as type " + type_name, location);
}

std::shared_ptr<TycoValue> TycoLexer::parse_inline_instance(const std::string& content, const std::string& struct_name, const SourceLocation& location) {
    // Parse Person(arg1, arg2) or {field1: val1, field2: val2} syntax
    std::string args_str;
    
    if (content.front() == '{' && content.back() == '}') {
        // {field: value, ...} format
        args_str = content.substr(1, content.length() - 2);
    } else if (content.find('(') != std::string::npos) {
        // StructName(arg1, arg2) format
        size_t start = content.find('(');
        size_t end = content.rfind(')');
        if (start == std::string::npos || end == std::string::npos) {
            throw_parse_error("Malformed inline instance: " + content, location);
        }
        args_str = content.substr(start + 1, end - start - 1);
    } else {
        throw_parse_error("Unknown inline instance format: " + content, location);
    }
    
    // Get struct definition to get fields
    // Note: We may not have the struct definition yet if it's defined later
    // For now, create an instance without validation
    auto instance = std::make_shared<TycoInstance>(struct_name);
    
    // Parse arguments (similar to instance line parsing)
    std::vector<std::pair<std::string, std::string>> field_values;
    
    // Split by comma while respecting quotes and brackets
    std::string current;
    int depth = 0;
    bool in_quotes = false;
    char quote_char = 0;
    
    for (size_t i = 0; i < args_str.length(); ++i) {
        char c = args_str[i];
        
        if (!in_quotes) {
            if (c == '"' || c == '\'') {
                in_quotes = true;
                quote_char = c;
                current += c;
                continue;
            } else if (c == '[' || c == '{' || c == '(') {
                depth++;
            } else if (c == ']' || c == '}' || c == ')') {
                depth--;
            } else if (c == ',' && depth == 0) {
                if (!trim(current).empty()) {
                    field_values.push_back({"", trim(current)});
                }
                current.clear();
                continue;
            }
        } else {
            if (c == quote_char && (i == 0 || args_str[i-1] != '\\')) {
                in_quotes = false;
            }
        }
        
        current += c;
    }
    
    if (!trim(current).empty()) {
        field_values.push_back({"", trim(current)});
    }
    
    // Check for "field: value" pattern
    std::regex field_value_regex(R"(^([a-z_][a-zA-Z0-9_]*)\s*:\s*(.+)$)");
    for (auto& [field_name, value_str] : field_values) {
        std::smatch match;
        if (std::regex_match(value_str, match, field_value_regex)) {
            field_name = match[1].str();
            value_str = match[2].str();
        }
    }
    
    // For inline instances, we may not have struct definition yet
    // Store as positional or named arguments
    size_t positional_index = 0;
    for (const auto& [field_name, value_str] : field_values) {
        std::string actual_field_name;
        
        if (field_name.empty()) {
            // Positional argument - use temporary name since we don't have struct def
            // These will need to be resolved later if struct info is needed
            actual_field_name = "_arg" + std::to_string(positional_index++);
        } else {
            actual_field_name = field_name;
        }
        
        // Parse value without knowing type - use generic string parsing
        auto val = parse_value(value_str, "str", location);  // Default to string for now
        instance->set_attribute(actual_field_name, val);
    }
    
    return instance;
}

std::shared_ptr<TycoContext> TycoLexer::parse_string(const std::string& content, const std::string& source_name) {
    std::vector<SourceLine> lines;
    std::istringstream iss(content);
    std::string line;
    size_t row = 0;
    while (std::getline(iss, line)) {
        ++row;
        lines.emplace_back(line, SourceLocation(source_name, row, 1, line));
    }
    return parse_lines(lines);
}

std::shared_ptr<TycoContext> TycoLexer::parse_lines(const std::vector<SourceLine>& lines) {
    auto context = std::make_shared<TycoContext>();
    
    // State machine
    enum class ParseState { TopLevel, InStructSchema, InStructInstances };
    ParseState state = ParseState::TopLevel;
    
    std::shared_ptr<TycoStruct> current_struct;
    struct InstanceLine {
        std::string content;
        SourceLocation location;
    };
    std::vector<InstanceLine> instance_lines;
    
    // Lambda to parse accumulated instance lines for a struct
    auto parse_struct_instances = [this](std::shared_ptr<TycoStruct> struct_def,
                                         const std::vector<InstanceLine>& inst_lines) {
        if (!struct_def || inst_lines.empty()) return;
        
        const auto& fields = struct_def->get_fields();
        
        for (const auto& inst_line : inst_lines) {
            const std::string& inst_text = inst_line.content;
            const SourceLocation& inst_loc = inst_line.location;
            auto instance = std::make_shared<TycoInstance>(struct_def->get_name());
            
            // Parse field values (can be positional or named)
            std::vector<std::pair<std::string, std::string>> field_values;  // (field_name, value_str)
            
            // Split by comma while respecting quotes and brackets
            std::string current;
            int depth = 0;
            bool in_quotes = false;
            char quote_char = 0;
            
            for (size_t i = 0; i < inst_text.length(); ++i) {
                char c = inst_text[i];
                
                if (!in_quotes) {
                    if (c == '"' || c == '\'') {
                        in_quotes = true;
                        quote_char = c;
                        current += c;
                        continue;
                    } else if (c == '[' || c == '{' || c == '(') {
                        depth++;
                    } else if (c == ']' || c == '}' || c == ')') {
                        depth--;
                    } else if (c == ',' && depth == 0) {
                        // Field separator
                        if (!trim(current).empty()) {
                            field_values.push_back({"", trim(current)});
                        }
                        current.clear();
                        continue;
                    }
                } else {
                    if (c == quote_char && (i == 0 || inst_text[i-1] != '\\')) {
                        in_quotes = false;
                    }
                }
                
                current += c;
            }
            
            if (!trim(current).empty()) {
                field_values.push_back({"", trim(current)});
            }
            
            // Check each value for "field: value" pattern
            for (auto& [field_name, value_str] : field_values) {
                // Check if it starts with a valid field name followed by colon
                size_t colon_pos = value_str.find(':');
                if (colon_pos != std::string::npos) {
                    std::string potential_field = value_str.substr(0, colon_pos);
                    potential_field = trim(potential_field);
                    
                    // Check if it's a valid field name
                    if (!potential_field.empty() && 
                        std::isalpha(potential_field[0]) &&
                        std::all_of(potential_field.begin(), potential_field.end(), 
                                    [](char c) { return std::isalnum(c) || c == '_'; })) {
                        field_name = potential_field;
                        value_str = trim(value_str.substr(colon_pos + 1));
                    }
                }
            }
            
            // Assign values to fields
            size_t positional_index = 0;
            bool using_named_fields = false;
            
            for (const auto& [field_name, value_str] : field_values) {
                std::string actual_field_name;
                FieldSchema field_schema;
                bool found = false;
                
                if (field_name.empty()) {
                    // Positional argument
                    if (using_named_fields) {
                        throw_parse_error("Cannot use positional arguments after named arguments", inst_loc);
                    }
                    if (positional_index >= fields.size()) {
                        throw_parse_error("Too many positional arguments for struct", inst_loc);
                    }
                    actual_field_name = fields[positional_index].name;
                    field_schema = fields[positional_index];
                    positional_index++;
                    found = true;
                } else {
                    // Named argument
                    using_named_fields = true;
                    actual_field_name = field_name;
                    for (const auto& f : fields) {
                        if (f.name == field_name) {
                            field_schema = f;
                            found = true;
                            break;
                        }
                    }
                }
                
                if (!found) {
                    throw_parse_error("Unknown field: " + actual_field_name, inst_loc);
                }
                
                // Parse the value
                std::string type_name = field_schema.type_name;
                if (field_schema.is_array) {
                    type_name += "[]";
                }
                
                auto val = parse_value(value_str, type_name, inst_loc);
                instance->set_attribute(actual_field_name, val);
            }
            
            // Apply default values for any fields not specified
            for (const auto& field : fields) {
                if (!instance->has_attribute(field.name) && field.default_value) {
                    instance->set_attribute(field.name, field.default_value);
                }
            }
            
            struct_def->add_instance(instance);
        }
    };
    
    // Helper to check if a string contains an unclosed multiline string delimiter
    auto has_unclosed_multiline = [](const std::string& s, const std::string& delim) -> bool {
        size_t pos = s.find(delim);
        if (pos == std::string::npos) return false;
        // Check if there's a closing delimiter
        size_t close_pos = s.find(delim, pos + delim.length());
        return close_pos == std::string::npos;
    };
    
    for (size_t line_idx = 0; line_idx < lines.size(); ++line_idx) {
        const auto& line_info = lines[line_idx];
        const std::string& line = line_info.text;
        std::string trimmed = trim(line);
        
        // Remove inline comments (but not within strings)
        size_t comment_pos = trimmed.find('#');
        if (comment_pos != std::string::npos) {
            // Simple heuristic: if # is not inside quotes, it's a comment
            // For now, just strip from # onwards (doesn't handle # in strings perfectly)
            bool in_string = false;
            char quote_char = 0;
            for (size_t i = 0; i < comment_pos; ++i) {
                if (!in_string && (trimmed[i] == '"' || trimmed[i] == '\'')) {
                    in_string = true;
                    quote_char = trimmed[i];
                } else if (in_string && trimmed[i] == quote_char && (i == 0 || trimmed[i-1] != '\\')) {
                    in_string = false;
                }
            }
            if (!in_string) {
                trimmed = trim(trimmed.substr(0, comment_pos));
            }
        }
        
        // Skip empty lines
        if (trimmed.empty()) continue;
        
        // Check for struct definition: "StructName:"
        std::regex struct_def_regex(R"(([A-Z][a-zA-Z0-9_]*):)");
        std::smatch match;
        
        if (std::regex_match(trimmed, match, struct_def_regex)) {
            // Parse instances for previous struct if any
            if (current_struct && state == ParseState::InStructInstances) {
                parse_struct_instances(current_struct, instance_lines);
            }
            
            std::string struct_name = match[1].str();
            
            // Check if struct already exists (e.g., from an #include)
            auto existing_struct = context->get_struct(struct_name);
            if (existing_struct) {
                // Reuse existing struct
                current_struct = existing_struct;
            } else {
                // Create new struct
                current_struct = std::make_shared<TycoStruct>(struct_name);
                context->add_struct(current_struct);
            }
            
            state = ParseState::InStructSchema;
            instance_lines.clear();
            continue;
        }
        
        // Check for field schema or global variable
        // Type can be: lowercase (str, int, etc.) OR uppercase (Person, Host, etc.) OR array syntax
        std::regex field_regex(R"(\s*(\*)?(\?)?([a-zA-Z][a-zA-Z0-9_]*)(\[\])?\s+([a-z_][a-zA-Z0-9_]*):(?:\s+(.+))?)");
        
        if (std::regex_match(line, match, field_regex)) {
            bool is_primary = !match[1].str().empty();
            bool is_nullable = !match[2].str().empty();
            std::string type_str = match[3].str();
            bool is_array = !match[4].str().empty();
            std::string name = match[5].str();
            std::string value_str = match[6].str();
            
            // Check if value contains an unclosed multiline string
            if (!value_str.empty() && (has_unclosed_multiline(value_str, "\"\"\"") || 
                                       has_unclosed_multiline(value_str, "'''"))) {
                // Read subsequent lines until we find the closing delimiter
                std::string delimiter = value_str.find("\"\"\"") != std::string::npos ? "\"\"\"" : "'''";
                std::string accumulated = value_str;
                
                // Find position of opening delimiter
                size_t open_pos = accumulated.find(delimiter);
                
                while (line_idx + 1 < lines.size()) {
                    ++line_idx;
                    std::string next_line = lines[line_idx].text;
                    accumulated += "\n" + next_line;
                    
                    // Check if we now have a closing delimiter
                    size_t close_pos = accumulated.find(delimiter, open_pos + delimiter.length());
                    if (close_pos != std::string::npos) {
                        value_str = accumulated;
                        break;
                    }
                }
            }
            
            // Build full type string
            std::string base_type = type_str;
            if (is_array) {
                type_str += "[]";
            }
            
            // Check if this is a global (non-indented line when we might be in a struct)
            bool is_global_line = !starts_with(line, " ") && !starts_with(line, "\t");
            
            if ((state == ParseState::InStructSchema || state == ParseState::TopLevel) ||
                (state == ParseState::InStructInstances && is_global_line)) {
                if (current_struct && !is_global_line) {
                    // Field schema
                    FieldSchema field;
                    field.name = name;
                    field.type_name = base_type;
                    field.is_primary_key = is_primary;
                    field.is_nullable = is_nullable;
                    field.is_array = is_array;
                    
                    if (!value_str.empty()) {
                        // Default value - parse and store
                        std::string default_type = is_array ? (base_type + "[]") : type_str;
                        field.default_value = parse_value(trim(value_str), default_type, line_info.location);
                    }
                    
                    current_struct->add_field(field);
                } else {
                    // Global variable
                    if (!value_str.empty()) {
                        auto val = parse_value(trim(value_str), type_str, line_info.location);
                        context->set_global(name, val);
                    }
                }
            }
            continue;
        }
        
        // Check for default value update: "  fieldname: value" (indented, no type, has colon)
        // This allows updating defaults after schema definition or in files that include base schemas
        std::regex default_update_regex(R"(\s+([a-z_][a-zA-Z0-9_]*):(?:\s+(.+))?)");
        if (current_struct && std::regex_match(line, match, default_update_regex)) {
            std::string field_name = match[1].str();
            std::string value_str = match[2].str();
            
            // Find this field in the struct
            const auto& fields = current_struct->get_fields();
            auto field_it = std::find_if(fields.begin(), fields.end(),
                [&field_name](const FieldSchema& f) { return f.name == field_name; });
            
            if (field_it != fields.end()) {
                // Update the default value for this field
                // We need to modify the struct's field schema
                std::vector<FieldSchema> updated_fields;
                for (const auto& field : fields) {
                    FieldSchema updated_field = field;
                    if (field.name == field_name) {
                        if (!value_str.empty()) {
                            std::string type_str = field.type_name;
                            if (field.is_array) {
                                type_str += "[]";
                            }
                            updated_field.default_value = parse_value(trim(value_str), type_str, line_info.location);
                        } else {
                            // Empty value removes the default
                            updated_field.default_value = nullptr;
                        }
                    }
                    updated_fields.push_back(updated_field);
                }
                
                // Replace struct with updated version
                // Since we can't modify fields directly, we need to create a new struct
                auto updated_struct = std::make_shared<TycoStruct>(current_struct->get_name());
                for (const auto& field : updated_fields) {
                    updated_struct->add_field(field);
                }
                // Copy existing instances
                for (const auto& inst : current_struct->get_instances()) {
                    updated_struct->add_instance(inst);
                }
                context->add_struct(updated_struct);
                current_struct = updated_struct;
                
                continue;
            }
        }
        
        // Check for instance data line: "  - value1, value2, ..."
        if (starts_with(trimmed, "-")) {
            if (current_struct) {
                state = ParseState::InStructInstances;
                std::string inst_line = trimmed.substr(1);  // Remove "-"
                InstanceLine inst{inst_line, line_info.location};
                
                // Handle backslash line continuation
                while (!inst_line.empty() && inst_line.back() == '\\' && line_idx + 1 < lines.size()) {
                    inst_line.pop_back();  // Remove trailing backslash
                    ++line_idx;
                    std::string next_line = lines[line_idx].text;
                    inst_line += trim(next_line);  // Append next line (trimmed)
                    inst.content = inst_line;
                }
                
                // Check if this line has an unclosed multiline string
                if (has_unclosed_multiline(inst_line, "\"\"\"") || has_unclosed_multiline(inst_line, "'''")) {
                    std::string delimiter = inst_line.find("\"\"\"") != std::string::npos ? "\"\"\"" : "'''";
                    size_t open_pos = inst_line.find(delimiter);
                    
                    // Accumulate lines until we find the closing delimiter
                    while (line_idx + 1 < lines.size()) {
                        ++line_idx;
                        std::string next_line = lines[line_idx].text;
                        inst_line += "\n" + next_line;
                        inst.content = inst_line;
                        
                        size_t close_pos = inst_line.find(delimiter, open_pos + delimiter.length());
                        if (close_pos != std::string::npos) {
                            break;
                        }
                    }
                }
                
                inst.content = inst_line;
                instance_lines.push_back(inst);
            }
            continue;
        }
        
        // If in instance state and line is indented (not starting with -), it's a continuation
        if (state == ParseState::InStructInstances && !trimmed.empty() && 
            !line.empty() && (line[0] == ' ' || line[0] == '\t') && !starts_with(trimmed, "-")) {
            // Continuation of previous instance line
            if (!instance_lines.empty()) {
                instance_lines.back().content += " " + trimmed;
            }
            continue;
        }
    }
    
    // Parse remaining instances for the last struct
    parse_struct_instances(current_struct, instance_lines);
    
    return context;
}

std::shared_ptr<TycoContext> TycoLexer::parse_file(const std::string& filepath) {
    auto lines = read_file_with_includes(filepath);
    return parse_lines(lines);
}

} // namespace tyco
