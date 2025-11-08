#ifndef TYCO_PARSER_NEW_H
#define TYCO_PARSER_NEW_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include <variant>
#include <stdexcept>
#include <regex>

namespace tyco {

// Forward declarations
class TycoValue;
class TycoArray;
class TycoReference;
class TycoInstance;
class TycoStruct;
class TycoContext;

// Type enum for runtime type checking
enum class TycoType {
    Null,
    Bool,
    Int,
    Float,
    String,
    Date,
    Time,
    DateTime,
    Array,
    Instance,
    Reference
};

// Base class for all Tyco values
class TycoValue {
public:
    virtual ~TycoValue() = default;
    virtual TycoType type() const = 0;
    virtual std::string to_string() const = 0;
    virtual std::shared_ptr<TycoValue> clone() const = 0;
    
    // Type-specific conversions
    virtual bool as_bool() const { throw std::runtime_error("Not a bool"); }
    virtual int64_t as_int() const { throw std::runtime_error("Not an int"); }
    virtual double as_float() const { throw std::runtime_error("Not a float"); }
    virtual std::string as_string() const { throw std::runtime_error("Not a string"); }
    
    // Template rendering
    virtual void render_templates(TycoContext* context, TycoInstance* parent) {}
};

// Primitive value types
class TycoNull : public TycoValue {
public:
    TycoType type() const override { return TycoType::Null; }
    std::string to_string() const override { return "null"; }
    std::shared_ptr<TycoValue> clone() const override { return std::make_shared<TycoNull>(); }
};

class TycoBool : public TycoValue {
    bool value;
public:
    explicit TycoBool(bool v) : value(v) {}
    TycoType type() const override { return TycoType::Bool; }
    std::string to_string() const override { return value ? "true" : "false"; }
    bool as_bool() const override { return value; }
    std::shared_ptr<TycoValue> clone() const override { return std::make_shared<TycoBool>(value); }
};

class TycoInt : public TycoValue {
    int64_t value;
public:
    explicit TycoInt(int64_t v) : value(v) {}
    TycoType type() const override { return TycoType::Int; }
    std::string to_string() const override { return std::to_string(value); }
    int64_t as_int() const override { return value; }
    std::shared_ptr<TycoValue> clone() const override { return std::make_shared<TycoInt>(value); }
};

class TycoFloat : public TycoValue {
    double value;
public:
    explicit TycoFloat(double v) : value(v) {}
    TycoType type() const override { return TycoType::Float; }
    std::string to_string() const override { return std::to_string(value); }
    double as_float() const override { return value; }
    std::shared_ptr<TycoValue> clone() const override { return std::make_shared<TycoFloat>(value); }
};

class TycoString : public TycoValue {
    std::string value;
    bool is_template;
public:
    explicit TycoString(const std::string& v, bool tpl = false) : value(v), is_template(tpl) {}
    TycoType type() const override { return TycoType::String; }
    std::string to_string() const override { return value; }
    std::string as_string() const override { return value; }
    std::shared_ptr<TycoValue> clone() const override { return std::make_shared<TycoString>(value, is_template); }
    
