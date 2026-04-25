#!/usr/bin/env python3
"""
arcal Schema Compiler — XSD → C++ Accessor headers.

Usage:
    python compiler.py --schema <xsd_dir> --out <include_dir> [--ns-map <file>]

Reads all .xsd files in <xsd_dir>, generates one header per XSD global element
and complex/simple type into <include_dir>/uci/type/.

Namespace mapping (OMSC-SPC-008 §4, extensible without code modification):
    https://www.vdl.afrl.af.mil/programs/oam  →  uci
    http://www.w3.org/2001/XMLSchema           →  xs
Additional mappings can be supplied via --ns-map (JSON file: {"uri": "cxx_ns"}).
"""

import argparse
import json
import os
import sys
from pathlib import Path

try:
    from lxml import etree
    from jinja2 import Environment, FileSystemLoader
except ImportError:
    print("ERROR: Install dependencies: pip install lxml jinja2", file=sys.stderr)
    sys.exit(1)

# ---------------------------------------------------------------------------
# Default namespace → C++ namespace mapping (OMSC-SPC-008 Table 4.0-1)
# This table must not be overridden by external mappings.
# ---------------------------------------------------------------------------
PROTECTED_NS_MAP = {
    "https://www.vdl.afrl.af.mil/programs/oam": "uci",
    "http://www.w3.org/2001/XMLSchema": "xs",
}

XSD_NS = "http://www.w3.org/2001/XMLSchema"
OMS_NS = "https://www.vdl.afrl.af.mil/programs/oam"

# ---------------------------------------------------------------------------
# XSD parsing helpers
# ---------------------------------------------------------------------------

def load_ns_map(path: Path) -> dict:
    with open(path) as f:
        user_map = json.load(f)
    for key in PROTECTED_NS_MAP:
        if key in user_map:
            print(f"WARNING: ignoring attempt to override protected namespace '{key}'", file=sys.stderr)
            del user_map[key]
    return {**PROTECTED_NS_MAP, **user_map}


def xsd_name_to_cxx(name: str) -> str:
    """Convert XSD type name to CamelCase C++ class name."""
    if not name:
        return name
    # Remove any namespace prefix
    if ":" in name:
        name = name.split(":")[-1]
    return name[0].upper() + name[1:]


def is_optional(element) -> bool:
    return element.get("minOccurs", "1") == "0"


def max_occurs(element) -> str:
    return element.get("maxOccurs", "1")


def is_bounded_list(element) -> bool:
    mo = max_occurs(element)
    return mo != "1" and mo != "unbounded"


def is_unbounded_list(element) -> bool:
    return max_occurs(element) == "unbounded"


def is_list(element) -> bool:
    return is_bounded_list(element) or is_unbounded_list(element)


# ---------------------------------------------------------------------------
# Schema model classes
# ---------------------------------------------------------------------------

PRIMITIVE_ACCESSOR_TYPE_MAP: dict[str, str] = {
    "boolean":      "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "byte":         "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "short":        "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "int":          "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "long":         "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "integer":      "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "decimal":      "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "float":        "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "double":       "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "unsignedByte": "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "unsignedShort":"uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "unsignedInt":  "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "unsignedLong": "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "base64Binary": "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "hexBinary":    "uci::base::accessorType::ACCESSOR_TYPE_SIMPLE_PRIMITIVE",
    "string":       "uci::base::accessorType::ACCESSOR_TYPE_STRING",
    "anyURI":       "uci::base::accessorType::ACCESSOR_TYPE_STRING",
    "dateTime":     "uci::base::accessorType::ACCESSOR_TYPE_STRING",
    "dateTimeStamp":"uci::base::accessorType::ACCESSOR_TYPE_STRING",
    "date":         "uci::base::accessorType::ACCESSOR_TYPE_STRING",
    "time":         "uci::base::accessorType::ACCESSOR_TYPE_STRING",
    "duration":     "uci::base::accessorType::ACCESSOR_TYPE_STRING",
    "ID":           "uci::base::accessorType::ACCESSOR_TYPE_STRING",
    "IDREF":        "uci::base::accessorType::ACCESSOR_TYPE_STRING",
    "NMTOKEN":      "uci::base::accessorType::ACCESSOR_TYPE_STRING",
}


class FieldModel:
    def __init__(self, name, type_name, optional=False, list_kind=None,
                 min_occurs=0, max_occurs_val=1, resolved_cxx_type=None):
        self.name = name
        self.cxx_name = xsd_name_to_cxx(name)
        self.type_name = type_name
        # Use C++ primitive directly if available, else generate class name
        self.cxx_type = resolved_cxx_type if resolved_cxx_type else xsd_name_to_cxx(type_name)
        self.optional = optional
        self.list_kind = list_kind  # None, "bounded", "unbounded"
        self.min_occurs = min_occurs
        self.max_occurs_val = max_occurs_val
        self.accessor_type = "uci::base::accessorType::ACCESSOR_TYPE_COMPLEX"  # resolved later
        self.type_is_abstract = False
        self.type_is_generated = False
        self.storage_cxx_type = self.cxx_type


class ChoiceModel:
    def __init__(self, variants):
        self.variants = variants  # list of FieldModel


class TypeModel:
    def __init__(self, name, fields=None, choices=None, base_type=None,
                 enum_values=None, is_string_restriction=False,
                 is_abstract=False):
        self.name = name
        self.cxx_name = xsd_name_to_cxx(name)
        self.fields = fields or []
        self.choices = choices or []
        self.base_type = base_type
        self.enum_values = enum_values
        self.is_enum = enum_values is not None
        self.is_string_restriction = is_string_restriction
        self.is_abstract = is_abstract
        self.derived_types: list[str] = []


class GlobalElementModel:
    def __init__(self, name, type_name):
        self.name = name
        self.cxx_name = xsd_name_to_cxx(name)
        self.type_name = type_name
        self.cxx_type = xsd_name_to_cxx(type_name)


# ---------------------------------------------------------------------------
# XSD parser
# ---------------------------------------------------------------------------

