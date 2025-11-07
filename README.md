# Tyco C++

A C++ parser and library for the Tyco configuration language.

## Overview

This library provides a C++ implementation of the Tyco configuration language parser. It uses a hash map-based approach similar to XML parsing libraries in C++, making it suitable for dynamic configuration loading without requiring compile-time knowledge of the configuration structure.

## Features

- **Dynamic Configuration Loading**: Uses `std::variant` and hash maps for flexible data storage
- **Type Safety**: Strong typing with runtime type checking
- **XML-like API**: Familiar hash map access patterns similar to popular XML libraries
- **Cross-Platform**: Built with CMake for portability
- **Comprehensive Testing**: Google Test integration for reliability

## Architecture

The library follows a hash map-based approach where:

- **TycoValue**: A variant type that can hold any Tyco data type (bool, int, float, string, array, object)
- **TycoContext**: Main container with hash maps for globals and structured objects
- **TycoParser**: Parser that converts Tyco text into the hash map structure

This design is similar to how XML parsing works in C++ - you don't need to define structs at compile time, instead you access data dynamically through hash lookups.

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
./tyco-tests
```

### Using the CLI

```bash
cd build
./tyco-cli config.tyco
```

### Library Usage

```cpp
#include "tyco/parser.h"

int main() {
    // Load from file
    tyco::TycoContext context = tyco::load("config.tyco");
    
    // Access global variables (hash map style)
    std::string env = context["environment"].get_string();
    int port = context["port"].get_int();
    
    // Access structured objects
    const auto& servers = context.get_objects("Server");
    for (const auto& server : servers) {
        std::string name = server["name"].get_string();
        int server_port = server["port"].get_int();
        std::cout << "Server: " << name << ":" << server_port << std::endl;
    }
    
    return 0;
}
```

## API Reference

### TycoValue

The `TycoValue` class is a variant wrapper that can hold any Tyco data type:

```cpp
// Type checking
bool is_string() const;
bool is_int() const;
bool is_float() const;
bool is_bool() const;
bool is_array() const;
bool is_object() const;

// Safe access (returns default if wrong type)
std::string get_string(const std::string& default_val = "") const;
int64_t get_int(int64_t default_val = 0) const;
double get_float(double default_val = 0.0) const;
bool get_bool(bool default_val = false) const;

// Direct access (throws if wrong type)
const std::string& as_string() const;
int64_t as_int() const;
// ... etc

// Container access
TycoValue& operator[](const std::string& key);  // For objects
TycoValue& operator[](size_t index);            // For arrays
```

### TycoContext

The main container for parsed configuration:

```cpp
// Global variable access
const TycoValue& get_global(const std::string& name) const;
TycoValue& operator[](const std::string& key);

// Structured object access
const std::vector<TycoValue>& get_objects(const std::string& type_name) const;
void add_object(const std::string& type_name, const TycoValue& object);

// Introspection
std::vector<std::string> get_global_names() const;
std::vector<std::string> get_object_types() const;
```

## Development Status

This is a skeleton implementation with basic infrastructure in place:

- ✅ Core data structures (TycoValue, TycoContext)
- ✅ Hash map-based storage
- ✅ Type-safe variant system
- ✅ Basic API design
- ✅ Test framework setup
- ✅ CMake build system
- 🚧 **Parser implementation** (currently basic skeleton)
- 🚧 **Template expansion**
- 🚧 **Full Tyco syntax support**

## Next Steps

1. **Complete Parser Implementation**: Implement full Tyco syntax parsing
2. **Template System**: Add support for `{variable}` template expansion
3. **Error Handling**: Improve error reporting and validation
4. **JSON Export**: Complete the `to_json()` methods
5. **Performance**: Optimize for large configuration files
6. **Documentation**: Add more examples and API documentation

## Contributing

The parser implementation needs to be completed to match the Python reference implementation. Key areas:

- Lexical analysis and tokenization
- Struct definition parsing
- Array and object instance parsing
- Template variable expansion
- Global and local scope resolution

## License

MIT License - see the [LICENSE](LICENSE) file for details.