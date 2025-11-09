# Tyco C++ Implementation Status

## Overview

This is the C++ implementation of the Tyco (Typed Configuration) language parser. It is currently **incomplete** and requires significant work to achieve full compliance with the Tyco v0.1.0 specification.

## Current Status: ⚠️ NON-COMPLIANT (~15% feature coverage)

### What Works
- ✅ Basic struct definitions
- ✅ Simple value types (bool, int, float, string)
- ✅ Basic arrays
- ✅ Global variables
- ✅ CMake build system
- ✅ GoogleTest integration

### What's Missing (Critical)
- ❌ File inclusion (`#include` directive)
- ❌ References (`Person(primary_key)`)
- ❌ Primary key system (`*` marker)
- ❌ Nullable types (`?` marker)
- ❌ Template expansion (`{variable}`)
- ❌ Date/time/datetime types
- ❌ Number format support (hex: `0xFF`, octal: `0o777`, binary: `0b1010`)
- ❌ Proper string parsing (multiline `"""`, escape sequences, literal strings)
- ❌ Default values
- ❌ Rendering pipeline (3-phase: primary keys → references → templates)
- ❌ JSON output matching Python format

## Test Suite Integration

The canonical test suite has been added as a git submodule:
```bash
tests/shared/  # Git submodule → tyco-test-suite
├── inputs/    # 11 .tyco test files
└── expected/  # 11 .json expected outputs
```

### Test Results

| Test | Status | Reason |
|------|--------|--------|
| simple1.tyco | ❌ | Missing templates, proper strings |
| basic_types.tyco | ⚠️ | Works partially, missing date/time/datetime |
| number_formats.tyco | ❌ | No hex/octal/binary support |
| datetime_types.tyco | ❌ | No date/time/datetime types |
| arrays.tyco | ⚠️ | Basic arrays work, edge cases untested |
| nullable.tyco | ❌ | No nullable support |
| references.tyco | ❌ | No reference system |
| templates.tyco | ❌ | Incomplete template expansion |
| defaults.tyco | ❌ | No default values |
| quoted_strings.tyco | ❌ | Missing multiline & escapes |
| edge_cases.tyco | ❌ | Various failures |

**Pass Rate: 0/11** (some partial functionality in basic_types and arrays)

## Architecture

### Current Implementation (`parser.h`, `parser.cpp`)
- Uses `std::variant` for value representation
- Basic parsing of structs and globals
- No rendering pipeline
- Incomplete feature set

### Planned Implementation (`parser.h`, `parser.cpp` - WIP)
- Class hierarchy matching Python reference:
  - `TycoValue` (base class)
  - `TycoStruct` (schema + instances)
  - `TycoInstance` (struct instance)
  - `TycoReference` (lazy reference resolution)
  - `TycoArray`, `TycoString`, primitives
- Proper rendering pipeline
- Full feature support

## Building

```bash
cd tyco-cpp
mkdir -p build && cd build
cmake ..
make
```

## Running Tests

```bash
cd build
./tyco-tests  # Current unit tests
```

## Development Roadmap

See [`FEATURE_GAP_ANALYSIS.md`](./FEATURE_GAP_ANALYSIS.md) for detailed feature comparison and implementation plan.

**Estimated time to compliance: 2-3 weeks of focused development**

### Phase 1: Core Data Model
- Implement complete type hierarchy
- Add TycoStruct with field schemas
- Add TycoReference with resolution
- Add date/time/datetime types

### Phase 2: Lexer Enhancements
- File inclusion system
- Number format parsing (hex/octal/binary)
- Proper string literal handling
- Parse all markers (`*`, `?`, `[]`)

### Phase 3: Rendering Pipeline
- Build primary key maps
- Resolve references
- Render templates (with nested field access)

### Phase 4: Testing & Validation
- Run canonical test suite
- Fix failures iteratively
- Achieve 11/11 test pass rate

## Reference Implementation

The Python implementation (`tyco-python/`) is the canonical reference. Key files:
- `tyco-python/tyco/parser.py` - Complete parser (933 lines)
- `tyco-test-suite/` - Canonical tests

## Contributing

**Current Priority:** Achieve spec compliance before accepting contributions.

If you want to help:
1. Review `FEATURE_GAP_ANALYSIS.md`
2. Pick a missing feature
3. Implement it following Python reference
4. Test against canonical test suite
5. Submit PR

## License

MIT License - See LICENSE file

## Contact

Part of the TypedConfig project: https://github.com/typedconfig

## Version History

- **v0.0.1** (Current) - Initial implementation, non-compliant
  - Basic struct parsing
  - Simple value types
  - No advanced features
  - NOT RECOMMENDED FOR USE

- **v0.1.0** (Planned) - First compliant release
  - Full Tyco v0.1.0 spec support
  - All canonical tests passing
  - Production ready