class SchemaParser:
    def __init__(self, schema_dir: Path):
        self.schema_dir = schema_dir
        self.types: dict[str, TypeModel] = {}
        self.global_elements: list[GlobalElementModel] = []
        self._trees: list[etree._ElementTree] = []

    def parse_all(self):
        for xsd_file in sorted(self.schema_dir.glob("*.xsd")):
            tree = etree.parse(str(xsd_file))
            self._trees.append(tree)

        for tree in self._trees:
            root = tree.getroot()
            self._parse_schema(root)

        self._resolve_accessor_types()
        self._resolve_inheritance_metadata()

    def _get_accessor_type(self, type_name: str) -> str:
        if type_name in PRIMITIVE_ACCESSOR_TYPE_MAP:
            return PRIMITIVE_ACCESSOR_TYPE_MAP[type_name]
        if type_name in self.types:
            t = self.types[type_name]
            if t.is_enum:
                return "uci::base::accessorType::ACCESSOR_TYPE_ENUMERATION"
            if t.is_string_restriction:
                return "uci::base::accessorType::ACCESSOR_TYPE_STRING"
        return "uci::base::accessorType::ACCESSOR_TYPE_COMPLEX"

    def _resolve_accessor_types(self):
        for type_model in self.types.values():
            for field in type_model.fields:
                field.accessor_type = self._get_accessor_type(field.type_name)
                field.type_is_generated = field.type_name in self.types
                if field.type_is_generated:
                    field.storage_cxx_type = f"{field.cxx_type}Impl"
            for choice in type_model.choices:
                for v in choice.variants:
                    v.accessor_type = self._get_accessor_type(v.type_name)
                    v.type_is_generated = v.type_name in self.types
                    if v.type_is_generated:
                        v.storage_cxx_type = f"{v.cxx_type}Impl"

    def _resolve_inheritance_metadata(self):
        for type_model in self.types.values():
            if type_model.base_type in self.types:
                self.types[type_model.base_type].derived_types.append(type_model.name)

        for type_model in self.types.values():
            for field in type_model.fields:
                field.type_is_abstract = self.types.get(field.type_name, TypeModel("")).is_abstract
            for choice in type_model.choices:
                for v in choice.variants:
                    v.type_is_abstract = self.types.get(v.type_name, TypeModel("")).is_abstract

    def _parse_schema(self, root):
        ns = {"xs": XSD_NS}

        # Global elements
        for elem in root.findall("xs:element", ns):
            name = elem.get("name")
            type_name = elem.get("type", "").split(":")[-1]
            if name:
                self.global_elements.append(GlobalElementModel(name, type_name))

        # Complex types
        for ct in root.findall("xs:complexType", ns):
            model = self._parse_complex_type(ct, ns)
            if model:
                self.types[model.name] = model

        # Simple types (enumerations)
        for st in root.findall("xs:simpleType", ns):
            model = self._parse_simple_type(st, ns)
            if model:
                self.types[model.name] = model

    def _parse_complex_type(self, ct, ns) -> TypeModel | None:
        name = ct.get("name")
        if not name:
            return None

        fields = []
        choices = []
        base_type = None

        # Extension / restriction
        for content_tag in ["xs:complexContent", "xs:simpleContent"]:
            content = ct.find(content_tag, ns)
            if content is not None:
                for ext in content.findall("xs:extension", ns):
                    base_type = ext.get("base", "").split(":")[-1]
                    for seq in ext.findall("xs:sequence", ns):
                        fields.extend(self._parse_sequence(seq, ns))
                    for ch in ext.findall("xs:choice", ns):
                        choices.append(self._parse_choice(ch, ns))

        # Direct sequence
        for seq in ct.findall("xs:sequence", ns):
            fields.extend(self._parse_sequence(seq, ns))

        # Direct choice
        for ch in ct.findall("xs:choice", ns):
            choices.append(self._parse_choice(ch, ns))

        return TypeModel(name, fields=fields, choices=choices, base_type=base_type,
                         is_abstract=ct.get("abstract") == "true")

    def _parse_sequence(self, seq, ns) -> list:
        fields = []
        for elem in seq.findall("xs:element", ns):
            field = self._element_to_field(elem)
            if field:
                fields.append(field)
        return fields

    def _parse_choice(self, choice, ns) -> ChoiceModel:
        variants = []
        for elem in choice.findall("xs:element", ns):
            field = self._element_to_field(elem)
            if field:
                variants.append(field)
        return ChoiceModel(variants)

    def _element_to_field(self, elem) -> FieldModel | None:
        name = elem.get("name") or elem.get("ref", "").split(":")[-1]
        if not name:
            return None
        # Skip anonymous inline types and untyped placeholder elements
        if elem.get("type") is None and elem.get("ref") is None:
            return None
        type_name = elem.get("type", name).split(":")[-1]

        min_occ = int(elem.get("minOccurs", "1"))
        max_occ_str = elem.get("maxOccurs", "1")
        max_occ = -1 if max_occ_str == "unbounded" else int(max_occ_str)

        optional  = min_occ == 0 and max_occ == 1
        list_kind = None
        if max_occ == -1:
            list_kind = "unbounded"
        elif max_occ > 1:
            list_kind = "bounded"

        # Map XSD primitives to C++ built-in types directly
        resolved_type = CXX_PRIMITIVE_MAP.get(type_name, None)
        return FieldModel(name, type_name, optional=optional,
                          list_kind=list_kind, min_occurs=min_occ,
                          max_occurs_val=max_occ,
                          resolved_cxx_type=resolved_type)

    def _parse_simple_type(self, st, ns) -> TypeModel | None:
        name = st.get("name")
        if not name:
            return None

        restriction = st.find("xs:restriction", ns)
        if restriction is None:
            return None

        enum_values = []
        for enum in restriction.findall("xs:enumeration", ns):
            enum_values.append(enum.get("value"))

        if enum_values:
            return TypeModel(name, enum_values=enum_values)

        # Non-enumeration restriction (length/pattern facets): wrap the base type
        base = restriction.get("base", "xs:string").split(":")[-1]
        resolved = CXX_PRIMITIVE_MAP.get(base, "std::string")
        return TypeModel(name, base_type=resolved, fields=[], is_string_restriction=True)


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------

ACCESSOR_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/base/Accessor.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/BoundedList.h"
#include "uci/base/SimpleList.h"
{% if type.base_type and type.base_type not in primitive_types %}\
#include "uci/type/{{ type.base_type }}.h"
{% endif %}\
{% if global_element %}\
#include "uci/base/Listener.h"
#include "uci/base/Reader.h"
#include "uci/base/Writer.h"
{% endif %}\
{% for field in type.fields %}\
{% if field.type_name not in primitive_types %}\
#include "uci/type/{{ field.cxx_type }}.h"
{% endif %}\
{% endfor %}\
{% for choice in type.choices %}\
{% for v in choice.variants %}\
{% if v.type_name not in primitive_types %}\
#include "uci/type/{{ v.cxx_type }}.h"
{% endif %}\
{% endfor %}\
{% endfor %}\
#include <cstdint>
#include <string>

namespace uci {
namespace type {

{% if type.base_type and type.base_type not in primitive_types %}\
class {{ type.cxx_name }} : public virtual uci::type::{{ type.base_type }} {
{% else %}\
class {{ type.cxx_name }} : public virtual uci::base::Accessor {
{% endif %}\
public:
    AccessorType getAccessorType() const noexcept override { return ACCESSOR_TYPE_COMPLEX; }
    const std::string& typeName() const override {
        static const std::string kName{"{{ type.name }}"};
        return kName;
    }
    virtual void copy(const {{ type.cxx_name }}& rhs) = 0;
    static {{ type.cxx_name }}& create(uci::base::AbstractServiceBusConnection* asb);
    static {{ type.cxx_name }}& create(const {{ type.cxx_name }}& rhs,
                                       uci::base::AbstractServiceBusConnection* asb);
    static void destroy({{ type.cxx_name }}& accessor);
{% if global_element %}\

    class Listener : public uci::base::Listener {
    public:
        virtual void handleMessage(const {{ type.cxx_name }}& message) = 0;
        virtual ~Listener() = default;
    protected:
        Listener() = default;
        Listener(const Listener&) = delete;
        Listener& operator=(const Listener&) = delete;
    };

    class Reader : public uci::base::Reader {
    public:
        virtual void addListener(Listener& listener) = 0;
        virtual void removeListener(Listener& listener) = 0;
        virtual unsigned long read(unsigned long timeout, unsigned long numberOfMessages, Listener& listener) = 0;
        virtual unsigned long readNoWait(unsigned long numberOfMessages, Listener& listener) = 0;
        virtual void close() = 0;
        friend class {{ type.cxx_name }};
    protected:
        Reader() = default;
        Reader(const Reader&) = delete;
        Reader& operator=(const Reader&) = delete;
        virtual ~Reader() = default;
    };

    class Writer : public uci::base::Writer {
    public:
        virtual void write({{ type.cxx_name }}& accessor) = 0;
        virtual void close() = 0;
        friend class {{ type.cxx_name }};
    protected:
        Writer() = default;
        Writer(const Writer&) = delete;
        Writer& operator=(const Writer&) = delete;
        virtual ~Writer() = default;
    };

    static Reader& createReader(const std::string& topic,
                                uci::base::AbstractServiceBusConnection* asb);
    static void    destroyReader(Reader& reader);
    static Writer& createWriter(const std::string& topic,
                                uci::base::AbstractServiceBusConnection* asb);
    static void    destroyWriter(Writer& writer);
{% endif %}\

{% for field in type.fields %}\
{% if field.list_kind == "bounded" %}\
    virtual uci::base::BoundedList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() = 0;
    virtual const uci::base::BoundedList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() const = 0;
{% elif field.list_kind == "unbounded" %}\
    virtual uci::base::SimpleList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() = 0;
    virtual const uci::base::SimpleList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() const = 0;
{% elif field.optional %}\
    virtual bool has{{ field.cxx_name }}() const = 0;
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& enable{{ field.cxx_name }}() = 0;
    virtual void clear{{ field.cxx_name }}() = 0;
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() = 0;
    virtual const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const = 0;
{% else %}\
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() = 0;
    virtual const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const = 0;
    virtual void set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) = 0;
{% endif %}\
{% endfor %}\

{% for choice in type.choices %}\
    enum {{ type.cxx_name }}ChoiceOrdinalEnum {
{% for v in choice.variants %}\
        CHOICE_{{ v.cxx_name | upper }},
{% endfor %}\
        CHOICE_NONE
    };
    virtual {{ type.cxx_name }}ChoiceOrdinalEnum get{{ type.cxx_name }}ChoiceOrdinal() const = 0;
{% for v in choice.variants %}\
    virtual bool is{{ v.cxx_name }}() const = 0;
    virtual {{ v.cxx_type | qualify(primitive_types, v.type_name) }}& choose{{ v.cxx_name }}() = 0;
    virtual const {{ v.cxx_type | qualify(primitive_types, v.type_name) }}& get{{ v.cxx_name }}() const = 0;
{% endfor %}\
{% endfor %}\

