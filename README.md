# Tyco C++

A C++ parser and library for the Tyco configuration language.

## ⚠️ STATUS: INCOMPLETE - NOT PRODUCTION READY

**This implementation is currently non-compliant with the Tyco v0.1.0 specification.**

- **Compliance Rate:** ~15% (only basic features work)
- **Test Suite:** 0/11 canonical tests passing
- **Missing:** References, templates, #include, nullable types, date/time types, and more

📋 **See [`STATUS.md`](./STATUS.md) for current implementation status**  
📊 **See [`FEATURE_GAP_ANALYSIS.md`](./FEATURE_GAP_ANALYSIS.md) for detailed feature comparison**

**Recommendation:** Use the [Python implementation](../tyco-python) for production. This C++ version needs 2-3 weeks of development to achieve compliance.

---

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
./tyco-tests  # Current unit tests (basic only)
```

### Using the CLI (Limited Functionality)

```bash
cd build
./tyco-cli config.tyco  # Only works with very basic .tyco files
```

**Note:** Many Tyco features will not work. See STATUS.md for details.

### Library Usage (Current API - Subject to Change)

```cpp
#include "tyco/parser.h"

int main() {
    // Load from file (limited feature support)
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

**Warning:** This API does not support:
- References (`Person(primary_key)`)
- Templates (`{variable}`)
- Nullable types
- Date/time types
- Many other features - see FEATURE_GAP_ANALYSIS.md

## Test Suite

The canonical test suite is included as a git submodule:

```bash
# Initialize submodule
git submodule update --init --recursive

# Test files are in:
tests/shared/inputs/    # 11 .tyco test files
tests/shared/expected/  # 11 .json expected outputs
```

**Current test results: 0/11 passing**

## Development Roadmap

See `FEATURE_GAP_ANALYSIS.md` for detailed implementation plan.

**Estimated effort:** 2-3 weeks to achieve full compliance

### Priority Features (Blocking Compliance)
1. References and primary key system
2. Template expansion with proper scoping
3. File inclusion (`#include`)
4. Date/time/datetime types
5. Rendering pipeline (3-phase)
6. Number format parsing (hex/octal/binary)
7. Proper string literal handling

## Reference Documentation

- **Tyco Specification:** See `../web/v0.1.0.html`
- **Python Reference:** See `../tyco-python/tyco/parser.py` (canonical implementation)
- **Test Suite:** See `tests/shared/` (git submodule)

## Architecture (Current vs Planned)

### Current Implementation (`parser.h`, `parser.cpp`)
- Uses `std::variant` for values
- Basic struct parsing
- No rendering pipeline
- **Status:** Non-compliant

### Planned Implementation (`parser_new.h`, `parser_new.cpp` - WIP)
- Proper class hierarchy
- Full feature support matching Python
- 3-phase rendering pipeline
- **Status:** In development

## Contributing

**Note:** This implementation is currently being brought up to spec compliance. If you want to contribute:

1. Read `FEATURE_GAP_ANALYSIS.md`
2. Pick a missing feature
3. Follow the Python reference implementation
4. Add tests from canonical test suite
5. Ensure all tests pass before submitting PR

## Compliance Checklist

- [ ] File inclusion (`#include`)
- [ ] Primary keys (`*` marker)
- [ ] References (`Type(key)`)
- [ ] Nullable types (`?` marker)
- [ ] Template expansion (`{var}`)
- [ ] Date/time/datetime types
- [ ] Number formats (hex/octal/binary)
- [ ] String literals (multiline, escapes)
- [ ] Default values
- [ ] Arrays (all edge cases)
- [ ] Rendering pipeline
- [ ] JSON output matching Python

**Progress:** 0/12 features complete
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