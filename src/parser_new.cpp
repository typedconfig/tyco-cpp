#include "tyco/parser_new.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace tyco {

using json = nlohmann::json;

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
static std::string parse_string_literal(const std::string& token) {
    std::string trimmed = trim(token);
    
    // Check if it's quoted at all
    if (trimmed.empty()) {
        return "";
    }
    
    char first_char = trimmed.front();
    
    // Multiline string (""")
    if (starts_with(trimmed, "\"\"\"")) {
        size_t end_pos = trimmed.find("\"\"\"", 3);
        if (end_pos == std::string::npos) {
            throw std::runtime_error("Unclosed multiline string");
        }
        return trimmed.substr(3, end_pos - 3);
    }
    
    // Basic string (")
    if (first_char == '"' && trimmed.back() == '"' && trimmed.length() >= 2) {
        std::string content = trimmed.substr(1, trimmed.length() - 2);
        // Process escape sequences
        std::string result;
        for (size_t i = 0; i < content.length(); ++i) {
            if (content[i] == '\\' && i + 1 < content.length()) {
                char next = content[i + 1];
                switch (next) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '\\': result += '\\'; break;
                    case '"': result += '"'; break;
                    default: result += next; break;
                }
                ++i;
            } else {
                result += content[i];
            }
        }
        return result;
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
static int64_t parse_integer(const std::string& token) {
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
        throw std::runtime_error("Failed to parse integer '" + token + "': " + e.what());
    }
}

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
    
    value = result;
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
        throw std::runtime_error("Unknown struct: " + struct_name);
    }
    
    resolved_instance = struct_def->find_by_primary_key(primary_key_value);
    if (!resolved_instance) {
        throw std::runtime_error("Cannot find " + struct_name + " with primary key: " + primary_key_value);
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

// TycoContext rendering pipeline
void TycoContext::render() {
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
            for (const auto& [key, attr] : inst->get_attributes()) {
                j_obj[key] = value_to_json(attr);
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

std::string TycoContext::to_json() const {
    json result = json::object();
    
    // Add globals
    for (const auto& [name, val] : globals) {
        result[name] = value_to_json(val);
    }
    
    // Add struct instances
    for (const auto& [name, struct_def] : structs) {
        json instances = json::array();
        for (const auto& inst : struct_def->get_instances()) {
            instances.push_back(value_to_json(inst));
        }
        result[name] = instances;
    }
    
    return result.dump(2);
}

// TycoLexer implementation
std::vector<std::string> TycoLexer::read_file_with_includes(const std::string& filepath) {
    // Canonicalize path
    std::filesystem::path abs_path = std::filesystem::absolute(filepath);
    std::string canonical = abs_path.string();
    
    if (included_files.count(canonical)) {
        return {};  // Already included
    }
    included_files.insert(canonical);
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    
    std::vector<std::string> lines;
    std::string line;
    std::filesystem::path dir = abs_path.parent_path();
    
    while (std::getline(file, line)) {
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
            lines.push_back(line);
        }
    }
    
    return lines;
}

std::shared_ptr<TycoValue> TycoLexer::parse_value(const std::string& token, const std::string& type_name) {
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
        return std::make_shared<TycoInt>(parse_integer(trimmed));
    }
    
    // Float
    if (type_name == "float") {
        return std::make_shared<TycoFloat>(std::stod(trimmed));
    }
    
    // Date
    if (type_name == "date") {
        return std::make_shared<TycoDate>(parse_string_literal(trimmed));
    }
    
    // Time
    if (type_name == "time") {
        return std::make_shared<TycoTime>(parse_string_literal(trimmed));
    }
    
    // DateTime
    if (type_name == "datetime") {
        return std::make_shared<TycoDateTime>(parse_string_literal(trimmed));
    }
    
    // String
    if (type_name == "str") {
        std::string str_val = parse_string_literal(trimmed);
        bool is_template = has_template(str_val);
        return std::make_shared<TycoString>(str_val, is_template);
    }
    
    // Reference: StructName(primary_key_value)
    std::regex ref_regex(R"(([A-Z][a-zA-Z0-9_]*)\(([^)]+)\))");
    std::smatch match;
    if (std::regex_match(trimmed, match, ref_regex)) {
        std::string struct_name = match[1].str();
        std::string pk_value = parse_string_literal(match[2].str());
        return std::make_shared<TycoReference>(struct_name, pk_value);
    }
    
    // Inline instance: {field1: value1, field2: value2}
    if (trimmed.front() == '{' && trimmed.back() == '}') {
        return parse_inline_instance(trimmed, type_name);
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
                    arr->add(parse_value(current, elem_type));
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
            arr->add(parse_value(current, elem_type));
        }
        
        return arr;
    }
    
    throw std::runtime_error("Cannot parse value: " + token + " as type " + type_name);
}