    static std::string getUCITypeVersion() { return "{{ uci_version }}"; }

protected:
    {{ type.cxx_name }}() = default;
    {{ type.cxx_name }}(const {{ type.cxx_name }}&) = default;
    {{ type.cxx_name }}& operator=(const {{ type.cxx_name }}&) = default;
    ~{{ type.cxx_name }}() override = default;
};

} // namespace type
} // namespace uci
"""

ACCESSOR_IMPL_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/type/{{ type.cxx_name }}.h"
#include "arcal/TypedAccessor.h"
#include "uci/base/BoundedListImpl.h"
#include "uci/base/SimpleListImpl.h"
{% if type.base_type and type.base_type not in primitive_types %}\
#include "generated/type_impl/{{ type.base_type }}Impl.h"
{% endif %}\
{% for field in type.fields %}\
{% if field.type_is_generated %}\
#include "generated/type_impl/{{ field.cxx_type }}Impl.h"
{% endif %}\
{% endfor %}\
{% for choice in type.choices %}\
{% for v in choice.variants %}\
{% if v.type_is_generated %}\
#include "generated/type_impl/{{ v.cxx_type }}Impl.h"
{% endif %}\
{% endfor %}\
{% endfor %}\

namespace arcal {
namespace type {

{% if type.base_type and type.base_type not in primitive_types %}\
class {{ type.cxx_name }}Impl : public virtual uci::type::{{ type.cxx_name }}, public arcal::type::{{ type.base_type }}Impl, public virtual arcal::type::TypedAccessor {
{% else %}\
class {{ type.cxx_name }}Impl : public virtual uci::type::{{ type.cxx_name }}, public virtual arcal::type::TypedAccessor {
{% endif %}\
public:
    using UciType = uci::type::{{ type.cxx_name }};
    static constexpr uint32_t TYPE_TAG = {{ type_tag }}u;

    {{ type.cxx_name }}Impl() = default;
    {{ type.cxx_name }}Impl(const {{ type.cxx_name }}Impl&) = default;
    {{ type.cxx_name }}Impl& operator=(const {{ type.cxx_name }}Impl&) = default;
    ~{{ type.cxx_name }}Impl() override = default;

    uint32_t typeTag() const noexcept override { return TYPE_TAG; }
    void reset() override { *this = {{ type.cxx_name }}Impl{}; }
    void copy(const UciType& rhs) override {
{% if type.base_type and type.base_type not in primitive_types %}\
        arcal::type::{{ type.base_type }}Impl::copy(rhs);
{% endif %}\
{% for field in type.fields %}\
{% if field.list_kind %}\
        {{ field.name }}_.clear();
        for (const auto& item : rhs.get{{ field.cxx_name }}()) {
            {{ field.name }}_.push_back(item);
        }
{% elif field.optional %}\
        has{{ field.cxx_name }}_ = rhs.has{{ field.cxx_name }}();
        if (has{{ field.cxx_name }}_) {
{% if field.type_is_generated %}\
            {{ field.name }}_.copy(rhs.get{{ field.cxx_name }}());
{% else %}\
            {{ field.name }}_ = rhs.get{{ field.cxx_name }}();
{% endif %}\
        }
{% else %}\
{% if field.type_is_generated %}\
        {{ field.name }}_.copy(rhs.get{{ field.cxx_name }}());
{% else %}\
        {{ field.name }}_ = rhs.get{{ field.cxx_name }}();
{% endif %}\
{% endif %}\
{% endfor %}\
{% for choice in type.choices %}\
        choiceOrdinal_ = rhs.get{{ type.cxx_name }}ChoiceOrdinal();
{% for v in choice.variants %}\
        if (rhs.is{{ v.cxx_name }}()) {
{% if v.type_is_generated %}\
            choice{{ v.cxx_name }}_.copy(rhs.get{{ v.cxx_name }}());
{% else %}\
            choice{{ v.cxx_name }}_ = rhs.get{{ v.cxx_name }}();
{% endif %}\
        }
{% endfor %}\
{% endfor %}\
    }

{% for field in type.fields %}\
{% if field.list_kind == "bounded" %}\
    uci::base::BoundedList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    const uci::base::BoundedList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
{% elif field.list_kind == "unbounded" %}\
    uci::base::SimpleList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    const uci::base::SimpleList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
{% elif field.optional %}\
    bool has{{ field.cxx_name }}() const override { return has{{ field.cxx_name }}_; }
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& enable{{ field.cxx_name }}() override { has{{ field.cxx_name }}_ = true; return {{ field.name }}_; }
    void clear{{ field.cxx_name }}() override { has{{ field.cxx_name }}_ = false; }
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
{% else %}\
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
{% if field.type_is_generated %}\
    void set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) override { {{ field.name }}_.copy(v); }
{% else %}\
    void set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) override { {{ field.name }}_ = v; }
{% endif %}\
{% endif %}\
{% endfor %}\

{% for choice in type.choices %}\
    UciType::{{ type.cxx_name }}ChoiceOrdinalEnum get{{ type.cxx_name }}ChoiceOrdinal() const override { return choiceOrdinal_; }
{% for v in choice.variants %}\
    bool is{{ v.cxx_name }}() const override { return choiceOrdinal_ == UciType::CHOICE_{{ v.cxx_name | upper }}; }
    {{ v.cxx_type | qualify(primitive_types, v.type_name) }}& choose{{ v.cxx_name }}() override { choiceOrdinal_ = UciType::CHOICE_{{ v.cxx_name | upper }}; return choice{{ v.cxx_name }}_; }
    const {{ v.cxx_type | qualify(primitive_types, v.type_name) }}& get{{ v.cxx_name }}() const override { return choice{{ v.cxx_name }}_; }
{% endfor %}\
{% endfor %}\

private:
{% for field in type.fields %}\
{% if field.list_kind == "bounded" %}\
    uci::base::BoundedListImpl<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}, {{ field.min_occurs }}, {{ field.max_occurs_val }}, {{ field | storage_qualify(primitive_types) }}> {{ field.name }}_;
{% elif field.list_kind == "unbounded" %}\
    uci::base::SimpleListImpl<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}, {{ field | storage_qualify(primitive_types) }}> {{ field.name }}_;
{% elif field.optional %}\
    bool has{{ field.cxx_name }}_{false};
    {{ field | storage_qualify(primitive_types) }} {{ field.name }}_;
{% else %}\
    {{ field | storage_qualify(primitive_types) }} {{ field.name }}_;
{% endif %}\
{% endfor %}\
{% for choice in type.choices %}\
    UciType::{{ type.cxx_name }}ChoiceOrdinalEnum choiceOrdinal_{ UciType::CHOICE_NONE };
{% for v in choice.variants %}\
    {{ v | storage_qualify(primitive_types) }} choice{{ v.cxx_name }}_;
{% endfor %}\
{% endfor %}\
};

} // namespace type
} // namespace arcal
"""

ENUM_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/Accessor.h"
#include <ostream>
#include <stdexcept>
#include <string>

namespace uci {
namespace type {

class {{ type.cxx_name }} : public virtual uci::base::Accessor {
public:
    enum EnumerationItem {
        enumNotSet = 0,
{% for v in type.enum_values %}\
        {{ v | sanitize_enum }},
{% endfor %}\
        enumMaxExclusive,
    };

    AccessorType getAccessorType() const noexcept override { return ACCESSOR_TYPE_ENUMERATION; }
    const std::string& typeName() const override {
        static const std::string kName{"{{ type.name }}"};
        return kName;
    }

    virtual void copy(const {{ type.cxx_name }}& rhs) = 0;
    static {{ type.cxx_name }}& create(uci::base::AbstractServiceBusConnection* asb);
    static {{ type.cxx_name }}& create(const {{ type.cxx_name }}& rhs,
                                       uci::base::AbstractServiceBusConnection* asb);
    static void destroy({{ type.cxx_name }}& accessor);

    virtual void setValue(EnumerationItem v) = 0;
    virtual EnumerationItem getValue() const = 0;

    static int getNumberOfItems() { return {{ type.enum_values | length }}; }

    bool isValid() const { return isValid(getValue()); }
    static bool isValid(EnumerationItem v) { return v > enumNotSet && v < enumMaxExclusive; }
    static bool isValid(const std::string& name) {
        static const char* names[] = {
{% for v in type.enum_values %}\
            "{{ v }}",
{% endfor %}\
        };
        for (int i = 0; i < getNumberOfItems(); ++i) {
            if (name == names[i]) return true;
        }
        return false;
    }

    std::string toName() const {
        static const char* names[] = {
            "enumNotSet",
{% for v in type.enum_values %}\
            "{{ v }}",
{% endfor %}\
        };
        const auto value = getValue();
        if (value < 0 || value >= enumMaxExclusive) return "invalid";
        return names[static_cast<int>(value)];
    }

    static EnumerationItem valueFromName(const std::string& name) {
        static const char* names[] = {
            "enumNotSet",
{% for v in type.enum_values %}\
            "{{ v }}",
{% endfor %}\
        };
        constexpr int total = 1 + {{ type.enum_values | length }};
        for (int i = 0; i < total; ++i) {
            if (name == names[i]) {
                return static_cast<EnumerationItem>(i);
            }
        }
        throw std::invalid_argument("{{ type.cxx_name }}::fromName: unknown value: " + name);
    }

