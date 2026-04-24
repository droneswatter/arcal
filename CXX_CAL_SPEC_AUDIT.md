# ARCAL C++ CAL Spec Audit

This audit tracks ARCAL conformance to OMSC-SPC-008 RevK, the C++ CAL
specification. Meeting this public API shape is P0: ARCAL's generated
`uci::type::*` classes must behave like CAL accessor interfaces, not ordinary
C++ value objects.

The key design decision is:

- `uci::type::*` classes are public CxxCAL accessor interfaces.
- ARCAL-generated implementation classes own storage and implement those
  interfaces.
- Application code constructs accessors through generated factory methods.
- Application code does not stack-construct, copy-construct, assign, or delete
  generated accessors directly.

This is intentional in the spec. It lets CAL implementations choose allocation
and storage strategies such as pools, arenas, shared memory, transport loaning,
or other implementation-specific memory management.

## Spec Anchors

- Section 7, Accessor Construction and Destruction:
  generated accessors use factory methods; default constructors, copy
  constructors, and assignment operators are protected to prevent direct
  application construction.
- Section 10.2.1:
  generated accessor destructors, default constructors, and copy constructors
  are protected.
- Section 10.2.2:
  generated complex accessors provide public `copy(const T&)`; copy assignment
  remains protected.
- Section 10.2.3.18 and 10.2.3.19:
  non-abstract complex accessors provide public static `create(...)`,
  copy-create, and `destroy(...)`.
- Section 10.3:
  global element accessors additionally provide nested Reader/Writer classes
  and `createReader`, `destroyReader`, `createWriter`, and `destroyWriter`.

## Current Gap Summary

Current generated headers are concrete value classes:

- public default constructors
- public copy constructors
- public copy assignment
- public destructors
- direct data members in the `uci::type::*` public class
- no generated accessor `create(...)`, copy-create, or `destroy(...)`
- no generated `copy(const T&)`

That shape is useful for early implementation but is not CxxCAL-conformant.

## Schema Facts That Affect The Design

The UCI 2.5 schema includes:

- 70 abstract complex types.
- 83 fields or choice variants typed as abstract complex bases.
- 25 abstract-base list fields.

That means the interface/implementation split must support polymorphic storage
from the start. A storage model that only embeds concrete `ChildImpl` members is
not sufficient for all schema shapes.

## Target Architecture

Generate two layers.

Public interface:

```cpp
namespace uci::type {

class ActionCommandMT : public MessageType {
public:
    virtual ActionCommandMDT& getMessageData() = 0;
    virtual const ActionCommandMDT& getMessageData() const = 0;
    virtual void setMessageData(const ActionCommandMDT& value) = 0;

    virtual void copy(const ActionCommandMT& rhs) = 0;

    static ActionCommandMT& create(
        uci::base::AbstractServiceBusConnection* asb = nullptr);
    static ActionCommandMT& create(
        const ActionCommandMT& rhs,
        uci::base::AbstractServiceBusConnection* asb = nullptr);
    static void destroy(ActionCommandMT& accessor);

protected:
    ActionCommandMT() = default;
    ActionCommandMT(const ActionCommandMT&) = default;
    ActionCommandMT& operator=(const ActionCommandMT&) = default;
    ~ActionCommandMT() override = default;
};

} // namespace uci::type
```

ARCAL implementation:

```cpp
namespace arcal::type {

class ActionCommandMTImpl final : public uci::type::ActionCommandMT {
public:
    ActionCommandMTImpl() = default;
    explicit ActionCommandMTImpl(const uci::type::ActionCommandMT& rhs) {
        copy(rhs);
    }

    uci::type::ActionCommandMDT& getMessageData() override {
        return messageData_;
    }

    const uci::type::ActionCommandMDT& getMessageData() const override {
        return messageData_;
    }

    void setMessageData(const uci::type::ActionCommandMDT& value) override {
        messageData_.copy(value);
    }

    void copy(const uci::type::ActionCommandMT& rhs) override {
        setMessageData(rhs.getMessageData());
    }

private:
    ActionCommandMDTImpl messageData_;
};

} // namespace arcal::type
```

The exact implementation namespace and file layout can evolve, but the public
`uci::type` namespace must remain spec-shaped.

## P0 Work Items

