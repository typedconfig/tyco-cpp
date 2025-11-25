# Tyco C++ Implementation - Feature Gap Analysis

**Date:** 2024  
**Status:** In Progress - Compliance Review  
**Goal:** Ensure tyco-cpp fully implements Tyco v0.1.0 specification

## Executive Summary

The current C++ implementation is **incomplete** and fails to handle many critical Tyco features. This document compares the C++ parser against the Python reference implementation (v0.1.2) and identifies all missing functionality.

## Test Suite Status

| Test File | Status | Notes |
|-----------|--------|-------|
| simple1.tyco | ❌ FAIL | Missing: templates, proper string parsing |
| basic_types.tyco | ⚠️ PARTIAL | Has basic types but missing date/time/datetime |
| number_formats.tyco | ❌ FAIL | Missing: hex (0x), octal (0o), binary (0b) |
| datetime_types.tyco | ❌ FAIL | No date/time/datetime support |
| arrays.tyco | ⚠️ PARTIAL | Basic arrays work, nested arrays untested |
| nullable.tyco | ❌ FAIL | No nullable type support (`?` marker) |
| references.tyco | ❌ FAIL | No reference resolution (`Type(key)`) |
| templates.tyco | ❌ FAIL | Template expansion incomplete/broken |
| defaults.tyco | ❌ FAIL | No default value support |
| quoted_strings.tyco | ❌ FAIL | Missing: multiline `"""`, escape sequences |
| edge_cases.tyco | ❌ FAIL | Various edge cases not handled |

**Overall Compliance: ~15%** (only most basic features work)

## Missing Features (Critical)

### 1. File Inclusion System (`#include`)

**Python Implementation:**
```python
# In TycoLexer.__init__
def _process_includes(self, lines):
    for line in lines:
        if line.strip().startswith('#include'):
            included_path = self._parse_include_directive(line)
            self._load_file(included_path)
```

**C++ Status:** ❌ **NOT IMPLEMENTED**

**Impact:** Cannot compose configurations from multiple files

**Test Coverage:** Not explicitly tested but used in real-world configs

---

### 2. Reference System (`StructName(primary_key_value)`)

**Python Implementation:**
```python
class TycoReference:
    def __init__(self, struct_name, primary_key_value):
        self.struct_name = struct_name
        self.primary_key_value = primary_key_value
        self.resolved_instance = None
    
    def resolve(self, context):
        struct = context.get_struct(self.struct_name)
        self.resolved_instance = struct.find_by_primary_key(self.primary_key_value)
```

**C++ Status:** ❌ **NOT IMPLEMENTED**

**Impact:** Cannot create relationships between structs

**Test Coverage:** `references.tyco`

**Example:**
```tyco
Person:
 *str name:
  int age:
  - "Alice", 30

Team:
 *str team_name:
  Person leader:
  - "Engineering", Person(Alice)  # ← Reference not supported
```

---

### 3. Template Expansion (`{variable}`)

**Python Implementation:**
```python
class TycoValue:
    def render_templates(self, context, parent):
        # Finds {var} patterns and replaces with values
        # Supports nested access: {host.owner.name}
```

**C++ Status:** ⚠️ **PARTIALLY IMPLEMENTED** (incomplete scope resolution)