    void setValueFromName(const std::string& name) { setValue(valueFromName(name)); }

    bool operator==(const {{ type.cxx_name }}& rhs) const { return getValue() == rhs.getValue(); }
    bool operator!=(const {{ type.cxx_name }}& rhs) const { return getValue() != rhs.getValue(); }
    bool operator<(const {{ type.cxx_name }}& rhs)  const { return getValue() <  rhs.getValue(); }
    bool operator<=(const {{ type.cxx_name }}& rhs) const { return getValue() <= rhs.getValue(); }
    bool operator>(const {{ type.cxx_name }}& rhs)  const { return getValue() >  rhs.getValue(); }
    bool operator>=(const {{ type.cxx_name }}& rhs) const { return getValue() >= rhs.getValue(); }

    bool operator==(EnumerationItem rhs) const { return getValue() == rhs; }
    bool operator!=(EnumerationItem rhs) const { return getValue() != rhs; }

    friend std::ostream& operator<<(std::ostream& os, const {{ type.cxx_name }}& rhs) {
        return os << rhs.toName();
    }

protected:
    {{ type.cxx_name }}() = default;
    {{ type.cxx_name }}(const {{ type.cxx_name }}&) = default;
    {{ type.cxx_name }}& operator=(const {{ type.cxx_name }}&) = default;
    ~{{ type.cxx_name }}() override = default;
};

} // namespace type
} // namespace uci
"""

ENUM_IMPL_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/type/{{ type.cxx_name }}.h"
#include "arcal/TypedAccessor.h"

namespace arcal {
namespace type {

class {{ type.cxx_name }}Impl : public virtual uci::type::{{ type.cxx_name }}, public virtual arcal::type::TypedAccessor {
public:
    using UciType = uci::type::{{ type.cxx_name }};
    using EnumerationItem = UciType::EnumerationItem;
    static constexpr uint32_t TYPE_TAG = {{ type_tag }}u;

    {{ type.cxx_name }}Impl() = default;
    {{ type.cxx_name }}Impl(const {{ type.cxx_name }}Impl&) = default;
    {{ type.cxx_name }}Impl& operator=(const {{ type.cxx_name }}Impl&) = default;
    ~{{ type.cxx_name }}Impl() override = default;

    uint32_t typeTag() const noexcept override { return TYPE_TAG; }
    void reset() override { value_ = UciType::enumNotSet; }
    void copy(const UciType& rhs) override { value_ = rhs.getValue(); }
    void setValue(EnumerationItem v) override { value_ = v; }
    EnumerationItem getValue() const override { return value_; }

private:
    EnumerationItem value_{UciType::enumNotSet};
};

} // namespace type
} // namespace arcal
"""

STRING_RESTRICTION_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/Accessor.h"
#include <string>
{% if type.base_type == "std::vector<uint8_t>" %}\
#include <vector>
#include <cstdint>
{% elif type.base_type in ("int8_t","uint8_t","int16_t","uint16_t","int32_t","uint32_t","int64_t","uint64_t") %}\
#include <cstdint>
{% endif %}\

namespace uci {
namespace type {

class {{ type.cxx_name }} : public virtual uci::base::Accessor {
public:
    AccessorType getAccessorType() const noexcept override { return ACCESSOR_TYPE_SIMPLE_PRIMITIVE; }
    const std::string& typeName() const override {
        static const std::string kName{"{{ type.name }}"};
        return kName;
    }

    virtual void copy(const {{ type.cxx_name }}& rhs) = 0;
    static {{ type.cxx_name }}& create(uci::base::AbstractServiceBusConnection* asb);
    static {{ type.cxx_name }}& create(const {{ type.cxx_name }}& rhs,
                                       uci::base::AbstractServiceBusConnection* asb);
    static void destroy({{ type.cxx_name }}& accessor);

    virtual const {{ type.base_type }}& getValue() const = 0;
    virtual void setValue(const {{ type.base_type }}& v) = 0;
    {{ type.cxx_name }}& operator=(const {{ type.base_type }}& v) { setValue(v); return *this; }
    operator {{ type.base_type }}() const { return getValue(); }

    static std::string getUCITypeVersion() { return "{{ uci_version }}"; }

protected:
    {{ type.cxx_name }}() = default;
    {{ type.cxx_name }}(const {{ type.cxx_name }}&) = default;
    {{ type.cxx_name }}& operator=(const {{ type.cxx_name }}&) = default;
    ~{{ type.cxx_name }}() override = default;
};

} // namespace type
} // namespace uci
"""

STRING_RESTRICTION_IMPL_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/type/{{ type.cxx_name }}.h"
#include "arcal/TypedAccessor.h"

namespace arcal {
namespace type {

class {{ type.cxx_name }}Impl : public virtual uci::type::{{ type.cxx_name }}, public virtual arcal::type::TypedAccessor {
public:
    using UciType = uci::type::{{ type.cxx_name }};
    static constexpr uint32_t TYPE_TAG = {{ type_tag }}u;

    {{ type.cxx_name }}Impl() = default;
    {{ type.cxx_name }}Impl(const {{ type.cxx_name }}Impl&) = default;
    {{ type.cxx_name }}Impl& operator=(const {{ type.cxx_name }}Impl&) = default;
    ~{{ type.cxx_name }}Impl() override = default;

    uint32_t typeTag() const noexcept override { return TYPE_TAG; }
    void reset() override { value_ = {{ type.base_type }}{}; }
    void copy(const UciType& rhs) override { value_ = rhs.getValue(); }
    const {{ type.base_type }}& getValue() const override { return value_; }
    void setValue(const {{ type.base_type }}& v) override { value_ = v; }

private:
    {{ type.base_type }} value_{};
};

} // namespace type
} // namespace arcal
"""

GLOBAL_ELEMENT_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