    bool has_template() const { return is_template; }
    void set_value(const std::string& v) { value = v; }
    void render_templates(TycoContext* context, TycoInstance* parent) override;
};

// Date/time types (stored as strings)
class TycoDate : public TycoValue {
    std::string value;
public:
    explicit TycoDate(const std::string& v) : value(v) {}
    TycoType type() const override { return TycoType::Date; }
    std::string to_string() const override { return value; }
    std::string as_string() const override { return value; }
    std::shared_ptr<TycoValue> clone() const override { return std::make_shared<TycoDate>(value); }
};

class TycoTime : public TycoValue {
    std::string value;
public:
    explicit TycoTime(const std::string& v);  // Normalizes in constructor
    TycoType type() const override { return TycoType::Time; }
    std::string to_string() const override { return value; }
    std::string as_string() const override { return value; }
    std::shared_ptr<TycoValue> clone() const override { return std::make_shared<TycoTime>(value); }
};

class TycoDateTime : public TycoValue {
    std::string value;
public:
    explicit TycoDateTime(const std::string& v);  // Normalizes in constructor
    TycoType type() const override { return TycoType::DateTime; }
    std::string to_string() const override { return value; }
    std::string as_string() const override { return value; }
    std::shared_ptr<TycoValue> clone() const override { return std::make_shared<TycoDateTime>(value); }
};

// Complex types
class TycoArray : public TycoValue {
    std::vector<std::shared_ptr<TycoValue>> items;
public:
    TycoArray() = default;
    explicit TycoArray(const std::vector<std::shared_ptr<TycoValue>>& v) : items(v) {}
    
    TycoType type() const override { return TycoType::Array; }
    std::string to_string() const override;
    std::shared_ptr<TycoValue> clone() const override;
    
    void add(std::shared_ptr<TycoValue> item) { items.push_back(item); }
    size_t size() const { return items.size(); }
    std::shared_ptr<TycoValue> get(size_t idx) const { return items[idx]; }
    const std::vector<std::shared_ptr<TycoValue>>& get_items() const { return items; }
    
    void render_templates(TycoContext* context, TycoInstance* parent) override;
};

class TycoInstance : public TycoValue {
    std::string struct_name;
    std::unordered_map<std::string, std::shared_ptr<TycoValue>> attributes;
    std::vector<std::string> field_order;  // Track insertion order
    TycoInstance* parent_instance;
    
public:
    TycoInstance(const std::string& name) : struct_name(name), parent_instance(nullptr) {}
    
    TycoType type() const override { return TycoType::Instance; }
    std::string to_string() const override;
    std::shared_ptr<TycoValue> clone() const override;
    
    void set_attribute(const std::string& name, std::shared_ptr<TycoValue> value) {
        if (attributes.find(name) == attributes.end()) {
            field_order.push_back(name);
        }
        attributes[name] = value;
    }
    
    std::shared_ptr<TycoValue> get_attribute(const std::string& name) const {
        auto it = attributes.find(name);
        return it != attributes.end() ? it->second : nullptr;
    }
    
    bool has_attribute(const std::string& name) const {
        return attributes.find(name) != attributes.end();
    }
    
    void remove_attribute(const std::string& name) {
        attributes.erase(name);
        field_order.erase(std::remove(field_order.begin(), field_order.end(), name), field_order.end());
    }
    
    const std::unordered_map<std::string, std::shared_ptr<TycoValue>>& get_attributes() const {
        return attributes;
    }
    
    const std::vector<std::string>& get_field_order() const {
        return field_order;
    }
    
    std::string get_struct_name() const { return struct_name; }
    
    void set_parent(TycoInstance* p) { parent_instance = p; }
    TycoInstance* get_parent() const { return parent_instance; }
    
    void render_templates(TycoContext* context, TycoInstance* parent) override;
};

class TycoReference : public TycoValue {
    std::string struct_name;
    std::string primary_key_value;
    mutable std::shared_ptr<TycoInstance> resolved_instance;
    
public:
    TycoReference(const std::string& sname, const std::string& pk)
        : struct_name(sname), primary_key_value(pk), resolved_instance(nullptr) {}
    
    TycoType type() const override { return TycoType::Reference; }
    std::string to_string() const override { return struct_name + "(" + primary_key_value + ")"; }
    std::shared_ptr<TycoValue> clone() const override {
        return std::make_shared<TycoReference>(struct_name, primary_key_value);
    }
    
    std::string get_struct_name() const { return struct_name; }
    std::string get_primary_key_value() const { return primary_key_value; }
    