std::shared_ptr<TycoValue> TycoLexer::parse_inline_instance(const std::string& content, const std::string& struct_name) {
    // TODO: Implement inline instance parsing
    throw std::runtime_error("Inline instances not yet implemented");
}

std::shared_ptr<TycoContext> TycoLexer::parse_string(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    
    auto context = std::make_shared<TycoContext>();
    
    // State machine
    enum class ParseState { TopLevel, InStructSchema, InStructInstances };
    ParseState state = ParseState::TopLevel;
    
    std::shared_ptr<TycoStruct> current_struct;
    std::vector<std::string> instance_lines;
    
    for (const std::string& line : lines) {
        std::string trimmed = trim(line);
        
        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#') continue;
        
        // Check for struct definition: "StructName:"
        std::regex struct_def_regex(R"(([A-Z][a-zA-Z0-9_]*):)");
        std::smatch match;
        
        if (std::regex_match(trimmed, match, struct_def_regex)) {
            // Save previous struct if any
            if (current_struct && state == ParseState::InStructInstances) {
                // Parse instance lines
                // TODO: Implement instance parsing
            }
            
            std::string struct_name = match[1].str();
            current_struct = std::make_shared<TycoStruct>(struct_name);
            context->add_struct(current_struct);
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
            
            // Build full type string
            std::string base_type = type_str;
            if (is_array) {
                type_str += "[]";
            }
            
            if (state == ParseState::InStructSchema || state == ParseState::TopLevel) {
                if (current_struct) {
                    // Field schema
                    FieldSchema field;
                    field.name = name;
                    field.type_name = base_type;
                    field.is_primary_key = is_primary;
                    field.is_nullable = is_nullable;
                    field.is_array = is_array;
                    current_struct->add_field(field);
                    
                    if (!value_str.empty()) {
                        // Default value - store for later
                        // TODO: Handle defaults
                    }
                } else {
                    // Global variable
                    if (!value_str.empty()) {
                        auto val = parse_value(trim(value_str), type_str);
                        context->set_global(name, val);
                    }
                }
            }
            continue;
        }
        
        // Check for instance data line: "  - value1, value2, ..."
        if (starts_with(trimmed, "-")) {
            if (current_struct) {
                state = ParseState::InStructInstances;
                instance_lines.push_back(trimmed.substr(1));  // Remove "-"
            }
            continue;
        }
    }
    
    // Parse remaining instances
    if (current_struct && !instance_lines.empty()) {
        const auto& fields = current_struct->get_fields();
        
        for (const std::string& inst_line : instance_lines) {
            auto instance = std::make_shared<TycoInstance>(current_struct->get_name());
            
            // Parse field values (can be positional or named)
            std::vector<std::pair<std::string, std::string>> field_values;  // (field_name, value_str)
            
            // Split by comma while respecting quotes and brackets
            std::string current;
            int depth = 0;
            bool in_quotes = false;
            char quote_char = 0;
            
            for (size_t i = 0; i < inst_line.length(); ++i) {
                char c = inst_line[i];
                
                if (!in_quotes) {
                    if (c == '"' || c == '\'') {
                        in_quotes = true;
                        quote_char = c;
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
                    if (c == quote_char && (i == 0 || inst_line[i-1] != '\\')) {
                        in_quotes = false;
                    }
                }
                
                current += c;
            }
            
            if (!trim(current).empty()) {
                field_values.push_back({"", trim(current)});
            }
            
            // Check each value for "field: value" pattern
            std::regex field_value_regex(R"(^([a-z_][a-zA-Z0-9_]*)\s*:\s*(.+)$)");
            for (auto& [field_name, value_str] : field_values) {
                std::smatch match;
                if (std::regex_match(value_str, match, field_value_regex)) {
                    field_name = match[1].str();
                    value_str = match[2].str();
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
                        throw std::runtime_error("Cannot use positional arguments after named arguments");
                    }
                    if (positional_index >= fields.size()) {
                        throw std::runtime_error("Too many positional arguments for struct");
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
                    throw std::runtime_error("Unknown field: " + actual_field_name);
                }
                
                // Parse the value
                std::string type_name = field_schema.type_name;
                if (field_schema.is_array) {
                    type_name += "[]";
                }
                
                auto val = parse_value(value_str, type_name);
                instance->set_attribute(actual_field_name, val);
            }
            
            current_struct->add_instance(instance);
        }
    }
    
    return context;
}

std::shared_ptr<TycoContext> TycoLexer::parse_file(const std::string& filepath) {
    auto lines = read_file_with_includes(filepath);
    std::string content;
    for (const auto& line : lines) {
        content += line + "\n";
    }
    return parse_string(content);
}

} // namespace tyco