// Listener, Reader, Writer, and factory methods are declared as nested classes
// and static members of {{ elem.cxx_type }} in the type header.
#include "uci/type/{{ elem.cxx_type }}.h"
"""

PRIMITIVE_TYPES = {
    "boolean", "byte", "short", "int", "long", "float", "double",
    "string", "anyURI", "dateTime", "dateTimeStamp", "unsignedByte",
    "unsignedShort", "unsignedInt", "unsignedLong", "integer", "decimal",
    "base64Binary", "hexBinary", "ID", "IDREF", "NMTOKEN", "time", "date", "duration",
}

CXX_PRIMITIVE_MAP = {
    "boolean": "bool",
    "byte": "int8_t", "short": "int16_t", "int": "int32_t", "long": "int64_t",
    "unsignedByte": "uint8_t", "unsignedShort": "uint16_t",
    "unsignedInt": "uint32_t", "unsignedLong": "uint64_t",
    "integer": "int64_t", "decimal": "double",
    "float": "float", "double": "double",
    "string": "std::string", "anyURI": "std::string",
    "dateTime": "std::string", "dateTimeStamp": "std::string",
    "date": "std::string", "time": "std::string", "duration": "std::string",
    "base64Binary": "std::vector<uint8_t>", "hexBinary": "std::vector<uint8_t>",
    "ID": "std::string", "IDREF": "std::string", "NMTOKEN": "std::string",
}


def _sanitize_enum(value: str) -> str:
    """Convert an XSD enumeration value to a valid C++ identifier."""
    result = value.upper()
    for ch, rep in [("-", "_"), (".", "_"), (" ", "_"), ("/", "_"), ("(", "_"), (")", "")]:
        result = result.replace(ch, rep)
    # C++ identifiers cannot start with a digit
    if result and result[0].isdigit():
        result = "E_" + result
    return result or "EMPTY"


def _qualify_filter(cxx_type: str, primitive_types: set, type_name: str) -> str:
    """Prefix uci::type:: on non-primitive types that need namespace qualification."""
    if type_name in primitive_types:
        return cxx_type  # already a C++ built-in (e.g., bool, int32_t, std::string)
    if "::" in cxx_type:
        return cxx_type  # already qualified
    return f"uci::type::{cxx_type}"


def _storage_qualify_filter(field: FieldModel, primitive_types: set) -> str:
    """Return the concrete ARCAL storage type for a generated field/variant."""
    if field.type_name in primitive_types:
        return field.cxx_type
    if field.type_is_generated:
        return f"arcal::type::{field.storage_cxx_type}"
    return _qualify_filter(field.cxx_type, primitive_types, field.type_name)


def _make_env() -> "Environment":
    from jinja2 import Environment
    env = Environment(keep_trailing_newline=True)
    env.filters["qualify"] = _qualify_filter
    env.filters["storage_qualify"] = _storage_qualify_filter
    env.filters["sanitize_enum"] = _sanitize_enum
    return env

# Compiled once at import time; reused for every render call.
_ENV = _make_env()
_TMPL_ACCESSOR        = _ENV.from_string(ACCESSOR_H_TEMPLATE)
_TMPL_ACCESSOR_IMPL   = _ENV.from_string(ACCESSOR_IMPL_H_TEMPLATE)
_TMPL_ENUM            = _ENV.from_string(ENUM_H_TEMPLATE)
_TMPL_ENUM_IMPL       = _ENV.from_string(ENUM_IMPL_H_TEMPLATE)
_TMPL_STRING_RESTRICT = _ENV.from_string(STRING_RESTRICTION_H_TEMPLATE)
_TMPL_STRING_RESTRICT_IMPL = _ENV.from_string(STRING_RESTRICTION_IMPL_H_TEMPLATE)
_TMPL_GLOBAL_ELEMENT  = _ENV.from_string(GLOBAL_ELEMENT_H_TEMPLATE)


def render_type(type_model: TypeModel, uci_version: str,
                global_element: GlobalElementModel | None = None) -> str:
    if type_model.is_enum:
        tmpl = _TMPL_ENUM
    elif type_model.is_string_restriction:
        tmpl = _TMPL_STRING_RESTRICT
    else:
        tmpl = _TMPL_ACCESSOR
    return tmpl.render(type=type_model, primitive_types=PRIMITIVE_TYPES,
                       cxx_primitive_map=CXX_PRIMITIVE_MAP, uci_version=uci_version,
                       global_element=global_element)


def render_type_impl(type_model: TypeModel) -> str:
    if type_model.is_enum:
        tmpl = _TMPL_ENUM_IMPL
    elif type_model.is_string_restriction:
        tmpl = _TMPL_STRING_RESTRICT_IMPL
    else:
        tmpl = _TMPL_ACCESSOR_IMPL
    return tmpl.render(type=type_model, primitive_types=PRIMITIVE_TYPES, type_tag=fnv1a32(type_model.name))


def render_global_element(elem: GlobalElementModel) -> str:
    return _TMPL_GLOBAL_ELEMENT.render(elem=elem)


# ---------------------------------------------------------------------------
# CDR handler code generation
# ---------------------------------------------------------------------------

CDR_ENCODE_MAP = {
    "bool":     "arcal::externalizer::cdr::encode_bool",
    "int8_t":   "arcal::externalizer::cdr::encode_int8",
    "uint8_t":  "arcal::externalizer::cdr::encode_uint8",
    "int16_t":  "arcal::externalizer::cdr::encode_int16",
    "uint16_t": "arcal::externalizer::cdr::encode_uint16",
    "int32_t":  "arcal::externalizer::cdr::encode_int32",
    "uint32_t": "arcal::externalizer::cdr::encode_uint32",
    "int64_t":  "arcal::externalizer::cdr::encode_int64",
    "uint64_t": "arcal::externalizer::cdr::encode_uint64",
    "float":    "arcal::externalizer::cdr::encode_float",
    "double":   "arcal::externalizer::cdr::encode_double",
    "std::string": "arcal::externalizer::cdr::encode_string",
    "std::vector<uint8_t>": "arcal::externalizer::cdr::encode_bytes",
}
CDR_DECODE_MAP = {k: v.replace("encode_", "decode_") for k, v in CDR_ENCODE_MAP.items()}


CDR_CPP_TEMPLATE = """\
// Generated by arcal schema compiler. DO NOT EDIT.
#include "uci/type/{{ type.cxx_name }}.h"
#include "generated/type_impl/{{ type.cxx_name }}Impl.h"
#include "CdrPrimitives.h"
#include "CdrRegistry.h"

namespace arcal { namespace externalizer { namespace cdr { namespace gen {

{% if type.is_enum %}\
void {{ type.cxx_name }}_serialize(const uci::base::Accessor& base, std::vector<uint8_t>& buf) {
    const auto& obj = dynamic_cast<const uci::type::{{ type.cxx_name }}&>(base);
    arcal::externalizer::cdr::encode_uint32(buf, static_cast<uint32_t>(obj.getValue()));
}
void {{ type.cxx_name }}_deserialize(const std::vector<uint8_t>& buf, uci::base::Accessor& base) {
    auto& obj = dynamic_cast<uci::type::{{ type.cxx_name }}&>(base);
    std::size_t off = 0;
    obj.setValue(static_cast<uci::type::{{ type.cxx_name }}::EnumerationItem>(
        arcal::externalizer::cdr::decode_uint32(buf, off)));
}
void {{ type.cxx_name }}_deserialize_at(const std::vector<uint8_t>& buf, std::size_t& off, uci::base::Accessor& base) {
    auto& obj = dynamic_cast<uci::type::{{ type.cxx_name }}&>(base);
    obj.setValue(static_cast<uci::type::{{ type.cxx_name }}::EnumerationItem>(
        arcal::externalizer::cdr::decode_uint32(buf, off)));
}
{% elif type.is_string_restriction %}\
void {{ type.cxx_name }}_serialize(const uci::base::Accessor& base, std::vector<uint8_t>& buf) {
    const auto& obj = dynamic_cast<const uci::type::{{ type.cxx_name }}&>(base);
    {{ encode_map.get(type.base_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.getValue());
}
void {{ type.cxx_name }}_deserialize(const std::vector<uint8_t>& buf, uci::base::Accessor& base) {
    auto& obj = dynamic_cast<uci::type::{{ type.cxx_name }}&>(base);
    std::size_t off = 0;
    obj.setValue({{ decode_map.get(type.base_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off));
}
void {{ type.cxx_name }}_deserialize_at(const std::vector<uint8_t>& buf, std::size_t& off, uci::base::Accessor& base) {
    auto& obj = dynamic_cast<uci::type::{{ type.cxx_name }}&>(base);
    obj.setValue({{ decode_map.get(type.base_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off));
}
{% else %}\
static void {{ type.cxx_name }}_serialize_impl(const uci::type::{{ type.cxx_name }}& obj, std::vector<uint8_t>& buf);
static void {{ type.cxx_name }}_deserialize_impl(uci::type::{{ type.cxx_name }}& obj, const std::vector<uint8_t>& buf, std::size_t& off);

void {{ type.cxx_name }}_serialize_impl(const uci::type::{{ type.cxx_name }}& obj, std::vector<uint8_t>& buf) {
{% if type.base_type and type.base_type not in primitive_types %}\
    arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ type.base_type }}Impl::TYPE_TAG).serialize(obj, buf);
{% endif %}\
{% for choice in type.choices %}\
    arcal::externalizer::cdr::encode_uint32(buf, static_cast<uint32_t>(obj.get{{ type.cxx_name }}ChoiceOrdinal()));
{% for v in choice.variants %}\
    if (obj.is{{ v.cxx_name }}()) {
{% if v.type_name in primitive_types %}\
        {{ encode_map.get(v.cxx_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ v.cxx_name }}());
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ v.cxx_type }}Impl::TYPE_TAG).serialize(obj.get{{ v.cxx_name }}(), buf);
{% endif %}\
    }
{% endfor %}\
{% endfor %}\
{% for field in type.fields %}\
{% if field.list_kind %}\
    arcal::externalizer::cdr::encode_sequence_length(buf, static_cast<uint32_t>(obj.get{{ field.cxx_name }}().size()));
    for (const auto& item : obj.get{{ field.cxx_name }}()) {
{% if field.type_name in primitive_types %}\
        {{ encode_map.get(field.cxx_type, 'arcal::externalizer::cdr::encode_string') }}(buf, item);
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ field.cxx_type }}Impl::TYPE_TAG).serialize(item, buf);
{% endif %}\
    }
{% elif field.optional %}\
    arcal::externalizer::cdr::encode_optional_flag(buf, obj.has{{ field.cxx_name }}());
    if (obj.has{{ field.cxx_name }}()) {
{% if field.type_name in primitive_types %}\
        {{ encode_map.get(field.cxx_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ field.cxx_name }}());
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ field.cxx_type }}Impl::TYPE_TAG).serialize(obj.get{{ field.cxx_name }}(), buf);
{% endif %}\
    }
{% else %}\
{% if field.type_name in primitive_types %}\
    {{ encode_map.get(field.cxx_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ field.cxx_name }}());
{% else %}\
    arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ field.cxx_type }}Impl::TYPE_TAG).serialize(obj.get{{ field.cxx_name }}(), buf);
{% endif %}\
{% endif %}\
{% endfor %}\
}