    void resolve(TycoContext* context);
    std::shared_ptr<TycoInstance> get_resolved() const { return resolved_instance; }
};

// Struct schema definition
struct FieldSchema {
    std::string name;
    std::string type_name;  // "str", "int", "Person", etc.
    bool is_primary_key;
    bool is_nullable;
    bool is_array;
    std::shared_ptr<TycoValue> default_value;
    
    FieldSchema() : is_primary_key(false), is_nullable(false), is_array(false), default_value(nullptr) {}
};

class TycoStruct {
    std::string name;
    std::vector<FieldSchema> fields;
    std::string primary_key_field;
    std::vector<std::shared_ptr<TycoInstance>> instances;
    std::unordered_map<std::string, std::shared_ptr<TycoInstance>> mapped_instances;
    
public:
    explicit TycoStruct(const std::string& n) : name(n) {}
    
    void add_field(const FieldSchema& field) {
        fields.push_back(field);
        if (field.is_primary_key) {
            primary_key_field = field.name;
        }
    }
    
    void add_instance(std::shared_ptr<TycoInstance> inst) {
        instances.push_back(inst);
    }
    
    std::string get_name() const { return name; }
    const std::vector<FieldSchema>& get_fields() const { return fields; }
    std::string get_primary_key_field() const { return primary_key_field; }
    const std::vector<std::shared_ptr<TycoInstance>>& get_instances() const { return instances; }
    
    void build_primary_key_map();
    std::shared_ptr<TycoInstance> find_by_primary_key(const std::string& key) const;
};

// Main context class
class TycoContext {
    std::unordered_map<std::string, std::shared_ptr<TycoValue>> globals;
    std::vector<std::string> global_order;  // Track insertion order
    std::unordered_map<std::string, std::shared_ptr<TycoStruct>> structs;
    std::vector<std::string> struct_order;  // Track insertion order
    
public:
    TycoContext() = default;
    
    void set_global(const std::string& name, std::shared_ptr<TycoValue> value) {
        if (globals.find(name) == globals.end()) {
            global_order.push_back(name);
        }
        globals[name] = value;
    }
    
    std::shared_ptr<TycoValue> get_global(const std::string& name) const {
        auto it = globals.find(name);
        return it != globals.end() ? it->second : nullptr;
    }
    
    const std::unordered_map<std::string, std::shared_ptr<TycoValue>>& get_globals() const {
        return globals;
    }
    
    const std::vector<std::string>& get_global_order() const {
        return global_order;
    }
    
    void add_struct(std::shared_ptr<TycoStruct> s) {
        if (structs.find(s->get_name()) == structs.end()) {
            struct_order.push_back(s->get_name());
        }
        structs[s->get_name()] = s;
    }
    
    std::shared_ptr<TycoStruct> get_struct(const std::string& name) const {
        auto it = structs.find(name);
        return it != structs.end() ? it->second : nullptr;
    }
    
    const std::unordered_map<std::string, std::shared_ptr<TycoStruct>>& get_structs() const {
        return structs;
    }
    
    const std::vector<std::string>& get_struct_order() const {
        return struct_order;
    }
    
    // Rendering pipeline
    void render();
    std::string to_json() const;
    
    // Resolve inline instances with positional args (_arg0, _arg1, etc.)
    void resolve_inline_instances();
};

// Lexer and parser
class TycoLexer {
    std::string base_path;
    std::unordered_set<std::string> included_files;
    
    std::vector<std::string> read_file_with_includes(const std::string& filepath);
    std::shared_ptr<TycoValue> parse_value(const std::string& token, const std::string& type_name);
    std::shared_ptr<TycoValue> parse_inline_instance(const std::string& content, const std::string& struct_name);
    
public:
    TycoLexer() = default;
    
    std::shared_ptr<TycoContext> parse_file(const std::string& filepath);
    std::shared_ptr<TycoContext> parse_string(const std::string& content);
};

} // namespace tyco

#endif // TYCO_PARSER_NEW_H