**Current Issues:**
- No nested field access (`{host.name}` doesn't work)
- Doesn't integrate with rendering pipeline
- Variable resolution is simplistic

**Test Coverage:** `templates.tyco`

**Example:**
```tyco
str environment: production
str message: "Running in {environment}"  # Should expand to "Running in production"

Server:
  str name:
  str desc:
  - "web1", "Server {name}"  # Should expand to "Server web1"
```

---

### 4. Primary Keys (`*` marker)

**Python Implementation:**
```python
class TycoStruct:
    def __init__(self):
        self.primary_key_field = None
        self.mapped_instances = {}  # key -> instance
    
    def build_primary_key_map(self):
        for inst in self.instances:
            key = inst.get_attribute(self.primary_key_field)
            self.mapped_instances[str(key)] = inst
```

**C++ Status:** ⚠️ **PARSER RECOGNIZES BUT DOESN'T USE**

**Current Issues:**
- Parser identifies `*` in schema
- But doesn't build primary key maps
- References can't work without this

**Test Coverage:** `references.tyco`

---

### 5. Nullable Types (`?` marker)

**Python Implementation:**
```python
class FieldSchema:
    is_nullable: bool  # Parsed from `?type field:`
    
# In validation:
if value is None and not field.is_nullable:
    raise ValueError(f"{field.name} cannot be null")
```

**C++ Status:** ❌ **NOT IMPLEMENTED**

**Impact:** Cannot represent optional fields

**Test Coverage:** `nullable.tyco`

**Example:**
```tyco
Person:
  str name:
 ?str email:  # ← Optional field
  - "Alice", null  # email can be null
  - "Bob", "bob@example.com"
```

---

### 6. Number Format Parsing

**Python Implementation:**
```python
def parse_int(token):
    if token.startswith('0x'): return int(token, 16)  # Hex
    if token.startswith('0o'): return int(token, 8)   # Octal
    if token.startswith('0b'): return int(token, 2)   # Binary
    return int(token)  # Decimal
```

**C++ Status:** ❌ **ONLY DECIMAL SUPPORTED**

**Test Coverage:** `number_formats.tyco`

**Example:**
```tyco
int hex_value: 0xFF        # Should be 255
int octal_value: 0o777     # Should be 511
int binary_value: 0b1010   # Should be 10
```

---

### 7. Date/Time/DateTime Types

**Python Implementation:**
```python
# Types: date, time, datetime
# Stored as strings, validated format
date_val = TycoValue("2024-01-15", value_type="date")
```

**C++ Status:** ❌ **NOT IMPLEMENTED**

**Test Coverage:** `datetime_types.tyco`

**Example:**
```tyco
date birthday: "1990-05-15"
time meeting: "14:30:00"
datetime created: "2024-01-15T10:30:00"
```

---

### 8. String Literal Handling

**Python Implementation:**
```python
def parse_string(token):
    if token.startswith('"""'):  # Multiline
        return parse_multiline_string(token)
    elif token.startswith('"'):  # Basic with escapes
        return parse_escaped_string(token)
    elif token.startswith("'"):  # Literal (no escapes)
        return token[1:-1]
```

**C++ Status:** ⚠️ **BASIC ONLY**

**Missing:**
- Multiline strings (`"""content"""`)
- Escape sequences (`\n`, `\t`, `\\`, `\"`)
- Literal strings (single quotes - no escape processing)

**Test Coverage:** `quoted_strings.tyco`

---

### 9. Default Values

**Python Implementation:**
```python
# In schema:
FieldSchema:
  str name:
  default_value: Optional[TycoValue]

# Applied when instance doesn't provide value
```

**C++ Status:** ❌ **NOT IMPLEMENTED**

**Test Coverage:** `defaults.tyco`

**Example:**
```tyco
Server:
  str name:
  str env: "production"  # ← Default value
  - "web1"               # Uses default env="production"
  - "db1", "staging"     # Overrides default
```

---

### 10. Rendering Pipeline

**Python Implementation (Critical!):**
```python
def render(context):
    # Step 1: Build primary key maps for all structs
    for struct in context.structs:
        struct.build_primary_key_map()
    
    # Step 2: Resolve all references
    for value in all_values:
        if isinstance(value, TycoReference):
            value.resolve(context)
    
    # Step 3: Render templates (AFTER references resolved)
    for value in all_values:
        value.render_templates(context)
```

**C++ Status:** ❌ **NO PIPELINE**

**Current Issues:**
- No multi-phase rendering
- Templates rendered before references resolved
- Results in incorrect template expansion

---

### 11. Inline Struct Instances

**Python Implementation:**
```python
# Syntax: {field1: value1, field2: value2}
Config:
  Server server:
  - {name: "web1", port: 8080}  # Inline instance
```

**C++ Status:** ❌ **NOT IMPLEMENTED**

**Test Coverage:** Not in canonical tests yet, but in spec

---

## Architecture Comparison

### Python (Reference)

```
TycoLexer
├── Tokenizes input
├── Processes #include directives
├── Parses schemas and instances
└── Returns TycoContext

TycoContext
├── Stores globals
├── Stores structs
├── render() method:
│   ├── Build primary key maps
│   ├── Resolve references
│   └── Render templates
└── as_object()/dumps_json() helpers (parity with Python)

TycoStruct
├── Schema (fields with types, markers)
├── Instances (list of TycoInstance)
└── Primary key map (for reference resolution)

TycoInstance
├── Attributes (name -> TycoValue)
├── Parent reference (for template scope)
└── render_templates() method

TycoValue types:
├── Primitives: Null, Bool, Int, Float, String
├── Temporal: Date, Time, DateTime
├── Complex: Array, Instance
└── Special: Reference (lazy-loaded)
```

### C++ (Current)

```
TycoParser
├── parse_string() - basic parsing
├── Uses std::variant for values
└── No rendering pipeline

TycoContext
├── Globals storage
├── Objects storage
└── No render() method

Missing:
├── TycoStruct class
├── TycoReference class  
├── Primary key tracking
├── Template rendering system
└── File inclusion
```

---

## Recommended Implementation Plan

### Phase 1: Core Data Model (Week 1)
1. ✅ Create proper class hierarchy matching Python
2. ✅ Implement TycoStruct with field schemas
3. ✅ Implement TycoInstance with attributes
4. ✅ Implement TycoReference with lazy resolution
5. ✅ Add Date/Time/DateTime types

### Phase 2: Lexer Enhancements (Week 1-2)
1. Add #include directive processing
2. Implement all number format parsing (hex/octal/binary)
3. Implement proper string literal parsing:
   - Multiline strings
   - Escape sequences
   - Literal strings (single quotes)
4. Parse primary key (`*`) and nullable (`?`) markers

### Phase 3: Rendering Pipeline (Week 2)
1. Implement TycoContext::render() with 3 phases:
   - Build primary key maps
   - Resolve references
   - Render templates
2. Template expansion with nested field access
3. Reference resolution with error handling

### Phase 4: Advanced Features (Week 2-3)
1. Default values
2. Inline struct instances
3. Comprehensive error messages
4. Edge case handling

### Phase 5: Testing & Validation (Week 3)
1. Run all 11 canonical tests
2. Fix failures iteratively
3. Add C++-specific unit tests
4. Performance optimization

---

## JSON Output Comparison

### Expected (from Python):
```json
{
  "main_contact": {
    "name": "Bob",
    "age": 25
  },
  "Person": [
    {"name": "Alice", "age": 30},
    {"name": "Bob", "age": 25}
  ]
}
```

### Current C++ Output:
- Doesn't produce JSON yet
- Would fail on references
- Templates wouldn't expand
- Missing many types

---

## Critical Blockers for Compliance

1. **References** - Cannot work without primary key maps
2. **Rendering Pipeline** - Templates fail without proper sequencing
3. **Type System** - Missing date/time/datetime
4. **String Parsing** - Critical for real-world configs

---

## Estimated Effort

| Component | Complexity | Time Estimate |
|-----------|------------|---------------|
| Data model rewrite | High | 2-3 days |
| Lexer enhancements | Medium | 2-3 days |
| Rendering pipeline | High | 2-3 days |
| Testing & debugging | High | 3-4 days |
| **Total** | | **2-3 weeks** |

---

## Conclusion

The current C++ implementation is a proof-of-concept that demonstrates basic parsing but lacks the core features needed for Tyco compliance. A significant rewrite is required, following the Python reference implementation's architecture.

**Recommendation:** Either:
1. Complete rewrite following Python's architecture (2-3 weeks)
2. Incremental implementation with continuous testing against canonical test suite
3. Consider if C++ is the right choice - could TypeScript/Go be faster to implement correctly?

**Next Steps:**
1. Add test suite as git submodule ✅
2. Create test harness to run canonical tests
3. Document current test failures
4. Begin Phase 1 implementation