void {{ type.cxx_name }}_deserialize_impl(uci::type::{{ type.cxx_name }}& obj, const std::vector<uint8_t>& buf, std::size_t& off) {
{% if type.base_type and type.base_type not in primitive_types %}\
    arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ type.base_type }}Impl::TYPE_TAG).deserialize_at(buf, off, obj);
{% endif %}\
{% for choice in type.choices %}\
    auto ord = static_cast<uci::type::{{ type.cxx_name }}::{{ type.cxx_name }}ChoiceOrdinalEnum>(
        arcal::externalizer::cdr::decode_uint32(buf, off));
    switch (ord) {
{% for v in choice.variants %}\
    case uci::type::{{ type.cxx_name }}::CHOICE_{{ v.cxx_name | upper }}: {
{% if v.type_name in primitive_types %}\
        obj.choose{{ v.cxx_name }}() = {{ decode_map.get(v.cxx_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off);
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ v.cxx_type }}Impl::TYPE_TAG).deserialize_at(buf, off, obj.choose{{ v.cxx_name }}());
{% endif %}\
        break;
    }
{% endfor %}\
    default: break;
    }
{% endfor %}\
{% for field in type.fields %}\
{% if field.list_kind %}\
    { uint32_t cnt = arcal::externalizer::cdr::decode_sequence_length(buf, off);
      obj.get{{ field.cxx_name }}().clear();
      for (uint32_t i = 0; i < cnt; ++i) {
{% if field.type_name in primitive_types %}\
          obj.get{{ field.cxx_name }}().push_back({{ decode_map.get(field.cxx_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off));
{% else %}\
          {{ field | storage_qualify(primitive_types) }} item;
          arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ field.cxx_type }}Impl::TYPE_TAG).deserialize_at(buf, off, item);
          obj.get{{ field.cxx_name }}().push_back(item);
{% endif %}\
      }
    }
{% elif field.optional %}\
    if (arcal::externalizer::cdr::decode_optional_flag(buf, off)) {
{% if field.type_name in primitive_types %}\
        obj.enable{{ field.cxx_name }}() = {{ decode_map.get(field.cxx_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off);
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ field.cxx_type }}Impl::TYPE_TAG).deserialize_at(buf, off, obj.enable{{ field.cxx_name }}());
{% endif %}\
    }
{% else %}\
{% if field.type_name in primitive_types %}\
    obj.set{{ field.cxx_name }}({{ decode_map.get(field.cxx_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off));
{% else %}\
    arcal::externalizer::CdrRegistry::instance().lookupByTag(arcal::type::{{ field.cxx_type }}Impl::TYPE_TAG).deserialize_at(buf, off, obj.get{{ field.cxx_name }}());
{% endif %}\
{% endif %}\
{% endfor %}\
}

void {{ type.cxx_name }}_serialize(const uci::base::Accessor& base, std::vector<uint8_t>& buf) {
    {{ type.cxx_name }}_serialize_impl(dynamic_cast<const uci::type::{{ type.cxx_name }}&>(base), buf);
}
void {{ type.cxx_name }}_deserialize(const std::vector<uint8_t>& buf, uci::base::Accessor& base) {
    std::size_t off = 0;
    {{ type.cxx_name }}_deserialize_impl(dynamic_cast<uci::type::{{ type.cxx_name }}&>(base), buf, off);
}
void {{ type.cxx_name }}_deserialize_at(const std::vector<uint8_t>& buf, std::size_t& off, uci::base::Accessor& base) {
    {{ type.cxx_name }}_deserialize_impl(dynamic_cast<uci::type::{{ type.cxx_name }}&>(base), buf, off);
}
{% endif %}\

} } } } // namespace arcal::externalizer::cdr::gen
"""


CDR_REGISTER_ALL_TEMPLATE = """\
// Generated by arcal schema compiler. DO NOT EDIT.
#include "CdrRegistry.h"
#include <vector>
#include <cstdint>

namespace uci { namespace base { class Accessor; } }

namespace arcal { namespace externalizer { namespace cdr { namespace gen {

{% for name in type_names %}\
void {{ name }}_serialize(const uci::base::Accessor&, std::vector<uint8_t>&);
void {{ name }}_deserialize(const std::vector<uint8_t>&, uci::base::Accessor&);
void {{ name }}_deserialize_at(const std::vector<uint8_t>&, std::size_t&, uci::base::Accessor&);
{% endfor %}\

} } } } // namespace arcal::externalizer::cdr::gen

namespace arcal { namespace externalizer { namespace cdr {

void register_all_cdr_handlers() {
    auto& reg = CdrRegistry::instance();
{% for name, xsd_name, tag in type_entries %}\
    reg.registerHandler("{{ xsd_name }}", {gen::{{ name }}_serialize, gen::{{ name }}_deserialize, gen::{{ name }}_deserialize_at});
    reg.registerByTag({{ tag }}u, "{{ xsd_name }}", {gen::{{ name }}_serialize, gen::{{ name }}_deserialize, gen::{{ name }}_deserialize_at});
{% endfor %}\
}

} } } // namespace arcal::externalizer::cdr
"""


FACTORY_ALL_CPP_TEMPLATE = """\
// Generated by arcal schema compiler. DO NOT EDIT.
// All typed Reader/Writer factory definitions — compiled as a single TU to
// ensure DDS headers are included exactly once before all UCI type headers.
#include "dds/DdsReader.h"
#include "dds/DdsWriter.h"
#include "dds/DdsAbstractServiceBusConnection.h"
#include "uci/base/UCIException.h"
{% for cxx_type in cxx_types %}\
#include "uci/type/{{ cxx_type }}.h"
{% endfor %}\

namespace uci { namespace type {
{% for cxx_type in cxx_types %}\

{{ cxx_type }}::Reader&
{{ cxx_type }}::createReader(const std::string& topic,
                             uci::base::AbstractServiceBusConnection* asb) {
    auto* dds = dynamic_cast<arcal::dds::DdsAbstractServiceBusConnection*>(asb);
    if (!dds) throwUciException("createReader: ASB is not a DDS connection");
    return *new arcal::dds::DdsReader<{{ cxx_type }}>(*dds, topic);
}

void {{ cxx_type }}::destroyReader(Reader& reader) {
    reader.close();
    delete &reader;
}

{{ cxx_type }}::Writer&
{{ cxx_type }}::createWriter(const std::string& topic,
                             uci::base::AbstractServiceBusConnection* asb) {
    auto* dds = dynamic_cast<arcal::dds::DdsAbstractServiceBusConnection*>(asb);
    if (!dds) throwUciException("createWriter: ASB is not a DDS connection");
    return *new arcal::dds::DdsWriter<{{ cxx_type }}>(*dds, topic);
}

void {{ cxx_type }}::destroyWriter(Writer& writer) {
    writer.close();
    delete &writer;
}
{% endfor %}\
} } // namespace uci::type
"""

_TMPL_CDR_CPP          = _ENV.from_string(CDR_CPP_TEMPLATE)
_TMPL_CDR_REGISTER_ALL = _ENV.from_string(CDR_REGISTER_ALL_TEMPLATE)
_TMPL_FACTORY_ALL_CPP  = _ENV.from_string(FACTORY_ALL_CPP_TEMPLATE)

# ---------------------------------------------------------------------------
# JSON handler code generation
# ---------------------------------------------------------------------------

JSON_CPP_TEMPLATE = """\
// Generated by arcal schema compiler. DO NOT EDIT.
#include "uci/type/{{ type.cxx_name }}.h"
#include "generated/type_impl/{{ type.cxx_name }}Impl.h"
#include "JsonParse.h"
#include "JsonPrimitives.h"
#include "JsonRegistry.h"

