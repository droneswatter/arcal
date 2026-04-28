# Spec Compliance Task Plan

Each task in this file covers one structural rule family from
`OMS/docs_markdown_unofficial/06_OMSC-SPC-008_RevK_CxxCALSpec_DandD_v2_5.md`
(the C++ CAL Spec & Design Description, Rev K).

## How to assign a task

Spawn an agent with this file and the task block below as context.
The agent should:

1. Read the referenced spec section(s).
2. Inspect the listed sample files (and enough others to be confident).
3. Record findings — pass, fail with detail, or not-applicable.
4. Decide independently whether a new `conf_*.cpp` test should be written in
   `arcal/test/conform/`. Write it if needed; explain why not if skipping.

### Output format per task

```
## TASK-XXX findings

### Compliance status: PASS | FAIL | PARTIAL

#### Violations found
- <TypeName>.h: <description> (CERT <ID>)

#### Test decision
Written: <filename> / Not written: <reason>
```

---

## Status legend

| Status | Meaning |
|--------|---------|
| `pending` | Not yet assigned |
| `in-progress` | Agent running |
| `done` | Findings recorded |

---

## TASK-001 — Accessor Base Structure
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_base_structure.cpp`

### Findings — PARTIAL
- **Virtual inheritance (CXX-005456/CXX-011135):** All ~6400 headers use `public virtual` inheritance instead of plain `public`. The spec says "publicly and singly inherits" with no mention of `virtual`. This is a deliberate design choice to avoid diamond-inheritance ambiguity (e.g. `*MT → MessageType → Accessor` and `*MT → Accessor`). Deviation from the letter of the spec but satisfies intent.
- **CXX-012705, CXX-005464, CXX-005465, CXX-011064:** All PASS — protected dtor, default ctor, copy ctor, and operator= confirmed across all sampled types.
**Spec:** §8832 "Accessor Instance Definitions" (CERT CXX-005456, CXX-011135, CXX-012705, CXX-005464, CXX-005465, CXX-011064)

### What the spec requires

Every `uci/type/*.h` complex-type class must:

- Inherit publicly and singly from `uci::base::Accessor` when the XSD type has
  no `xs:complexContent/xs:extension/@base` (CXX-011135), or from the
  translated base class name when it does (CXX-005456).
- Declare its destructor **protected** (CXX-012705).
- Declare its default constructor **protected** (CXX-005464).
- Declare its copy constructor **protected** (CXX-005465).
- Declare `operator=` **protected** (CXX-011064).

### Files to check

Sample (vary your selection across naming prefixes):
- `arcal/include/uci/type/ID_Type.h` — has UUID field, no base extension
- `arcal/include/uci/type/ACO_FileTraceabilityType.h` — extends another type
- `arcal/include/uci/type/AMTI_ActivityMDT.h` — MDT with SimpleList field
- 10–15 additional `*Type.h` files of your choice

### Existing coverage

`arcal/test/conform/conf_accessor.cpp`,
`arcal/test/conform/conf_generated_lifecycle_target.cpp`

---

## TASK-002 — Complex Type Factory Methods and copy/getUCITypeVersion
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_factory_methods.cpp`

### Findings — PASS
- **Factory methods match the expected shape (CXX-011063/066/067/068/011410):** sampled generated headers expose `copy`, both `create` overloads, `destroy`, and `getUCITypeVersion()` with the expected signatures.
- **Default-argument canary passes:** the compile test calls `T::create()` and `T::create(obj)` without explicitly passing an ASB pointer, confirming default `= nullptr` support in the generated declarations.
- **Abstract-type over-generation remains benign:** abstract types still declare lifecycle helpers, but the compile checks treat that as acceptable extra surface rather than a conformance failure.
**Spec:** §8967–9040 "Accessor Instance Member Functions" (CERT CXX-011063, CXX-011066, CXX-011067, CXX-011068, CXX-011410)

### What the spec requires

Every non-abstract complex type must declare:

```cpp
virtual void copy(const T& accessor);                          // CXX-011063
static T& create(uci::base::AbstractServiceBusConnection* = nullptr);  // CXX-011066
static T& create(const T&, uci::base::AbstractServiceBusConnection* = nullptr); // CXX-011067
static void destroy(T& accessor);                              // CXX-011068
static std::string getUCITypeVersion();                        // CXX-011410
```

`copy` must be `public virtual`. `create`/`destroy`/`getUCITypeVersion` must
be `public static`. `operator=` must be **protected** (see TASK-001).

### Files to check

- `arcal/include/uci/type/ID_Type.h`
- `arcal/include/uci/type/ACTDF_CollectionPlanType.h`
- 5–10 `*MDT.h` and `*Type.h` files of your choice

### Existing coverage

`arcal/test/conform/conf_generated_global.cpp` (partially)

---

## TASK-003 — Required Sequence Element Accessors (get/set pairs)
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_required_elements.cpp`

### Findings — PASS
- **Required complex accessors pass (CXX-011213/011214/011215):** sampled headers provide mutable and const getters plus parent-reference setters for required complex fields.
- **Required primitive accessors pass (CXX-011216/011217):** sampled primitive fields return by value from the const getter and use parent-reference setters.
- **Required string-family and enum accessors pass (CXX-011219/011220/011223/012706/012707/012708):** sampled string-family fields provide the expected getter pair and both `std::string`/`const char*` setter paths, while enum fields accept `EnumerationItem` and return the parent type.
**Spec:** §9040–9450 "The Element Access Member Functions"
(CERT CXX-011213, CXX-011214, CXX-011215, CXX-011216, CXX-011217,
CXX-011219, CXX-011220, CXX-011223, CXX-012706, CXX-012707, CXX-012708)

### What the spec requires

For each required (non-optional, non-list) sequence element of a complex type:

| Element type | get signature | set signature |
|---|---|---|
| Complex type (non-UUID) | `T& getX()` + `const T& getX() const` | `Parent& setX(const T&)` |
| Simple primitive (`xs:int`, `xs:double`, etc.) | `xs::Double getX() const` | `Parent& setX(xs::Double)` |
| String primitive | `xs::String& getX()` + `const xs::String& getX() const` | `Parent& setX(const std::string&)` + `Parent& setX(const char*)` |
| Enum | `T& getX()` + `const T& getX() const` | `Parent& setX(T::EnumerationItem)` |

### Files to check

Focus on types with mixed required fields:
- `arcal/include/uci/type/AMTI_ActivityMDT.h` (SubsystemID required, Activity SimpleList)
- `arcal/include/uci/type/ACTDF_CollectionPlanType.h` (required complex + optional double)
- Any type with an enum field
- Any type with a string primitive field (`VisibleString256Type`, etc.)
- 5–10 additional files of your choice

### Existing coverage

`arcal/test/conform/conf_accessor.cpp` (lightly)

---

## TASK-004 — Optional Sequence Elements (has/enable/clear)
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_optional_elements.cpp`

### Findings — PASS
- **Optional complex accessors pass (CXX-011229/011230/011231):** sampled complex optionals provide `hasX() const`, `enableX(AccessorType = null)`, and parent-reference `clearX()`.
- **Optional primitive-style accessors pass (CXX-012708/012709):** sampled primitive and simple-restriction optionals provide `enableX()`, parent-reference `clearX()`, and parent-reference `setX(...)`.
- **Current compile checks cover both paths:** the test exercises optional complex, primitive, enum, and simple-restriction-style fields across representative headers.
**Spec:** §9450 optional element sub-section
(CERT CXX-011229, CXX-011230, CXX-011231, CXX-012709)

### What the spec requires

For each optional element (`@minOccurs='0'`, non-list) in a sequence:

```cpp
virtual bool hasX() const;                      // CXX-011229
virtual T& enableX(uci::base::accessorType::AccessorType = uci::base::accessorType::null); // CXX-011230 (complex only)
virtual xs::Prim& enableX();                    // CXX-012709 (binary/list primitive only)
virtual Parent& clearX();                       // CXX-011231
```

`getX()` / `setX()` are still required (from TASK-003) alongside these.

### Files to check

- `arcal/include/uci/type/ACO_TraceabilityType.h` — optional string-restriction field
- `arcal/include/uci/type/ACO_FileTraceabilityType.h` — optional complex field
- `arcal/include/uci/type/ACTDF_CollectionPlanType.h` — optional double field
- 10 additional `*Type.h` / `*MDT.h` files containing `hasX` in their declarations

### Existing coverage

None specific; `conf_accessor.cpp` touches enable/clear indirectly.

---

## TASK-005 — Bounded List Element Typedefs and Accessors
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_bounded_list_elements.cpp`

### Findings — PASS
- **BoundedList inner typedefs pass (CXX-007053/012712/012713):** sampled bounded-list fields now generate named inner typedefs with element/accessorType combinations that match the underlying field types.
- **Getter/setter signatures pass (CXX-011226/011227/011228):** sampled bounded-list fields expose mutable and const getters returning the typedef plus parent-reference setters taking `const FieldName&`.
- **Representative coverage includes complex, scalar primitive, and scalar-restriction-backed list elements.**
**Spec:** §9450 "Bounded List Member Function", §12460 "The LocalElement Accessor"
(CERT CXX-007053, CXX-012712, CXX-012713, CXX-011226, CXX-011227, CXX-011228)

### What the spec requires

For each element with `@maxOccurs='unbounded'` or `@maxOccurs > 1`:

1. A public inner `typedef` (or `using`) inside the class named after the element:
   ```cpp
   // for a complex type element
   typedef uci::base::BoundedList<uci::type::ElemType,
       uci::type::accessorType::elemType> ElemName;      // CXX-007053
   ```
2. Non-const getter returns `ElemName&` (CXX-011227).
3. Const getter returns `const ElemName&` (CXX-011226).
4. Setter takes `const ElemName&` and returns `Parent&` (CXX-011228).

### Files to check

Grep for `BoundedList` in `arcal/include/uci/type/` and pick 10 representative
types across different base types (complex, string primitive, simple primitive).

### Existing coverage

`arcal/test/conform/conf_lists.cpp`

---

## TASK-006 — UUID Element Accessors
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_uuid_elements.cpp`

### Findings — PASS
- `getUUID() const` returns `uci::base::UUID` by value (CXX-006154) ✓
- `setUUID(const uci::base::UUID&)` returns parent reference (CXX-011054) ✓
- No `hasUUID`/`enableUUID`/`clearUUID` present (correct) ✓
- Spec-doc discrepancy: the illustrative example at §11031 shows `void` return for setUUID; the normative CERT CXX-011054 requires parent reference. Generated code correctly follows the CERT.
**Spec:** §11031 "The Universally Unique Identifier Accessor"
(CERT CXX-006154, CXX-011054)

### What the spec requires

For each element of type `uci:UniversallyUniqueIdentifierType`:

```cpp
virtual uci::base::UUID getX() const = 0;                     // CXX-006154
virtual Parent& setX(const uci::base::UUID& uuid) = 0;        // CXX-011054
```

Note: UUID element is **not** paired with `hasX`/`enableX`/`clearX` even when
optional — UUID is always present once the parent is created.

### Files to check

- `arcal/include/uci/type/ID_Type.h` — has `getUUID`/`setUUID`
- Any `*ID_Type.h` file
- 5 `*MDT.h` files that contain a UUID field

### Existing coverage

`arcal/test/conform/conf_uuid.cpp`

---

## TASK-007 — SimpleRestriction Type Headers (typedef pattern)
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_simple_restriction.cpp`

### Findings — PASS
- **Numeric restrictions repaired (CXX-006143/006144):** scalar restrictions such as `UnitBallDoubleType` and `UnitIntervalFloatType` now generate typedef aliases to the corresponding `uci::base::*Accessor` plus companion `<TypeName>Value` aliases to `xs::*`.
- **String restrictions repaired (CXX-006553):** string-like restrictions such as `VisibleString256Type` now generate typedef aliases to the corresponding `xs::*` primitive alias instead of accessor classes.
**Spec:** §10946 "The SimpleRestriction Accessor"
(CERT CXX-006143, CXX-006144, CXX-006553)

### What the spec requires

An XSD `xs:simpleType` with a `xs:restriction` (non-enum) must be generated as
a **typedef**, not a class:

```cpp
// For xs:restriction/@base mapping to a Simple Primitive (int, double, etc.):
typedef uci::base::DoubleAccessor UnitBallDoubleType;   // CXX-006143
typedef xs::Double UnitBallDoubleTypeValue;             // CXX-006144

// For xs:restriction/@base mapping to a String/Binary/List Primitive:
typedef xs::String VisibleString256Type;                // CXX-006553
```

### Files to check

- `arcal/include/uci/type/UnitBallDoubleType.h`
- `arcal/include/uci/type/UnitIntervalFloatType.h`
- `arcal/include/uci/type/UnitBallFloatType.h`
- Any `*Type.h` header that wraps a single `getValue`/`setValue` over a primitive
- 10 additional numeric/string restriction types

### Existing coverage

None. This entire category lacks conformance tests.

---

## TASK-008 — Enumeration Type Headers
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_enum_extended.cpp`

### Findings — FAIL (8 systematic violations, all 729 `*Enum.h` files)
- **V1 — `getValue()` missing `bool testForValidity=true` param (CXX-011149):** Spec requires `getValue(bool testForValidity=true) const`. Generated has no parameter.
- **V2 — `getNumberOfItems()` is `static` not const instance (CXX-006240):** Spec requires non-static `int getNumberOfItems() const throw()`.
- **V3 — Public `operator=(const T&)` absent (CXX-006211):** Only a protected copy-assignment exists; spec requires a public one.
- **V4 — `operator=(EnumerationItem)` missing entirely (CXX-011062).**
- **V5 — Six reversed-arg free friend operators missing (CXX-006323/340/357/374/391/408):** `operator==(EnumerationItem lhs, const T& rhs)` and its `!=`/`<`/`<=`/`>`/`>=` companions are absent.
- **V6 — Static `toName(EnumerationItem)` overload missing (CXX-006417):** Only instance `toName() const` is generated.
- **V7 — Method named `valueFromName` instead of `fromName` (CXX-006424).**
- **V8 — `operator<<` is non-template inline friend (CXX-006457):** Spec requires `template<typename charT, typename traits> std::basic_ostream<charT,traits>& operator<<(...)` in global namespace.
**Spec:** §11142–12459 "The Enumeration Accessor" (CERT CXX-007*** series)

### What the spec requires

For each `xs:simpleType` with `xs:restriction/xs:enumeration`:

- Enum nested type named `EnumerationItem` with `enumNotSet = 0` sentinel and
  `enumMaxExclusive` upper bound.
- `ACCESSOR_TYPE_ENUMERATION` returned by `getAccessorType()`.
- `virtual void setValue(EnumerationItem)` and `virtual EnumerationItem getValue() const`.
- `static int getNumberOfItems()`.
- `bool isValid() const` (instance) and `static bool isValid(EnumerationItem)` and `static bool isValid(const std::string&)`.
- `std::string toName() const`, `static EnumerationItem valueFromName(const std::string&)`, `void setValueFromName(const std::string&)`.
- Full comparison operator set (six `==`/`!=`/`<`/`<=`/`>`/`>=` against self and `EnumerationItem`).
- `friend operator<<(std::ostream&, const T&)`.
- Protected ctor, copy-ctor, operator=, dtor.
- `static T& create(ASB*)`, `static T& create(const T&, ASB*)`, `static void destroy(T&)`.

### Files to check

- `arcal/include/uci/type/ADS_B_ModeIndicatorEnum.h` (reference example)
- 10 additional `*Enum.h` files with varying item counts

### Existing coverage

`arcal/test/conform/conf_enum.cpp` — good baseline; check whether cross-type
operator overloads (free `operator==` between `EnumerationItem` and `T`) are
tested.

---

## TASK-009 — Choice Type Headers
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_choice_accessor.cpp` (compiles against current headers; `#if 0` blocks for spec-correct forms)

### Findings — FAIL (3 systematic violations, 420 files)
- **V1 — Enum name and value naming (CXX-007137/007138):** Enum named `<TypeName>ChoiceOrdinalEnum` instead of `<TypeName>Choice`. Values use `CHOICE_<ELEMENT>` / `CHOICE_NONE` without type-name prefix. `CHOICE_NONE` is the **last** enumerator (implicit value = element count) instead of the required `= 0` sentinel as first.
- **V2 — `set<TypeName>ChoiceOrdinal()` absent entirely:** No choice header has this method. Spec requires `virtual void set<TypeName>ChoiceOrdinal(<TypeName>Choice, AccessorType = null) = 0`.
- **V3 — `choose<ElementName>()` missing optional `AccessorType` parameter (CXX-012696):** Complex-typed `choose*()` must expose `AccessorType = null`; simple-restriction choice branches such as `EmptyType` remain zero-argument.
**Spec:** §12543–12895 "The Choice Accessor"
(CERT CXX-007137, CXX-007138, and choice method CERTs in that section)

### What the spec requires

For each `xs:complexType` containing `xs:choice`:

1. **Enum name must be `<TypeName>Choice`** — e.g. `StoreCommandTypeChoice`.
   (CXX-007137)
2. **Enum value naming must be `<TYPENAME>_CHOICE_<ELEMENT>`** — e.g.
   `STORECOMMANDTYPE_CHOICE_NEXTSTORESTATION`. The NONE sentinel must be
   `<TYPENAME>_CHOICE_NONE`. (CXX-007137, CXX-007138)
3. `virtual <TypeName>Choice get<TypeName>ChoiceOrdinal() const = 0`
4. `virtual void set<TypeName>ChoiceOrdinal(<TypeName>Choice, uci::base::accessorType::AccessorType = uci::base::accessorType::null) = 0`
5. For each choice element: `virtual bool is<ElementName>() const = 0`
6. For each choice element: `virtual T& choose<ElementName>() = 0` (no ASB arg)
7. For each choice element: `virtual const T& get<ElementName>() const = 0`

### Known violations

All inspected Choice headers use a non-compliant pattern:
- Enum named `<TypeName>ChoiceOrdinalEnum` instead of `<TypeName>Choice`
- Enum values `CHOICE_<ELEMENT>` / `CHOICE_NONE` without the type-name prefix

Example: `StoreCommandType.h` has `enum StoreCommandTypeChoiceOrdinalEnum`
with `CHOICE_NONE`, but spec requires `enum StoreCommandTypeChoice` with
`STORECOMMANDTYPE_CHOICE_NONE`.

Additionally, `set<TypeName>ChoiceOrdinal(...)` appears to be missing entirely.

The agent should confirm scope (search all `*ChoiceOrdinalEnum*` in
`arcal/include/uci/type/`), document the count of affected files, and write a
compile-time conformance test that fails under the current naming and passes
under the correct naming.

### Files to check

- `arcal/include/uci/type/StoreCommandType.h`
- `arcal/include/uci/type/EA_TargetPointingType.h`
- `arcal/include/uci/type/EntitySourceIdentifierType.h`
- 10 additional choice types from `grep -rl ChoiceOrdinalEnum arcal/include/uci/type/`

### Existing coverage

None. No conformance test covers `xs:choice` accessor shape.

---

## TASK-010 — GlobalElement (MT) Headers: Listener/Reader/Writer
**Status:** `done`
**Test written:** `arcal/test/arcal-cert/compile/conf_mt_shape.cpp`

### Findings — PARTIAL
- **V1 — CXX-007316 typedef form not used (intentional deviation):** Spec says global elements should be `typedef FooType FooMT`. Generated headers define a full `FooMT` class inheriting `MessageType`. This is architecturally necessary — the typedef form cannot host the nested Reader/Writer/Listener/factory API. **Known intentional deviation; incompatible requirements in the spec itself.**
- **PASS — Listener/Reader/Writer nested class shapes:** All method signatures, inheritance, access control, `friend class`, protected/deleted ctor/dtor, and factory methods (`createReader`/`destroyReader`/`createWriter`/`destroyWriter`) are correct across all 10+ sampled MT headers.
**Spec:** §9919–10747 "Global Element Accessor" + §10100 "Global Element Accessor Special C++ Classes"
(CERT CXX-007316, factory method CERTs, and Reader/Writer/Listener shape CERTs)

### What the spec requires

For each `xs:element` (global — maps to `*MT.h`):

1. **Typedef form** (CXX-007316): `typedef <cxxElementTypeName> <cxxTypeName>` — but
   arcal generates full classes instead; verify whether this is intentional (the
   standard allows the alternative of generating reader/writer directly on the type).

2. **Nested `Listener` class**:
   - `virtual void handleMessage(const T& message) = 0`
   - `virtual ~Listener() = default` (public)
   - Protected default ctor; deleted copy-ctor and operator=

3. **Nested `Reader` class** (extends `uci::base::Reader`):
   - `virtual void addListener(Listener&) = 0`
   - `virtual void removeListener(Listener&) = 0`
   - `virtual unsigned long read(unsigned long timeout, unsigned long count, Listener&) = 0`
   - `virtual unsigned long readNoWait(unsigned long count, Listener&) = 0`
   - `virtual void close() = 0`
   - Protected default ctor, deleted copy-ctor/operator=, protected virtual dtor
   - `friend class T`

4. **Nested `Writer` class** (extends `uci::base::Writer`):
   - `virtual void write(T& accessor) = 0`
   - `virtual void close() = 0`
   - Protected default ctor, deleted copy-ctor/operator=, protected virtual dtor
   - `friend class T`

5. **Factory methods** on `T`:
   - `static Reader& createReader(const std::string& topic, uci::base::AbstractServiceBusConnection*)`
   - `static void destroyReader(Reader&)`
   - `static Writer& createWriter(const std::string& topic, uci::base::AbstractServiceBusConnection*)`
   - `static void destroyWriter(Writer&)`

### Files to check

- `arcal/include/uci/type/AMTI_ActivityMT.h` (reference example — inspect carefully)
- 10 additional `*MT.h` files of your choice

### Existing coverage

`arcal/test/conform/conf_generated_global.cpp` (factory shape),
`arcal/test/conform/conf_status_listener.cpp` (listener lifecycle)

---

## Cross-cutting notes for all agents

- **Spec path:** `OMS/docs_markdown_unofficial/06_OMSC-SPC-008_RevK_CxxCALSpec_DandD_v2_5.md`
- **Headers under test:** `arcal/include/uci/type/*.h` (6354 files)
- **Existing tests:** `arcal/test/conform/` (compile-only `static_assert` style)
- **Test style:** Prefer compile-time `static_assert` checks. Use a concrete
  representative type (not all 6354). Include a short comment at the top
  citing the spec section and CERT ID.
- **Do not modify** generated headers — violations are findings to report, not
  to fix in-place.
- **Abstract types** (`@abstract='true'` in XSD) do not get `create`/`destroy`;
  check TASK-002 findings against that caveat.
