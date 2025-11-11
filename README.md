# Tyco C++

A complete C++ parser and library for the Tyco configuration language.

## ✅ STATUS: COMPLETE - PRODUCTION READY

**This implementation is fully compliant with the Tyco v0.1.0 specification.**

- **Compliance Rate:** 100% (all features implemented)
- **Test Suite:** 11/11 canonical tests passing ✓
- **Features:** All Tyco v0.1.0 features supported

🎉 **The C++ implementation now matches the Python reference implementation!**

---

## Overview

This library provides a complete C++ implementation of the Tyco configuration language parser. It supports all Tyco v0.1.0 features including references, templates, nullable types, date/time types, and more.

## Features

### Core Language Support
- ✅ **All primitive types**: `str`, `int`, `float`, `bool`
- ✅ **Date/time types**: `date`, `time`, `datetime` with ISO 8601 format
- ✅ **Number formats**: Decimal, hexadecimal (0x), octal (0o), binary (0b)
- ✅ **String literals**: Basic (`"`), literal (`'`), multiline (`"""`, `'''`)
- ✅ **Escape sequences**: `\n`, `\t`, `\r`, `\"`, `\\`, `\uXXXX`, `\UXXXXXXXX`

### Advanced Features
- ✅ **Arrays**: Typed arrays with nullable elements
- ✅ **Nullable types**: Optional fields with `?` marker
- ✅ **Primary keys**: Reference system with `*` marker
- ✅ **References**: Cross-struct references via `Type(primary_key)`
- ✅ **Templates**: Variable expansion with `{variable}` and nested access `{parent.field}`
- ✅ **Default values**: Schema-level defaults applied to instances
- ✅ **Inline instances**: Positional and named arguments
- ✅ **File inclusion**: `#include "path/to/file.tyco"` support
- ✅ **Comments**: Inline (`#`) and end-of-line comments

### Implementation Quality
- **Type Safety**: Strong typing with runtime type checking
- **Memory Safety**: Smart pointers throughout, no raw pointers
- **Cross-Platform**: Built with CMake for portability
- **Comprehensive Testing**: All 11 canonical tests passing
- **JSON Export**: Complete serialization matching Python output

## Quick Start

### Building

```bash
mkdir build && cd build
cmake ..
make
```

### Running Tests

```bash
cd build
ctest  # Run all tests
./tyco-cli path/to/file.tyco  # Parse and output JSON
```

### Using the CLI

```bash
cd build
./tyco-cli config.tyco  # Parses .tyco file and outputs JSON
```

### Library Usage

```cpp
#include "tyco/parser.h"

int main() {
    // Parse a Tyco configuration file
    tyco::TycoParser parser;
    std::shared_ptr<tyco::TycoContext> context = parser.parse_file("config.tyco");

    // Access global configuration values
    auto globals = context->get_globals();
    auto environment = globals["environment"]->as_string();
    auto debug = globals["debug"]->as_bool();
    auto timeout = globals["timeout"]->as_float();

    // Get all instances as objects
    auto objects = context->get_objects();
    auto databases = objects["Database"];
    auto servers = objects["Server"];

    // Access individual instance fields
    auto primaryDb = databases[0];
    auto dbHost = primaryDb->get_attribute("host")->as_string();
    auto dbPort = primaryDb->get_attribute("port")->as_int();

    // Export to JSON
    std::string json_output = context->to_json();
    std::cout << json_output << std::endl;

    return 0;
}
```

## Test Suite

The C++ implementation passes all canonical Tyco v0.1.0 tests:

```
✓ arrays          - Array types with all edge cases
✓ basic_types     - Primitive types (str, int, float, bool)
✓ datetime_types  - Date, time, datetime with ISO 8601
✓ defaults        - Schema-level default values
✓ edge_cases      - Various edge cases and corner cases
✓ nullable        - Optional fields with ? marker
✓ number_formats  - Hex (0x), octal (0o), binary (0b)
✓ quoted_strings  - All quote styles and escape sequences
✓ references      - Primary keys and cross-struct references
✓ simple1         - Comprehensive feature integration test
✓ templates       - Template expansion with nested access

11/11 tests passing (100%)
```