namespace arcal { namespace externalizer { namespace json { namespace gen {

namespace json_ext = ::arcal::externalizer::json;

{% if type.is_enum %}\
void {{ type.cxx_name }}_serialize(const uci::base::Accessor& base, std::string& out) {
    const auto& obj = dynamic_cast<const uci::type::{{ type.cxx_name }}&>(base);
    json_ext::emit_string(obj.toName(), out);
}
void {{ type.cxx_name }}_serialize_fields(const uci::base::Accessor& base, std::string& out, bool& first) {
    (void)first;
    {{ type.cxx_name }}_serialize(base, out);
}
void {{ type.cxx_name }}_deserialize(const nlohmann::json& value, uci::base::Accessor& base) {
    auto& obj = dynamic_cast<uci::type::{{ type.cxx_name }}&>(base);
    if (!value.is_string()) {
        throwUciException("JsonExternalizer::read: expected enum string for {{ type.name }}");
    }
    try {
        obj.setValueFromName(value.get<std::string>());
    } catch (const std::exception& e) {
        throwUciException("JsonExternalizer::read: {{ type.name }}: " << e.what());
    }
}
void {{ type.cxx_name }}_deserialize_fields(const nlohmann::json& value, uci::base::Accessor& base) {
    {{ type.cxx_name }}_deserialize(value, base);
}
{% elif type.is_string_restriction %}\
void {{ type.cxx_name }}_serialize(const uci::base::Accessor& base, std::string& out) {
    const auto& obj = dynamic_cast<const uci::type::{{ type.cxx_name }}&>(base);
    json_ext::emit_value(obj.getValue(), out);
}
void {{ type.cxx_name }}_serialize_fields(const uci::base::Accessor& base, std::string& out, bool& first) {
    (void)first;
    {{ type.cxx_name }}_serialize(base, out);
}
void {{ type.cxx_name }}_deserialize(const nlohmann::json& value, uci::base::Accessor& base) {
    auto& obj = dynamic_cast<uci::type::{{ type.cxx_name }}&>(base);
    obj.setValue(json_ext::parse_value<{{ type.base_type }}>(value, "{{ type.name }}"));
}
void {{ type.cxx_name }}_deserialize_fields(const nlohmann::json& value, uci::base::Accessor& base) {
    {{ type.cxx_name }}_deserialize(value, base);
}
{% else %}\
static void {{ type.cxx_name }}_serialize_fields_impl(const uci::type::{{ type.cxx_name }}& obj, std::string& out, bool& first);
static void {{ type.cxx_name }}_deserialize_fields_impl(const nlohmann::json& value, uci::type::{{ type.cxx_name }}& obj);

void {{ type.cxx_name }}_serialize_fields_impl(const uci::type::{{ type.cxx_name }}& obj, std::string& out, bool& first) {
{% if type.base_type and type.base_type not in primitive_types %}\
    json_ext::JsonRegistry::instance().lookup("{{ type.base_type }}").serialize_fields(obj, out, first);
{% endif %}\
{% for choice in type.choices %}\
{% for v in choice.variants %}\
    if (obj.is{{ v.cxx_name }}()) {
        json_ext::emit_key("{{ v.name }}", out, first);
{% if v.type_name in primitive_types %}\
        json_ext::emit_value(obj.get{{ v.cxx_name }}(), out);
{% else %}\
        json_ext::JsonRegistry::instance().lookup("{{ v.type_name }}").serialize(obj.get{{ v.cxx_name }}(), out);
{% endif %}\
    }
{% endfor %}\
{% endfor %}\
{% for field in type.fields %}\
{% if field.list_kind %}\
    json_ext::emit_key("{{ field.name }}", out, first);
    out += '[';
    { bool fa{{ field.cxx_name }} = true;
      for (const auto& item : obj.get{{ field.cxx_name }}()) {
        if (!fa{{ field.cxx_name }}) out += ',';
        fa{{ field.cxx_name }} = false;
{% if field.type_name in primitive_types %}\
        json_ext::emit_value(item, out);
{% else %}\
        json_ext::JsonRegistry::instance().lookup("{{ field.type_name }}").serialize(item, out);
{% endif %}\
      }
    }
    out += ']';
{% elif field.optional %}\
    if (obj.has{{ field.cxx_name }}()) {
        json_ext::emit_key("{{ field.name }}", out, first);
{% if field.type_name in primitive_types %}\
        json_ext::emit_value(obj.get{{ field.cxx_name }}(), out);
{% else %}\
        json_ext::JsonRegistry::instance().lookup("{{ field.type_name }}").serialize(obj.get{{ field.cxx_name }}(), out);
{% endif %}\
    }
{% else %}\
    json_ext::emit_key("{{ field.name }}", out, first);
{% if field.type_name in primitive_types %}\
    json_ext::emit_value(obj.get{{ field.cxx_name }}(), out);
{% else %}\
    json_ext::JsonRegistry::instance().lookup("{{ field.type_name }}").serialize(obj.get{{ field.cxx_name }}(), out);
{% endif %}\
{% endif %}\
{% endfor %}\
}

void {{ type.cxx_name }}_serialize(const uci::base::Accessor& base, std::string& out) {
    out += '{';
    bool first = true;
    {{ type.cxx_name }}_serialize_fields_impl(dynamic_cast<const uci::type::{{ type.cxx_name }}&>(base), out, first);
    out += '}';
}
void {{ type.cxx_name }}_serialize_fields(const uci::base::Accessor& base, std::string& out, bool& first) {
    {{ type.cxx_name }}_serialize_fields_impl(dynamic_cast<const uci::type::{{ type.cxx_name }}&>(base), out, first);
}

void {{ type.cxx_name }}_deserialize_fields_impl(const nlohmann::json& value, uci::type::{{ type.cxx_name }}& obj) {
    json_ext::require_object(value, "{{ type.name }}");
{% if type.base_type and type.base_type not in primitive_types %}\
    json_ext::JsonRegistry::instance().lookup("{{ type.base_type }}").deserialize_fields(value, obj);
{% endif %}\
{% for choice in type.choices %}\
{% set choice_idx = loop.index %}\
    int matchedChoice{{ loop.index }} = 0;
{% for v in choice.variants %}\
    if (auto member = json_ext::optional_member(value, "{{ v.name }}", "{{ type.name }}")) {
        ++matchedChoice{{ choice_idx }};
{% if v.type_name in primitive_types %}\
        obj.choose{{ v.cxx_name }}() = json_ext::parse_value<{{ v.cxx_type }}>(*member, "{{ type.name }}.{{ v.name }}");
{% else %}\
        json_ext::JsonRegistry::instance().lookup("{{ v.type_name }}").deserialize(*member, obj.choose{{ v.cxx_name }}());
{% endif %}\
    }
{% endfor %}\
    if (matchedChoice{{ loop.index }} > 1) {
        throwUciException("JsonExternalizer::read: multiple choice variants present for {{ type.name }}");
    }
{% endfor %}\
{% for field in type.fields %}\
{% if field.list_kind %}\
    {
        const auto& member = json_ext::require_member(value, "{{ field.name }}", "{{ type.name }}");
        json_ext::require_array(member, "{{ type.name }}.{{ field.name }}");
        obj.get{{ field.cxx_name }}().clear();
        for (const auto& itemJson : member) {
{% if field.type_name in primitive_types %}\
            obj.get{{ field.cxx_name }}().push_back(json_ext::parse_value<{{ field.cxx_type }}>(itemJson, "{{ type.name }}.{{ field.name }}"));
{% else %}\
            {{ field | storage_qualify(primitive_types) }} item;
            json_ext::JsonRegistry::instance().lookup("{{ field.type_name }}").deserialize(itemJson, item);
            obj.get{{ field.cxx_name }}().push_back(item);
{% endif %}\
        }
    }
{% elif field.optional %}\
    if (auto member = json_ext::optional_member(value, "{{ field.name }}", "{{ type.name }}")) {
{% if field.type_name in primitive_types %}\
        obj.enable{{ field.cxx_name }}() = json_ext::parse_value<{{ field.cxx_type }}>(*member, "{{ type.name }}.{{ field.name }}");
{% else %}\
        json_ext::JsonRegistry::instance().lookup("{{ field.type_name }}").deserialize(*member, obj.enable{{ field.cxx_name }}());
{% endif %}\
    } else {
        obj.clear{{ field.cxx_name }}();
    }
{% else %}\
    {
        const auto& member = json_ext::require_member(value, "{{ field.name }}", "{{ type.name }}");
{% if field.type_name in primitive_types %}\
        obj.set{{ field.cxx_name }}(json_ext::parse_value<{{ field.cxx_type }}>(member, "{{ type.name }}.{{ field.name }}"));
{% else %}\
        json_ext::JsonRegistry::instance().lookup("{{ field.type_name }}").deserialize(member, obj.get{{ field.cxx_name }}());
{% endif %}\
    }
{% endif %}\
{% endfor %}\
}

void {{ type.cxx_name }}_deserialize(const nlohmann::json& value, uci::base::Accessor& base) {
    {{ type.cxx_name }}_deserialize_fields_impl(value, dynamic_cast<uci::type::{{ type.cxx_name }}&>(base));
}
void {{ type.cxx_name }}_deserialize_fields(const nlohmann::json& value, uci::base::Accessor& base) {
    {{ type.cxx_name }}_deserialize_fields_impl(value, dynamic_cast<uci::type::{{ type.cxx_name }}&>(base));
}
{% endif %}\

} } } } // namespace arcal::externalizer::json::gen
"""

JSON_REGISTER_ALL_TEMPLATE = """\
// Generated by arcal schema compiler. DO NOT EDIT.
#include "JsonRegistry.h"
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace uci { namespace base { class Accessor; } }

namespace arcal { namespace externalizer { namespace json { namespace gen {

{% for name in type_names %}\
void {{ name }}_serialize(const uci::base::Accessor&, std::string&);
void {{ name }}_serialize_fields(const uci::base::Accessor&, std::string&, bool&);
void {{ name }}_deserialize(const nlohmann::json&, uci::base::Accessor&);
void {{ name }}_deserialize_fields(const nlohmann::json&, uci::base::Accessor&);
{% endfor %}\

} } } } // namespace arcal::externalizer::json::gen