### 1. Model Abstract Types

Parse and preserve `abstract="true"` on XSD complex types.

Required decisions:

- Abstract types do not get public `create(...)` / `destroy(...)` factories.
- Abstract public interfaces may still define fields and virtual accessors.
- Concrete derived implementation classes must be selectable when a field,
  list, or choice is typed as an abstract base.

### 2. Split Generated Public Interfaces From Implementations

Generate public headers under:

```text
include/uci/type/
```

Generate ARCAL implementation headers/sources under an implementation-owned
path, likely:

```text
include/arcal/type/
```

or, if complete implementation types are only needed inside ARCAL:

```text
src/generated/type/
```

Preferred starting point: `include/arcal/type/` so generated factories and
templated internals can include complete implementation types without fighting
visibility.

### 3. Generated Lifecycle And Factory API

For each non-abstract generated complex accessor:

- protected default constructor
- protected copy constructor
- protected copy assignment
- protected virtual destructor
- public `copy(const T&)`
- public static `create(AbstractServiceBusConnection* = nullptr)`
- public static `create(const T&, AbstractServiceBusConnection* = nullptr)`
- public static `destroy(T&)`

For simple restrictions and enumerations, evaluate the same lifecycle rules
against their specific spec sections and generated usage.

### 4. Field Storage Strategy

Implementation classes own the data members.

Concrete complex field:

- public interface returns `uci::type::Child&`
- implementation stores `arcal::type::ChildImpl`

Primitive/string/enum field:

- implementation stores the concrete backing value/accessor implementation
  required by that accessor kind.

Optional field:

- implementation stores presence flag plus concrete implementation storage
  where the schema type is concrete.

Choice field:

- implementation stores selected ordinal plus storage for each concrete choice
  variant, or a polymorphic owned accessor for abstract variants.

List field:

- concrete element lists may store concrete implementation objects while
  exposing interface references.
- abstract/polymorphic element lists need owned polymorphic accessors and a CAL
  list wrapper that supports implementation-selected derived types.

The list question is first-order because the schema contains abstract-base
lists such as `QueryResultType.Message` (`MessageType`, unbounded) and
`QueryType.And` / `QueryType.Or` (`QueryPET`, unbounded).

### 5. Update CDR And JSON Generation

Generated serializers should operate on public interfaces where possible.

Generated deserializers must instantiate implementation classes rather than
public interface classes. Current stack temporaries such as:

```cpp
ActionCommandMT msg;
```

must become factory-created accessors or implementation objects.

### 6. Update Tests And Examples

Known current stack-construction call sites include:

- `README.md`
- `test/json/*`
- `test/conform/conform_client_link.cpp`
- `test/install/consumer/main.cpp`
- `test/arcal-cert/cert/*`
- `test/arcal-cert/e2e/*`

Application-facing tests should migrate to:

```cpp
auto& msg = uci::type::ActionCommandMT::create();
// populate/use msg
uci::type::ActionCommandMT::destroy(msg);
```

Implementation-only tests may use ARCAL implementation classes directly when
they are explicitly testing implementation behavior.

## First Conformance Tests To Add

Start with representative generated lifecycle tests:

```cpp
using Msg = uci::type::ActionCommandMT;
static_assert(!std::is_constructible_v<Msg>);
static_assert(!std::is_copy_constructible_v<Msg>);
static_assert(!std::is_assignable_v<Msg&, const Msg&>);
static_assert(!std::is_destructible_v<Msg>);
```

Then assert the factory/copy surface:

```cpp
using CreateFn = Msg& (*)(uci::base::AbstractServiceBusConnection*);
using CopyCreateFn = Msg& (*)(const Msg&, uci::base::AbstractServiceBusConnection*);
using DestroyFn = void (*)(Msg&);
using CopyFn = void (Msg::*)(const Msg&);
```

These tests intentionally fail against the current generator and should land
with the generator migration that makes them pass.

## Open Questions

- What exact namespace should hold generated implementation classes?
- Should implementation headers be installed as supported ARCAL internals or
  treated as private build artifacts?
- How should polymorphic abstract-base fields and lists select concrete derived
  implementations?
- How much of `arcal-cert` should remain portable once generated accessor
  lifecycle is spec-shaped, and how much belongs in ARCAL-specific tests?