Test files are located in the shared test suite submodule:

```bash
# Initialize submodule if needed
git submodule update --init --recursive

# Test files location:
tests/shared/inputs/    # 11 .tyco test files
tests/shared/expected/  # 11 .json expected outputs
```

## Architecture

The parser uses a multi-stage rendering pipeline matching the Python reference implementation:

### Core Classes
- **TycoValue**: Base class for all value types (polymorphic hierarchy)
  - TycoNull, TycoBool, TycoInt, TycoFloat, TycoString
  - TycoDate, TycoTime, TycoDateTime
  - TycoArray, TycoInstance, TycoReference
- **TycoStruct**: Struct schema definition with field metadata
- **TycoInstance**: Instantiated struct with attribute values
- **TycoContext**: Main container coordinating globals, structs, and rendering
- **TycoParser**: Stateful parser with multi-file support

### Rendering Pipeline
1. **Parse Phase**: Lexical analysis and syntax tree building
2. **Reference Resolution**: Primary key indexing and reference linking
3. **Template Expansion**: Variable substitution with proper scoping

This three-phase approach ensures templates can reference other instances and that circular dependencies are handled correctly.

## API Reference

### TycoParser

```cpp
class TycoParser {
public:
    std::shared_ptr<TycoContext> parse_file(const std::string& filepath);
    std::shared_ptr<TycoContext> parse_string(const std::string& content);
};
```

### TycoContext

```cpp
class TycoContext {
public:
    // Global access
    std::shared_ptr<TycoValue> get_global(const std::string& name);
    void set_global(const std::string& name, std::shared_ptr<TycoValue> value);
    
    // Struct access
    std::shared_ptr<TycoStruct> get_struct(const std::string& name);
    void add_struct(std::shared_ptr<TycoStruct> s);
    
    // JSON export
    std::string to_json() const;
};
```

### TycoValue

```cpp
class TycoValue {
public:
    virtual TycoType type() const = 0;
    virtual std::string to_string() const = 0;
    
    // Type-specific conversions
    virtual bool as_bool() const;
    virtual int64_t as_int() const;
    virtual double as_float() const;
    virtual std::string as_string() const;
};
```
## Reference Documentation

- **Tyco Specification:** See `../web/v0.1.0.html`
- **Python Reference:** See `../tyco-python/tyco/parser.py` (reference implementation)
- **Test Suite:** See `../tyco-python/tyco/tests/` (canonical test suite)

## Implementation Notes

### String Literals
- **Basic strings** (`"..."`): Process escape sequences (`\n`, `\t`, `\uXXXX`, etc.)
- **Literal strings** (`'...'`): No escape processing, templates not expanded
- **Multiline basic** (`""" ... """`): Escape processing, leading newline stripped
- **Multiline literal** (`''' ... '''`): No processing, templates not expanded

### Templates
- Use `{variable}` syntax for expansion
- Support nested access: `{parent.field.subfield}`
- Only expanded in basic strings (not literal strings)
- Escape sequences processed after template expansion

### References
- Primary keys defined with `*` marker in schema
- Reference syntax: `StructName(primary_key_value)`
- References resolved during rendering pipeline
- Circular dependencies handled correctly

### Number Formats
- Decimal: `42`, `-100`, `3.14`
- Hexadecimal: `0xFF`, `0x1A2B`
- Octal: `0o755`, `0o644`
- Binary: `0b1010`, `0b11110000`

## Performance

The parser uses efficient data structures:
- O(1) hash map lookups for globals and struct definitions
- O(1) primary key indexing for reference resolution
- Smart pointers for automatic memory management
- Move semantics for efficient value transfers

## Contributing

Contributions are welcome! The implementation is complete and compliant with v0.1.0. Areas for enhancement:

1. **Performance optimization**: Profiling and optimization opportunities
2. **Error messages**: More detailed parse error reporting
3. **Documentation**: Additional examples and tutorials
4. **Language bindings**: Python/Node.js bindings via FFI
5. **Future versions**: Support for upcoming Tyco language features

Please ensure all tests pass before submitting a PR:
```bash
cd build && ctest
```

## License

MIT License - see the [LICENSE](LICENSE) file for details.