namespace arcal { namespace externalizer { namespace json {

void register_all_json_handlers() {
    auto& reg = JsonRegistry::instance();
{% for name, xsd_name in type_entries %}\
    reg.registerHandler("{{ xsd_name }}", {gen::{{ name }}_serialize, gen::{{ name }}_serialize_fields,
                                           gen::{{ name }}_deserialize, gen::{{ name }}_deserialize_fields});
{% endfor %}\
}

} } } // namespace arcal::externalizer::json
"""

_TMPL_JSON_CPP          = _ENV.from_string(JSON_CPP_TEMPLATE)
_TMPL_JSON_REGISTER_ALL = _ENV.from_string(JSON_REGISTER_ALL_TEMPLATE)


def render_json_handler(type_model: TypeModel) -> str:
    return _TMPL_JSON_CPP.render(
        type=type_model,
        primitive_types=PRIMITIVE_TYPES,
    )


def render_json_register_all(type_models: list) -> str:
    type_entries = [(m.cxx_name, m.name) for m in type_models]
    type_names   = [m.cxx_name for m in type_models]
    return _TMPL_JSON_REGISTER_ALL.render(
        type_names=type_names,
        type_entries=type_entries,
    )


def render_cdr_handler(type_model: TypeModel) -> str:
    return _TMPL_CDR_CPP.render(
        type=type_model,
        primitive_types=PRIMITIVE_TYPES,
        encode_map=CDR_ENCODE_MAP,
        decode_map=CDR_DECODE_MAP,
    )


TYPE_LIFECYCLE_TEMPLATE = """\
// Generated by arcal schema compiler. DO NOT EDIT.
{% for cxx_type in cxx_types %}\
#include "generated/type_impl/{{ cxx_type }}Impl.h"
{% endfor %}\

namespace uci { namespace type {

{% for cxx_type in cxx_types %}\
{{ cxx_type }}& {{ cxx_type }}::create(uci::base::AbstractServiceBusConnection*) {
    return *new arcal::type::{{ cxx_type }}Impl{};
}

{{ cxx_type }}& {{ cxx_type }}::create(const {{ cxx_type }}& rhs,
                                       uci::base::AbstractServiceBusConnection* asb) {
    auto& result = create(asb);
    result.copy(rhs);
    return result;
}

void {{ cxx_type }}::destroy({{ cxx_type }}& accessor) {
    delete &dynamic_cast<arcal::type::{{ cxx_type }}Impl&>(accessor);
}

{% endfor %}\
} } // namespace uci::type
"""

_TMPL_TYPE_LIFECYCLE = _ENV.from_string(TYPE_LIFECYCLE_TEMPLATE)


def render_type_lifecycle(type_models: list) -> str:
    return _TMPL_TYPE_LIFECYCLE.render(cxx_types=[m.cxx_name for m in type_models])


ACCESSOR_FACTORY_TEMPLATE = """\
// Generated by arcal schema compiler. DO NOT EDIT.
// Accessor factory: instantiates a default-constructed UCI message type by
// 32-bit FNV-1a tag. Only global element types are registered — these are
// the only UCI types that appear as top-level tagged payloads on the DDS bus.
#include "arcal/AccessorFactory.h"
{% for cxx_type in cxx_types %}\
#include "uci/type/{{ cxx_type }}.h"
{% endfor %}\
#include <unordered_map>

namespace arcal {

uci::base::Accessor* arcalCreateAccessor(uint32_t tag) {
    using Fn = uci::base::Accessor*(*)();
    static const std::unordered_map<uint32_t, Fn> kTable{
{% for cxx_type, tag in type_entries %}\
        { {{ tag }}u, []() -> uci::base::Accessor* { return &uci::type::{{ cxx_type }}::create(nullptr); } },
{% endfor %}\
    };
    auto it = kTable.find(tag);
    return it != kTable.end() ? it->second() : nullptr;
}

void arcalDestroyAccessor(uci::base::Accessor* acc) {
    delete acc;
}

} // namespace arcal
"""

_TMPL_ACCESSOR_FACTORY = _ENV.from_string(ACCESSOR_FACTORY_TEMPLATE)


def render_accessor_factory(elems: list) -> str:
    seen: set = set()
    cxx_types   = []
    type_entries = []
    for e in elems:
        if e.type_name and e.cxx_type not in seen:
            seen.add(e.cxx_type)
            cxx_types.append(e.cxx_type)
            type_entries.append((e.cxx_type, fnv1a32(e.type_name)))
    return _TMPL_ACCESSOR_FACTORY.render(cxx_types=cxx_types, type_entries=type_entries)


def render_factory_all(elems: list) -> str:
    seen: set = set()
    cxx_types = []
    for e in elems:
        if e.type_name and e.cxx_type not in seen:
            seen.add(e.cxx_type)
            cxx_types.append(e.cxx_type)
    return _TMPL_FACTORY_ALL_CPP.render(cxx_types=cxx_types)


def fnv1a32(s: str) -> int:
    h = 2166136261
    for c in s.encode('utf-8'):
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


def render_cdr_register_all(type_models: list) -> str:
    # type_entries: (cxx_name, xsd_name, tag) triples for all types
    type_entries = [(m.cxx_name, m.name, fnv1a32(m.name)) for m in type_models]
    seen_tags = {}
    for _, xsd_name, tag in type_entries:
        previous = seen_tags.setdefault(tag, xsd_name)
        if previous != xsd_name:
            raise ValueError(f"FNV-1a type tag collision: {previous} and {xsd_name} both map to {tag}")
    type_names   = [m.cxx_name for m in type_models]
    return _TMPL_CDR_REGISTER_ALL.render(
        type_names=type_names,
        type_entries=type_entries,
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="arcal OMS Schema Compiler")
    parser.add_argument("--schema", required=True, type=Path)
    parser.add_argument("--out",    required=True, type=Path)
    parser.add_argument("--ns-map", type=Path, default=None)
    args = parser.parse_args()

    ns_map = load_ns_map(args.ns_map) if args.ns_map else dict(PROTECTED_NS_MAP)

    type_out_dir = args.out / "uci" / "type"
    type_out_dir.mkdir(parents=True, exist_ok=True)

    cdr_out_dir  = args.out.parent / "src" / "generated"
    json_out_dir = cdr_out_dir / "json"
    impl_out_dir = cdr_out_dir / "type_impl"
    cdr_out_dir.mkdir(parents=True, exist_ok=True)
    json_out_dir.mkdir(parents=True, exist_ok=True)
    impl_out_dir.mkdir(parents=True, exist_ok=True)

    schema = SchemaParser(args.schema)
    schema.parse_all()

    # Build type_name → GlobalElementModel mapping for nested class injection
    global_element_for_type: dict[str, GlobalElementModel] = {
        elem.type_name: elem for elem in schema.global_elements
    }

    uci_version = "2.5.0"
    generated = 0
    cdr_generated = 0

    type_models = []
    for name, type_model in schema.types.items():
        ge = global_element_for_type.get(name)
        out_path = type_out_dir / f"{xsd_name_to_cxx(name)}.h"
        out_path.write_text(render_type(type_model, uci_version, global_element=ge))
        generated += 1

        impl_path = impl_out_dir / f"{xsd_name_to_cxx(name)}Impl.h"
        impl_path.write_text(render_type_impl(type_model))

        cdr_path = cdr_out_dir / f"{xsd_name_to_cxx(name)}_cdr.cpp"
        cdr_path.write_text(render_cdr_handler(type_model))
        cdr_generated += 1

        json_path = json_out_dir / f"{xsd_name_to_cxx(name)}_json.cpp"
        json_path.write_text(render_json_handler(type_model))

        type_models.append(type_model)

    (cdr_out_dir / "cdr_register_all.cpp").write_text(render_cdr_register_all(type_models))
    (json_out_dir / "json_register_all.cpp").write_text(render_json_register_all(type_models))
    (cdr_out_dir / "type_lifecycle_all.cpp").write_text(render_type_lifecycle(type_models))

    for elem in schema.global_elements:
        out_path = type_out_dir / f"{elem.cxx_name}.h"
        out_path.write_text(render_global_element(elem))
        generated += 1

    (cdr_out_dir / "factories_all.cpp").write_text(render_factory_all(schema.global_elements))
    (cdr_out_dir / "accessor_factory_all.cpp").write_text(render_accessor_factory(schema.global_elements))

    print(f"arcal schema compiler: generated {generated} headers → {type_out_dir}")
    print(f"arcal schema compiler: generated {cdr_generated} CDR handlers + register_all → {cdr_out_dir}")
    print(f"arcal schema compiler: generated {cdr_generated} JSON handlers + register_all → {json_out_dir}")
    print(f"arcal schema compiler: generated factories_all.cpp ({len(schema.global_elements)} types) → {cdr_out_dir}")


if __name__ == "__main__":
    main()
