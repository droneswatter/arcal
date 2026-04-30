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
UUID_TYPE_NAME = "UniversallyUniqueIdentifierType"
UUID_CXX_TYPE = "uci::base::UUID"
OMS_NS = "https://www.vdl.afrl.af.mil/programs/oam"
GENERATED_CORE_SHARD_SIZE = 80

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


def write_text_if_changed(path: Path, content: str) -> bool:
    """Write content atomically, preserving mtime when the file is unchanged."""
    encoded = content.encode("utf-8")
    try:
        if path.read_bytes() == encoded:
            return False
    except FileNotFoundError:
        pass

    tmp = path.with_name(f".{path.name}.tmp-{os.getpid()}")
    try:
        tmp.write_bytes(encoded)
        tmp.replace(path)
    finally:
        if tmp.exists():
            tmp.unlink()
    return True


def iter_chunks(items: list, size: int):
    for index in range(0, len(items), size):
        yield index // size, items[index:index + size]


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

SCALAR_PRIMITIVE_TYPES = {
    "boolean", "byte", "short", "int", "long",
    "float", "double", "unsignedByte", "unsignedShort",
    "unsignedInt", "unsignedLong", "integer", "decimal",
    "dateTime", "time", "duration",
}

TEXT_PRIMITIVE_TYPES = {
    "string", "anyURI", "dateTimeStamp",
    "date", "ID", "IDREF", "NMTOKEN",
}


def first_letter_to_lower(s: str) -> str:
    if not s:
        return s
    return s[0].lower() + s[1:]


# Fine-grained accessor type constants for XSD scalar primitives.
SCALAR_PRIMITIVE_ACCESSOR_CONST_MAP: dict[str, str] = {
    "boolean":      "uci::base::accessorType::booleanAccessor",
    "byte":         "uci::base::accessorType::byteAccessor",
    "short":        "uci::base::accessorType::shortAccessor",
    "int":          "uci::base::accessorType::intAccessor",
    "long":         "uci::base::accessorType::longAccessor",
    "float":        "uci::base::accessorType::floatAccessor",
    "double":       "uci::base::accessorType::doubleAccessor",
    "unsignedByte": "uci::base::accessorType::byteAccessor",
    "unsignedShort":"uci::base::accessorType::shortAccessor",
    "unsignedInt":  "uci::base::accessorType::intAccessor",
    "unsignedLong": "uci::base::accessorType::longAccessor",
    "integer":      "uci::base::accessorType::longAccessor",
    "decimal":      "uci::base::accessorType::doubleAccessor",
    "base64Binary": "uci::base::accessorType::binaryAccessor",
    "hexBinary":    "uci::base::accessorType::binaryAccessor",
}

# uci::base::XxxAccessor wrapper type for BoundedList typedef element position.
SCALAR_BOUNDED_LIST_ELEM_TYPE_MAP: dict[str, str] = {
    "bool":                 "uci::base::BooleanAccessor",
    "int8_t":               "uci::base::ByteAccessor",
    "int16_t":              "uci::base::ShortAccessor",
    "int32_t":              "uci::base::IntAccessor",
    "int64_t":              "uci::base::LongAccessor",
    "float":                "uci::base::FloatAccessor",
    "double":               "uci::base::DoubleAccessor",
    "uint8_t":              "uci::base::UnsignedByteAccessor",
    "uint16_t":             "uci::base::UnsignedShortAccessor",
    "uint32_t":             "uci::base::UnsignedIntAccessor",
    "uint64_t":             "uci::base::UnsignedLongAccessor",
    "std::vector<uint8_t>": "uci::base::BinaryAccessor",
}

XSD_PRIMITIVE_TO_XS_ALIAS: dict[str, str] = {
    "boolean":      "xs::Boolean",
    "byte":         "xs::Byte",
    "short":        "xs::Short",
    "int":          "xs::Int",
    "long":         "xs::Long",
    "float":        "xs::Float",
    "double":       "xs::Double",
    "unsignedByte": "xs::UnsignedByte",
    "unsignedShort":"xs::UnsignedShort",
    "unsignedInt":  "xs::UnsignedInt",
    "unsignedLong": "xs::UnsignedLong",
    "integer":      "xs::Long",
    "decimal":      "xs::Double",
    "base64Binary": "xs::Binary",
    "hexBinary":    "xs::Binary",
    "string":       "xs::String",
    "anyURI":       "xs::AnyURI",
    "dateTime":     "xs::DateTime",
    "dateTimeStamp":"xs::DateTimeStamp",
    "date":         "xs::Date",
    "time":         "xs::Time",
    "duration":     "xs::Duration",
    "ID":           "xs::ID",
    "IDREF":        "xs::IDREF",
    "NMTOKEN":      "xs::NMTOKEN",
}

XSD_SCALAR_TO_ACCESSOR_ALIAS: dict[str, str] = {
    "boolean":      "uci::base::BooleanAccessor",
    "byte":         "uci::base::ByteAccessor",
    "short":        "uci::base::ShortAccessor",
    "int":          "uci::base::IntAccessor",
    "long":         "uci::base::LongAccessor",
    "float":        "uci::base::FloatAccessor",
    "double":       "uci::base::DoubleAccessor",
    "unsignedByte": "uci::base::UnsignedByteAccessor",
    "unsignedShort":"uci::base::UnsignedShortAccessor",
    "unsignedInt":  "uci::base::UnsignedIntAccessor",
    "unsignedLong": "uci::base::UnsignedLongAccessor",
    "integer":      "uci::base::LongAccessor",
    "decimal":      "uci::base::DoubleAccessor",
    "base64Binary": "uci::base::BinaryAccessor",
    "hexBinary":    "uci::base::BinaryAccessor",
    "dateTime":     "uci::base::DateTimeAccessor",
    "time":         "uci::base::TimeAccessor",
    "duration":     "uci::base::DurationAccessor",
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
        self.type_is_enum = False
        self.type_is_simple_restriction = False
        self.type_is_string_restriction = False
        self.type_is_complex = False
        self.is_scalar_primitive = type_name in SCALAR_PRIMITIVE_TYPES
        self.is_text_primitive = type_name in TEXT_PRIMITIVE_TYPES
        self.storage_cxx_type = self.cxx_type
        self.is_uuid = type_name == UUID_TYPE_NAME
        self.bounded_list_iface_cxx_type = ""      # raw C++ element type for BoundedList impl
        self.bounded_list_typedef_elem_cxx_type = ""  # wrapper alias type for BoundedList typedef
        self.bounded_list_storage_cxx_type = ""    # StorageT for BoundedListImpl
        self.restriction_cxx_base_type = ""        # underlying C++ type for restriction typedefs


class ChoiceModel:
    def __init__(self, variants):
        self.variants = variants  # list of FieldModel


class TypeModel:
    def __init__(self, name, fields=None, choices=None, base_type=None,
                 enum_values=None, is_simple_restriction=False,
                 is_string_restriction=False, restriction_xsd_base=None,
                 restriction_alias_target=None, restriction_value_alias_target=None,
                 is_abstract=False):
        self.name = name
        self.cxx_name = xsd_name_to_cxx(name)
        self.fields = fields or []
        self.choices = choices or []
        self.base_type = base_type
        self.enum_values = enum_values
        self.is_enum = enum_values is not None
        self.is_simple_restriction = is_simple_restriction
        self.is_string_restriction = is_string_restriction
        self.restriction_xsd_base = restriction_xsd_base
        self.restriction_alias_target = restriction_alias_target
        self.restriction_value_alias_target = restriction_value_alias_target
        self.is_abstract = is_abstract
        self.derived_types: list[str] = []

    @property
    def uses_uuid(self) -> bool:
        if self.base_type == UUID_CXX_TYPE:
            return True
        if any(field.is_uuid for field in self.fields):
            return True
        return any(variant.is_uuid for choice in self.choices for variant in choice.variants)


class GlobalElementModel:
    def __init__(self, name, type_name):
        self.name = name
        self.cxx_name = xsd_name_to_cxx(name)
        self.type_name = type_name
        self.cxx_type = xsd_name_to_cxx(type_name)


class SubsetConfig:
    def __init__(self, cal_name_suffix: str, message_types: list[str], accessor_types: list[str]):
        self.cal_name_suffix = cal_name_suffix
        self.message_types = message_types
        self.accessor_types = accessor_types

    @staticmethod
    def load(path: Path) -> "SubsetConfig":
        with open(path) as f:
            raw = json.load(f)

        cal_name_suffix = raw.get("cal_name_suffix")
        if not isinstance(cal_name_suffix, str) or not cal_name_suffix.strip():
            raise ValueError("subset config must contain non-empty string field 'cal_name_suffix'")

        message_types = raw.get("message_types")
        if not isinstance(message_types, list) or not message_types:
            raise ValueError("subset config must contain non-empty array field 'message_types'")
        if not all(isinstance(item, str) and item.strip() for item in message_types):
            raise ValueError("subset config field 'message_types' must contain only non-empty strings")

        accessor_types = raw.get("accessor_types", [])
        if not isinstance(accessor_types, list):
            raise ValueError("subset config field 'accessor_types' must be an array when present")
        if not all(isinstance(item, str) and item.strip() for item in accessor_types):
            raise ValueError("subset config field 'accessor_types' must contain only non-empty strings")

        return SubsetConfig(
            cal_name_suffix=cal_name_suffix.strip(),
            message_types=[item.strip() for item in message_types],
            accessor_types=[item.strip() for item in accessor_types],
        )


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

    def select_subset(self, requested_message_types: list[str], requested_accessor_types: list[str]) -> tuple[list[GlobalElementModel], list[TypeModel]]:
        selected_globals = self._resolve_global_elements(requested_message_types)
        selected_type_roots = self._resolve_type_roots(requested_accessor_types)
        required_type_names = self._collect_required_type_names(selected_globals, selected_type_roots)

        filtered_globals = [elem for elem in self.global_elements if elem.name in {g.name for g in selected_globals}]
        filtered_types = [type_model for name, type_model in self.types.items() if name in required_type_names]
        return filtered_globals, filtered_types

    def _resolve_global_elements(self, requested_message_types: list[str]) -> list[GlobalElementModel]:
        lookup: dict[str, GlobalElementModel] = {}
        for elem in self.global_elements:
            for key in {elem.name, elem.cxx_name, elem.type_name, elem.cxx_type}:
                lookup.setdefault(key, elem)

        selected = []
        seen_names = set()
        missing = []
        for requested in requested_message_types:
            elem = lookup.get(requested)
            if elem is None:
                missing.append(requested)
                continue
            if elem.name not in seen_names:
                seen_names.add(elem.name)
                selected.append(elem)

        if missing:
            known = ", ".join(sorted(elem.name for elem in self.global_elements))
            raise ValueError(
                "unknown subset message type(s): "
                + ", ".join(missing)
                + "\nknown global element names include: "
                + known
            )

        return selected

    def _resolve_type_roots(self, requested_accessor_types: list[str]) -> list[str]:
        lookup: dict[str, str] = {}
        for name, type_model in self.types.items():
            lookup.setdefault(name, name)
            lookup.setdefault(type_model.cxx_name, name)

        resolved = []
        missing = []
        for requested in requested_accessor_types:
            resolved_name = lookup.get(requested)
            if resolved_name is None:
                missing.append(requested)
                continue
            if resolved_name not in resolved:
                resolved.append(resolved_name)

        if missing:
            known = ", ".join(sorted(self.types.keys()))
            raise ValueError(
                "unknown subset accessor type(s): "
                + ", ".join(missing)
                + "\nknown accessor type names include: "
                + known
            )

        return resolved

    def _collect_required_type_names(self, selected_globals: list[GlobalElementModel], selected_type_roots: list[str]) -> set[str]:
        required = set()
        pending = [elem.type_name for elem in selected_globals]
        pending.extend(selected_type_roots)

        while pending:
            type_name = pending.pop()
            if type_name in required:
                continue

            type_model = self.types.get(type_name)
            if type_model is None:
                continue

            required.add(type_name)

            if type_model.base_type and type_model.base_type in self.types:
                pending.append(type_model.base_type)

            for field in type_model.fields:
                if field.type_name in self.types:
                    pending.append(field.type_name)

            for choice in type_model.choices:
                for variant in choice.variants:
                    if variant.type_name in self.types:
                        pending.append(variant.type_name)

        return required

    def _get_accessor_type(self, type_name: str) -> str:
        if type_name == UUID_TYPE_NAME:
            return "uci::base::accessorType::uuidAccessor"
        if type_name in SCALAR_PRIMITIVE_ACCESSOR_CONST_MAP:
            return SCALAR_PRIMITIVE_ACCESSOR_CONST_MAP[type_name]
        if type_name in TEXT_PRIMITIVE_TYPES:
            return "xs::accessorType::ACCESSOR_TYPE_STRING"
        if type_name in self.types:
            t = self.types[type_name]
            cxx_name = xsd_name_to_cxx(type_name)
            if t.is_simple_restriction:
                if t.restriction_xsd_base in SCALAR_PRIMITIVE_ACCESSOR_CONST_MAP:
                    return SCALAR_PRIMITIVE_ACCESSOR_CONST_MAP[t.restriction_xsd_base]
                return "xs::accessorType::ACCESSOR_TYPE_STRING"
            return f"uci::type::accessorType::{first_letter_to_lower(cxx_name)}"
        return "uci::base::accessorType::ACCESSOR_TYPE_COMPLEX"

    def _get_bounded_list_types(self, field: "FieldModel") -> "tuple[str, str]":
        """Returns (iface_cxx_type, typedef_elem_cxx_type) for BoundedList fields."""
        if field.type_is_simple_restriction and field.type_name in self.types:
            t = self.types[field.type_name]
            return (t.restriction_alias_target, t.restriction_alias_target)
        if field.is_text_primitive or field.type_is_string_restriction:
            return ("std::string", "xs::String")
        if field.is_scalar_primitive:
            wrapper = SCALAR_BOUNDED_LIST_ELEM_TYPE_MAP.get(field.cxx_type, field.cxx_type)
            return (wrapper, wrapper)
        if field.is_uuid:
            return (field.cxx_type, field.cxx_type)
        if field.type_is_generated:
            qualified = f"uci::type::{field.cxx_type}"
            return (qualified, qualified)
        qualified = _qualify_filter(field.cxx_type, PRIMITIVE_TYPES, field.type_name)
        return (qualified, qualified)

    def _get_bounded_list_storage_type(self, field: "FieldModel") -> str:
        """Returns StorageT for BoundedListImpl."""
        if field.type_is_simple_restriction and field.type_name in self.types:
            return self.types[field.type_name].restriction_alias_target
        if field.is_text_primitive or field.type_is_string_restriction:
            return "std::string"
        if field.is_scalar_primitive:
            return SCALAR_BOUNDED_LIST_ELEM_TYPE_MAP.get(field.cxx_type, field.cxx_type)
        if field.is_uuid:
            return field.cxx_type
        if field.type_is_generated:
            return f"arcal::type::{field.cxx_type}Impl"
        return _qualify_filter(field.cxx_type, PRIMITIVE_TYPES, field.type_name)

    def _resolve_accessor_types(self):
        for type_model in self.types.values():
            for field in type_model.fields:
                field.accessor_type = self._get_accessor_type(field.type_name)
                field.is_uuid = field.type_name == UUID_TYPE_NAME
                field.type_is_simple_restriction = (
                    field.type_name in self.types and self.types[field.type_name].is_simple_restriction
                )
                field.type_is_generated = field.type_name in self.types and not field.is_uuid and not field.type_is_simple_restriction
                field.type_is_enum = field.type_name in self.types and self.types[field.type_name].is_enum
                field.type_is_string_restriction = (
                    field.type_name in self.types and self.types[field.type_name].is_string_restriction
                )
                field.type_is_complex = field.type_name in self.types and not (
                    self.types[field.type_name].is_enum or self.types[field.type_name].is_simple_restriction
                )
                if field.type_is_simple_restriction:
                    field.restriction_cxx_base_type = self.types[field.type_name].base_type
                if field.type_is_generated:
                    field.storage_cxx_type = f"{field.cxx_type}Impl"
                if field.list_kind == "bounded":
                    iface, typedef = self._get_bounded_list_types(field)
                    field.bounded_list_iface_cxx_type = iface
                    field.bounded_list_typedef_elem_cxx_type = typedef
                    field.bounded_list_storage_cxx_type = self._get_bounded_list_storage_type(field)
                elif field.list_kind == "unbounded":
                    field.bounded_list_storage_cxx_type = self._get_bounded_list_storage_type(field)
            for choice in type_model.choices:
                for v in choice.variants:
                    v.accessor_type = self._get_accessor_type(v.type_name)
                    v.is_uuid = v.type_name == UUID_TYPE_NAME
                    v.type_is_simple_restriction = (
                        v.type_name in self.types and self.types[v.type_name].is_simple_restriction
                    )
                    v.type_is_generated = v.type_name in self.types and not v.is_uuid and not v.type_is_simple_restriction
                    v.type_is_enum = v.type_name in self.types and self.types[v.type_name].is_enum
                    v.type_is_string_restriction = (
                        v.type_name in self.types and self.types[v.type_name].is_string_restriction
                    )
                    v.type_is_complex = v.type_name in self.types and not (
                        self.types[v.type_name].is_enum or self.types[v.type_name].is_simple_restriction
                    )
                    if v.type_is_simple_restriction:
                        v.restriction_cxx_base_type = self.types[v.type_name].base_type
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
        resolved_type = UUID_CXX_TYPE if type_name == UUID_TYPE_NAME else CXX_PRIMITIVE_MAP.get(type_name, None)
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

        # Non-enumeration restriction: generate a typedef alias to the matching
        # primitive accessor interface. The companion *Value alias remains the
        # raw xs::* value type.
        base = restriction.get("base", "xs:string").split(":")[-1]
        resolved = UUID_CXX_TYPE if name == UUID_TYPE_NAME else CXX_PRIMITIVE_MAP.get(base, "std::string")
        return TypeModel(
            name,
            base_type=resolved,
            fields=[],
            is_simple_restriction=True,
            is_string_restriction=base in TEXT_PRIMITIVE_TYPES,
            restriction_xsd_base=base,
            restriction_alias_target=XSD_SCALAR_TO_ACCESSOR_ALIAS.get(base, XSD_PRIMITIVE_TO_XS_ALIAS.get(base, "xs::String")),
            restriction_value_alias_target=XSD_PRIMITIVE_TO_XS_ALIAS.get(base),
        )


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------

ACCESSOR_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/base/Accessor.h"
#include "uci/base/AbstractServiceBusConnection.h"
#include "uci/base/BoundedList.h"
#include "uci/base/PrimitiveAccessors.h"
#include "uci/base/SimpleList.h"
#include "uci/type/accessorType.h"
#include "xs/accessorType.h"
{% if type.uses_uuid %}\
#include "uci/base/UUID.h"
{% endif %}\
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
    AccessorType getAccessorType() const noexcept override { return uci::type::accessorType::{{ type.cxx_name | first_letter_lower }}; }
    const std::string& typeName() const override {
        static const std::string kName{"{{ type.name }}"};
        return kName;
    }
    virtual void copy(const {{ type.cxx_name }}& rhs) = 0;
    static {{ type.cxx_name }}& create(uci::base::AbstractServiceBusConnection* asb = nullptr);
    static {{ type.cxx_name }}& create(const {{ type.cxx_name }}& rhs,
                                       uci::base::AbstractServiceBusConnection* asb = nullptr);
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
    typedef uci::base::BoundedList<{{ field.bounded_list_typedef_elem_cxx_type }}, {{ field.accessor_type }}> {{ field.cxx_name }};
    virtual {{ field.cxx_name }}& get{{ field.cxx_name }}() = 0;
    virtual const {{ field.cxx_name }}& get{{ field.cxx_name }}() const = 0;
    virtual {{ type.cxx_name }}& set{{ field.cxx_name }}(const {{ field.cxx_name }}&) = 0;
{% elif field.list_kind == "unbounded" %}\
    virtual uci::base::SimpleList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() = 0;
    virtual const uci::base::SimpleList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() const = 0;
{% elif field.is_uuid %}\
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }} get{{ field.cxx_name }}() const = 0;
    virtual {{ type.cxx_name }}& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) = 0;
{% elif field.optional %}\
    virtual bool has{{ field.cxx_name }}() const = 0;
{% if field.type_is_complex %}\
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& enable{{ field.cxx_name }}(uci::base::accessorType::AccessorType accessorType = uci::base::accessorType::null) = 0;
{% else %}\
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& enable{{ field.cxx_name }}() = 0;
{% endif %}\
    virtual {{ type.cxx_name }}& clear{{ field.cxx_name }}() = 0;
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() = 0;
    virtual const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const = 0;
{% if not field.type_is_complex %}\
{% if field.type_is_enum %}\
    virtual {{ type.cxx_name }}& set{{ field.cxx_name }}(typename {{ field.cxx_type | qualify(primitive_types, field.type_name) }}::EnumerationItem v) = 0;
    {{ type.cxx_name }}& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) { return set{{ field.cxx_name }}(v.getValue()); }
{% else %}\
    virtual {{ type.cxx_name }}& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) = 0;
{% if field.is_text_primitive %}\
    {{ type.cxx_name }}& set{{ field.cxx_name }}(const char* v) { return set{{ field.cxx_name }}(std::string{v ? v : ""}); }
{% endif %}\
{% endif %}\
{% endif %}\
{% else %}\
{% if field.is_scalar_primitive %}\
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() = 0;
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }} get{{ field.cxx_name }}() const = 0;
    virtual {{ type.cxx_name }}& set{{ field.cxx_name }}({{ field.cxx_type | qualify(primitive_types, field.type_name) }} v) = 0;
{% elif field.type_is_enum %}\
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() = 0;
    virtual const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const = 0;
    virtual {{ type.cxx_name }}& set{{ field.cxx_name }}(typename {{ field.cxx_type | qualify(primitive_types, field.type_name) }}::EnumerationItem v) = 0;
    {{ type.cxx_name }}& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) { return set{{ field.cxx_name }}(v.getValue()); }
{% else %}\
    virtual {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() = 0;
    virtual const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const = 0;
    virtual {{ type.cxx_name }}& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) = 0;
{% if field.is_text_primitive %}\
    {{ type.cxx_name }}& set{{ field.cxx_name }}(const char* v) { return set{{ field.cxx_name }}(std::string{v ? v : ""}); }
{% endif %}\
{% endif %}\
{% endif %}\
{% endfor %}\

{% for choice in type.choices %}\
    enum {{ type.cxx_name }}Choice {
        {{ type.cxx_name | upper }}_CHOICE_NONE = 0,
{% for v in choice.variants %}\
        {{ type.cxx_name | upper }}_CHOICE_{{ v.cxx_name | upper }},
{% endfor %}\
    };
    virtual {{ type.cxx_name }}Choice get{{ type.cxx_name }}ChoiceOrdinal() const = 0;
    virtual {{ type.cxx_name }}& set{{ type.cxx_name }}ChoiceOrdinal({{ type.cxx_name }}Choice ordinal, uci::base::accessorType::AccessorType type = uci::base::accessorType::null) = 0;
{% for v in choice.variants %}\
    virtual bool is{{ v.cxx_name }}() const = 0;
{% if v.type_is_generated %}\
    virtual {{ v.cxx_type | qualify(primitive_types, v.type_name) }}& choose{{ v.cxx_name }}(uci::base::accessorType::AccessorType type = uci::base::accessorType::null) = 0;
{% else %}\
    virtual {{ v.cxx_type | qualify(primitive_types, v.type_name) }}& choose{{ v.cxx_name }}() = 0;
{% endif %}\
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
class {{ type.cxx_name }}Impl : public virtual uci::type::{{ type.cxx_name }}, public arcal::type::{{ type.base_type }}Impl {
{% else %}\
class {{ type.cxx_name }}Impl : public virtual uci::type::{{ type.cxx_name }} {
{% endif %}\
public:
    using UciType = uci::type::{{ type.cxx_name }};
    static constexpr uint32_t TYPE_TAG = {{ type_tag }}u;

    {{ type.cxx_name }}Impl() = default;
    {{ type.cxx_name }}Impl(const {{ type.cxx_name }}Impl&) = default;
    {{ type.cxx_name }}Impl& operator=(const {{ type.cxx_name }}Impl&) = default;
    ~{{ type.cxx_name }}Impl() override = default;

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
{% elif field.is_uuid %}\
        {{ field.name }}_ = rhs.get{{ field.cxx_name }}();
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
    UciType::{{ field.cxx_name }}& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    const UciType::{{ field.cxx_name }}& get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
    UciType& set{{ field.cxx_name }}(const UciType::{{ field.cxx_name }}& rhs) override {
        {{ field.name }}_.clear();
        for (const auto& item : rhs) { {{ field.name }}_.push_back(item); }
        return *this;
    }
{% elif field.list_kind == "unbounded" %}\
    uci::base::SimpleList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    const uci::base::SimpleList<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}>& get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
{% elif field.is_uuid %}\
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }} get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
    UciType& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) override { {{ field.name }}_ = v; return *this; }
{% elif field.optional %}\
    bool has{{ field.cxx_name }}() const override { return has{{ field.cxx_name }}_; }
{% if field.type_is_complex %}\
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& enable{{ field.cxx_name }}(uci::base::accessorType::AccessorType) override { has{{ field.cxx_name }}_ = true; return {{ field.name }}_; }
{% else %}\
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& enable{{ field.cxx_name }}() override { has{{ field.cxx_name }}_ = true; return {{ field.name }}_; }
{% endif %}\
    UciType& clear{{ field.cxx_name }}() override { has{{ field.cxx_name }}_ = false; return *this; }
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
{% if not field.type_is_complex %}\
{% if field.type_is_enum %}\
    UciType& set{{ field.cxx_name }}(typename {{ field.cxx_type | qualify(primitive_types, field.type_name) }}::EnumerationItem v) override {
        has{{ field.cxx_name }}_ = true;
        {{ field.name }}_.setValue(v);
        return *this;
    }
{% elif field.type_is_generated %}\
    UciType& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) override {
        has{{ field.cxx_name }}_ = true;
        {{ field.name }}_.copy(v);
        return *this;
    }
{% else %}\
    UciType& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) override {
        has{{ field.cxx_name }}_ = true;
        {{ field.name }}_ = v;
        return *this;
    }
{% endif %}\
{% endif %}\
{% else %}\
{% if field.is_scalar_primitive %}\
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }} get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
    UciType& set{{ field.cxx_name }}({{ field.cxx_type | qualify(primitive_types, field.type_name) }} v) override { {{ field.name }}_ = v; return *this; }
{% elif field.type_is_enum %}\
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
    UciType& set{{ field.cxx_name }}(typename {{ field.cxx_type | qualify(primitive_types, field.type_name) }}::EnumerationItem v) override { {{ field.name }}_.setValue(v); return *this; }
{% else %}\
    {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() override { return {{ field.name }}_; }
    const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& get{{ field.cxx_name }}() const override { return {{ field.name }}_; }
{% if field.type_is_generated %}\
    UciType& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) override { {{ field.name }}_.copy(v); return *this; }
{% else %}\
    UciType& set{{ field.cxx_name }}(const {{ field.cxx_type | qualify(primitive_types, field.type_name) }}& v) override { {{ field.name }}_ = v; return *this; }
{% endif %}\
{% endif %}\
{% endif %}\
{% endfor %}\

{% for choice in type.choices %}\
    UciType::{{ type.cxx_name }}Choice get{{ type.cxx_name }}ChoiceOrdinal() const override { return choiceOrdinal_; }
    UciType& set{{ type.cxx_name }}ChoiceOrdinal(UciType::{{ type.cxx_name }}Choice ord, uci::base::accessorType::AccessorType = uci::base::accessorType::null) override { choiceOrdinal_ = ord; return *this; }
{% for v in choice.variants %}\
    bool is{{ v.cxx_name }}() const override { return choiceOrdinal_ == UciType::{{ type.cxx_name | upper }}_CHOICE_{{ v.cxx_name | upper }}; }
{% if v.type_is_generated %}\
    {{ v.cxx_type | qualify(primitive_types, v.type_name) }}& choose{{ v.cxx_name }}(uci::base::accessorType::AccessorType = uci::base::accessorType::null) override { choiceOrdinal_ = UciType::{{ type.cxx_name | upper }}_CHOICE_{{ v.cxx_name | upper }}; return choice{{ v.cxx_name }}_; }
{% else %}\
    {{ v.cxx_type | qualify(primitive_types, v.type_name) }}& choose{{ v.cxx_name }}() override { choiceOrdinal_ = UciType::{{ type.cxx_name | upper }}_CHOICE_{{ v.cxx_name | upper }}; return choice{{ v.cxx_name }}_; }
{% endif %}\
    const {{ v.cxx_type | qualify(primitive_types, v.type_name) }}& get{{ v.cxx_name }}() const override { return choice{{ v.cxx_name }}_; }
{% endfor %}\
{% endfor %}\

private:
{% for field in type.fields %}\
{% if field.list_kind == "bounded" %}\
    uci::base::BoundedListImpl<{{ field.bounded_list_iface_cxx_type }}, {{ field.accessor_type }}, {{ field.min_occurs }}, {{ field.max_occurs_val }}, {{ field.bounded_list_storage_cxx_type }}> {{ field.name }}_;
{% elif field.list_kind == "unbounded" %}\
    uci::base::SimpleListImpl<{{ field.cxx_type | qualify(primitive_types, field.type_name) }}, {{ field.accessor_type }}, {{ field | storage_qualify(primitive_types) }}> {{ field.name }}_;
{% elif field.is_uuid %}\
    {{ field | storage_qualify(primitive_types) }} {{ field.name }}_;
{% elif field.optional %}\
    bool has{{ field.cxx_name }}_{false};
    {{ field | storage_qualify(primitive_types) }} {{ field.name }}_;
{% else %}\
    {{ field | storage_qualify(primitive_types) }} {{ field.name }}_;
{% endif %}\
{% endfor %}\
{% for choice in type.choices %}\
    UciType::{{ type.cxx_name }}Choice choiceOrdinal_{ UciType::{{ type.cxx_name | upper }}_CHOICE_NONE };
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
#include "uci/type/accessorType.h"
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

    AccessorType getAccessorType() const noexcept override { return uci::type::accessorType::{{ type.cxx_name | first_letter_lower }}; }
    const std::string& typeName() const override {
        static const std::string kName{"{{ type.name }}"};
        return kName;
    }

    virtual void copy(const {{ type.cxx_name }}& rhs) = 0;
    static {{ type.cxx_name }}& create(uci::base::AbstractServiceBusConnection* asb = nullptr);
    static {{ type.cxx_name }}& create(const {{ type.cxx_name }}& rhs,
                                       uci::base::AbstractServiceBusConnection* asb = nullptr);
    static void destroy({{ type.cxx_name }}& accessor);

    virtual void setValue(EnumerationItem v) = 0;
    virtual EnumerationItem getValue(bool testForValidity = true) const = 0;

    int getNumberOfItems() const { return {{ type.enum_values | length }}; }

    bool isValid() const { return isValid(getValue()); }
    static bool isValid(EnumerationItem v) { return v > enumNotSet && v < enumMaxExclusive; }
    static bool isValid(const std::string& name) {
        static const char* names[] = {
{% for v in type.enum_values %}\
            "{{ v }}",
{% endfor %}\
        };
        constexpr int total = {{ type.enum_values | length }};
        for (int i = 0; i < total; ++i) {
            if (name == names[i]) return true;
        }
        return false;
    }

    static std::string toName(EnumerationItem v) {
        static const char* names[] = {
            "enumNotSet",
{% for v in type.enum_values %}\
            "{{ v }}",
{% endfor %}\
        };
        if (v < 0 || v >= enumMaxExclusive) return "invalid";
        return names[static_cast<int>(v)];
    }
    std::string toName() const { return toName(getValue()); }

    static EnumerationItem fromName(const std::string& name) {
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
    void setValueFromName(const std::string& name) { setValue(fromName(name)); }

    {{ type.cxx_name }}& operator=(const {{ type.cxx_name }}& rhs) { copy(rhs); return *this; }
    {{ type.cxx_name }}& operator=(EnumerationItem v) { setValue(v); return *this; }

    bool operator==(const {{ type.cxx_name }}& rhs) const { return getValue() == rhs.getValue(); }
    bool operator!=(const {{ type.cxx_name }}& rhs) const { return getValue() != rhs.getValue(); }
    bool operator<(const {{ type.cxx_name }}& rhs)  const { return getValue() <  rhs.getValue(); }
    bool operator<=(const {{ type.cxx_name }}& rhs) const { return getValue() <= rhs.getValue(); }
    bool operator>(const {{ type.cxx_name }}& rhs)  const { return getValue() >  rhs.getValue(); }
    bool operator>=(const {{ type.cxx_name }}& rhs) const { return getValue() >= rhs.getValue(); }

    bool operator==(EnumerationItem rhs) const { return getValue() == rhs; }
    bool operator!=(EnumerationItem rhs) const { return getValue() != rhs; }
    bool operator<(EnumerationItem rhs)  const { return getValue() <  rhs; }
    bool operator<=(EnumerationItem rhs) const { return getValue() <= rhs; }
    bool operator>(EnumerationItem rhs)  const { return getValue() >  rhs; }
    bool operator>=(EnumerationItem rhs) const { return getValue() >= rhs; }

    friend bool operator==(EnumerationItem lhs, const {{ type.cxx_name }}& rhs) { return rhs == lhs; }
    friend bool operator!=(EnumerationItem lhs, const {{ type.cxx_name }}& rhs) { return rhs != lhs; }
    friend bool operator<(EnumerationItem lhs,  const {{ type.cxx_name }}& rhs) { return rhs >  lhs; }
    friend bool operator<=(EnumerationItem lhs, const {{ type.cxx_name }}& rhs) { return rhs >= lhs; }
    friend bool operator>(EnumerationItem lhs,  const {{ type.cxx_name }}& rhs) { return rhs <  lhs; }
    friend bool operator>=(EnumerationItem lhs, const {{ type.cxx_name }}& rhs) { return rhs <= lhs; }

    template<typename charT, typename traits>
    friend std::basic_ostream<charT,traits>& operator<<(
            std::basic_ostream<charT,traits>& os, const {{ type.cxx_name }}& rhs) {
        for (const char c : rhs.toName()) os.put(os.widen(c));
        return os;
    }

protected:
    {{ type.cxx_name }}() = default;
    {{ type.cxx_name }}(const {{ type.cxx_name }}&) = default;
    ~{{ type.cxx_name }}() override = default;
};

} // namespace type
} // namespace uci
"""

ENUM_IMPL_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/type/{{ type.cxx_name }}.h"

namespace arcal {
namespace type {

class {{ type.cxx_name }}Impl : public virtual uci::type::{{ type.cxx_name }} {
public:
    using UciType = uci::type::{{ type.cxx_name }};
    using EnumerationItem = UciType::EnumerationItem;
    static constexpr uint32_t TYPE_TAG = {{ type_tag }}u;

    {{ type.cxx_name }}Impl() = default;
    {{ type.cxx_name }}Impl(const {{ type.cxx_name }}Impl&) = default;
    {{ type.cxx_name }}Impl& operator=(const {{ type.cxx_name }}Impl&) = default;
    ~{{ type.cxx_name }}Impl() override = default;

    void reset() override { value_ = UciType::enumNotSet; }
    void copy(const UciType& rhs) override { value_ = rhs.getValue(); }
    void setValue(EnumerationItem v) override { value_ = v; }
    EnumerationItem getValue(bool = true) const override { return value_; }

private:
    EnumerationItem value_{UciType::enumNotSet};
};

} // namespace type
} // namespace arcal
"""

STRING_RESTRICTION_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/base/PrimitiveAccessors.h"
#include "xs/accessorType.h"

namespace uci {
namespace type {

typedef {{ type.restriction_alias_target }} {{ type.cxx_name }};
{% if type.restriction_value_alias_target and type.restriction_xsd_base in scalar_primitive_types %}\
typedef {{ type.restriction_value_alias_target }} {{ type.cxx_name }}Value;
{% endif %}

} // namespace type
} // namespace uci
"""

STRING_RESTRICTION_IMPL_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.

#include "uci/type/{{ type.cxx_name }}.h"

namespace arcal {
namespace type {

class {{ type.cxx_name }}Impl : public virtual uci::type::{{ type.cxx_name }} {
public:
    using UciType = uci::type::{{ type.cxx_name }};
    static constexpr uint32_t TYPE_TAG = {{ type_tag }}u;

    {{ type.cxx_name }}Impl() = default;
    {{ type.cxx_name }}Impl(const {{ type.cxx_name }}Impl&) = default;
    {{ type.cxx_name }}Impl& operator=(const {{ type.cxx_name }}Impl&) = default;
    ~{{ type.cxx_name }}Impl() override = default;

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
    UUID_TYPE_NAME,
}

CXX_PRIMITIVE_MAP = {
    "boolean": "bool",
    "byte": "int8_t", "short": "int16_t", "int": "int32_t", "long": "int64_t",
    "unsignedByte": "uint8_t", "unsignedShort": "uint16_t",
    "unsignedInt": "uint32_t", "unsignedLong": "uint64_t",
    "integer": "int64_t", "decimal": "double",
    "float": "float", "double": "double",
    "string": "std::string", "anyURI": "std::string",
    # Table 9.1-1: duration, time, dateTime are int64_t (CERT CXX-004937).
    # dateTimeStamp and date are NOT in Table 9.1-1 and remain std::string.
    "dateTime": "int64_t", "time": "int64_t", "duration": "int64_t",
    "dateTimeStamp": "std::string",
    "date": "std::string",
    "base64Binary": "std::vector<uint8_t>", "hexBinary": "std::vector<uint8_t>",
    "ID": "std::string", "IDREF": "std::string", "NMTOKEN": "std::string",
    UUID_TYPE_NAME: UUID_CXX_TYPE,
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
    if field.type_is_simple_restriction:
        return _qualify_filter(field.cxx_type, primitive_types, field.type_name)
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
    env.filters["first_letter_lower"] = first_letter_to_lower
    return env

# Compiled once at import time; reused for every render call.
_ENV = _make_env()
_TMPL_ACCESSOR        = _ENV.from_string(ACCESSOR_H_TEMPLATE)
_TMPL_ACCESSOR_IMPL   = _ENV.from_string(ACCESSOR_IMPL_H_TEMPLATE)
_TMPL_ENUM            = _ENV.from_string(ENUM_H_TEMPLATE)
_TMPL_ENUM_IMPL       = _ENV.from_string(ENUM_IMPL_H_TEMPLATE)
_TMPL_STRING_RESTRICT = _ENV.from_string(STRING_RESTRICTION_H_TEMPLATE)
_TMPL_GLOBAL_ELEMENT  = _ENV.from_string(GLOBAL_ELEMENT_H_TEMPLATE)


def render_type(type_model: TypeModel, uci_version: str,
                global_element: GlobalElementModel | None = None) -> str:
    if type_model.is_enum:
        tmpl = _TMPL_ENUM
    elif type_model.is_simple_restriction:
        tmpl = _TMPL_STRING_RESTRICT
    else:
        tmpl = _TMPL_ACCESSOR
    return tmpl.render(type=type_model, primitive_types=PRIMITIVE_TYPES,
                       cxx_primitive_map=CXX_PRIMITIVE_MAP, uci_version=uci_version,
                       global_element=global_element, uuid_cxx_type=UUID_CXX_TYPE,
                       scalar_primitive_types=SCALAR_PRIMITIVE_TYPES)


def render_type_impl(type_model: TypeModel) -> str:
    if type_model.is_enum:
        tmpl = _TMPL_ENUM_IMPL
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
    UUID_CXX_TYPE: "arcal::externalizer::cdr::encode_uuid",
    # Table 9.1-1: Duration, Time, DateTime are int64_t — map xs:: aliases
    # so the template emits encode_int64, not the string fallback.
    "xs::Duration": "arcal::externalizer::cdr::encode_int64",
    "xs::Time":     "arcal::externalizer::cdr::encode_int64",
    "xs::DateTime": "arcal::externalizer::cdr::encode_int64",
}
CDR_DECODE_MAP = {k: v.replace("encode_", "decode_") for k, v in CDR_ENCODE_MAP.items()}


CDR_CPP_TEMPLATE = """\
// Generated by arcal schema compiler. DO NOT EDIT.
#include "uci/type/{{ type.cxx_name }}.h"
#include "generated/type_impl/{{ type.cxx_name }}Impl.h"
#include "uci/type/accessorType.h"
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
{% else %}\
static void {{ type.cxx_name }}_serialize_impl(const uci::type::{{ type.cxx_name }}& obj, std::vector<uint8_t>& buf);
static void {{ type.cxx_name }}_deserialize_impl(uci::type::{{ type.cxx_name }}& obj, const std::vector<uint8_t>& buf, std::size_t& off);

void {{ type.cxx_name }}_serialize_impl(const uci::type::{{ type.cxx_name }}& obj, std::vector<uint8_t>& buf) {
{% if type.base_type and type.base_type not in primitive_types %}\
    arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ type.base_type | first_letter_lower }}).serialize(obj, buf);
{% endif %}\
{% for choice in type.choices %}\
    arcal::externalizer::cdr::encode_uint32(buf, static_cast<uint32_t>(obj.get{{ type.cxx_name }}ChoiceOrdinal()));
{% for v in choice.variants %}\
    if (obj.is{{ v.cxx_name }}()) {
{% if v.type_name in primitive_types %}\
        {{ encode_map.get(v.cxx_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ v.cxx_name }}());
{% elif v.type_is_simple_restriction %}\
        {{ encode_map.get(v.restriction_cxx_base_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ v.cxx_name }}());
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ v.cxx_type | first_letter_lower }}).serialize(obj.get{{ v.cxx_name }}(), buf);
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
{% elif field.type_is_simple_restriction %}\
        {{ encode_map.get(field.restriction_cxx_base_type, 'arcal::externalizer::cdr::encode_string') }}(buf, item);
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ field.cxx_type | first_letter_lower }}).serialize(item, buf);
{% endif %}\
    }
{% elif field.is_uuid %}\
    {{ encode_map.get(field.cxx_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ field.cxx_name }}());
{% elif field.optional %}\
    arcal::externalizer::cdr::encode_optional_flag(buf, obj.has{{ field.cxx_name }}());
    if (obj.has{{ field.cxx_name }}()) {
{% if field.type_name in primitive_types %}\
        {{ encode_map.get(field.cxx_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ field.cxx_name }}());
{% elif field.type_is_simple_restriction %}\
        {{ encode_map.get(field.restriction_cxx_base_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ field.cxx_name }}());
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ field.cxx_type | first_letter_lower }}).serialize(obj.get{{ field.cxx_name }}(), buf);
{% endif %}\
    }
{% else %}\
{% if field.type_name in primitive_types %}\
    {{ encode_map.get(field.cxx_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ field.cxx_name }}());
{% elif field.type_is_simple_restriction %}\
    {{ encode_map.get(field.restriction_cxx_base_type, 'arcal::externalizer::cdr::encode_string') }}(buf, obj.get{{ field.cxx_name }}());
{% else %}\
    arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ field.cxx_type | first_letter_lower }}).serialize(obj.get{{ field.cxx_name }}(), buf);
{% endif %}\
{% endif %}\
{% endfor %}\
}

void {{ type.cxx_name }}_deserialize_impl(uci::type::{{ type.cxx_name }}& obj, const std::vector<uint8_t>& buf, std::size_t& off) {
{% if type.base_type and type.base_type not in primitive_types %}\
    arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ type.base_type | first_letter_lower }}).deserialize_at(buf, off, obj);
{% endif %}\
{% for choice in type.choices %}\
    auto ord = static_cast<uci::type::{{ type.cxx_name }}::{{ type.cxx_name }}Choice>(
        arcal::externalizer::cdr::decode_uint32(buf, off));
    switch (ord) {
{% for v in choice.variants %}\
    case uci::type::{{ type.cxx_name }}::{{ type.cxx_name | upper }}_CHOICE_{{ v.cxx_name | upper }}: {
{% if v.type_name in primitive_types %}\
        obj.choose{{ v.cxx_name }}() = {{ decode_map.get(v.cxx_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off);
{% elif v.type_is_simple_restriction %}\
        obj.choose{{ v.cxx_name }}() = {{ decode_map.get(v.restriction_cxx_base_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off);
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ v.cxx_type | first_letter_lower }}).deserialize_at(buf, off, obj.choose{{ v.cxx_name }}());
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
{% elif field.type_is_simple_restriction %}\
          obj.get{{ field.cxx_name }}().push_back({{ decode_map.get(field.restriction_cxx_base_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off));
{% else %}\
          {{ field.bounded_list_storage_cxx_type }} item;
          arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ field.cxx_type | first_letter_lower }}).deserialize_at(buf, off, item);
          obj.get{{ field.cxx_name }}().push_back(item);
{% endif %}\
      }
    }
{% elif field.is_uuid %}\
    obj.set{{ field.cxx_name }}({{ decode_map.get(field.cxx_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off));
{% elif field.optional %}\
    if (arcal::externalizer::cdr::decode_optional_flag(buf, off)) {
{% if field.type_name in primitive_types %}\
        obj.enable{{ field.cxx_name }}() = {{ decode_map.get(field.cxx_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off);
{% elif field.type_is_simple_restriction %}\
        obj.enable{{ field.cxx_name }}() = {{ decode_map.get(field.restriction_cxx_base_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off);
{% else %}\
        arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ field.cxx_type | first_letter_lower }}).deserialize_at(buf, off, obj.enable{{ field.cxx_name }}());
{% endif %}\
    }
{% else %}\
{% if field.type_name in primitive_types %}\
    obj.set{{ field.cxx_name }}({{ decode_map.get(field.cxx_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off));
{% elif field.type_is_simple_restriction %}\
    obj.set{{ field.cxx_name }}({{ decode_map.get(field.restriction_cxx_base_type, 'arcal::externalizer::cdr::decode_string') }}(buf, off));
{% else %}\
    arcal::externalizer::CdrRegistry::instance().lookupByTag(uci::type::accessorType::{{ field.cxx_type | first_letter_lower }}).deserialize_at(buf, off, obj.get{{ field.cxx_name }}());
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
// Typed Reader/Writer factory definitions.
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

ACCESSOR_TYPE_H_TEMPLATE = """\
#pragma once
// Generated by arcal schema compiler. DO NOT EDIT.
// uci::type::accessorType — one fine-grained constant per generated UCI type.
// Each constant equals fnv1a32(xsd_type_name) and doubles as the CDR wire tag.
#include "uci/base/accessorType.h"

namespace uci {
namespace type {
namespace accessorType {

using AccessorType = uci::base::accessorType::AccessorType;
{% for cxx_name, xsd_name, tag in type_entries %}\
inline constexpr AccessorType {{ cxx_name | first_letter_lower }} = {{ tag }}u;
{% endfor %}\
} // namespace accessorType
} // namespace type
} // namespace uci
"""

_TMPL_CDR_CPP          = _ENV.from_string(CDR_CPP_TEMPLATE)
_TMPL_CDR_REGISTER_ALL = _ENV.from_string(CDR_REGISTER_ALL_TEMPLATE)
_TMPL_FACTORY_ALL_CPP  = _ENV.from_string(FACTORY_ALL_CPP_TEMPLATE)
_TMPL_ACCESSOR_TYPE_H  = _ENV.from_string(ACCESSOR_TYPE_H_TEMPLATE)

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
{% elif v.type_is_simple_restriction %}\
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
{% elif field.type_is_simple_restriction %}\
        json_ext::emit_value(item, out);
{% else %}\
        json_ext::JsonRegistry::instance().lookup("{{ field.type_name }}").serialize(item, out);
{% endif %}\
      }
    }
    out += ']';
{% elif field.is_uuid %}\
    json_ext::emit_key("{{ field.name }}", out, first);
    json_ext::emit_value(obj.get{{ field.cxx_name }}(), out);
{% elif field.optional %}\
    if (obj.has{{ field.cxx_name }}()) {
        json_ext::emit_key("{{ field.name }}", out, first);
{% if field.type_name in primitive_types %}\
        json_ext::emit_value(obj.get{{ field.cxx_name }}(), out);
{% elif field.type_is_simple_restriction %}\
        json_ext::emit_value(obj.get{{ field.cxx_name }}(), out);
{% else %}\
        json_ext::JsonRegistry::instance().lookup("{{ field.type_name }}").serialize(obj.get{{ field.cxx_name }}(), out);
{% endif %}\
    }
{% else %}\
    json_ext::emit_key("{{ field.name }}", out, first);
{% if field.type_name in primitive_types %}\
    json_ext::emit_value(obj.get{{ field.cxx_name }}(), out);
{% elif field.type_is_simple_restriction %}\
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
{% elif v.type_is_simple_restriction %}\
        obj.choose{{ v.cxx_name }}() = json_ext::parse_value<{{ v.restriction_cxx_base_type }}>(*member, "{{ type.name }}.{{ v.name }}");
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
{% elif field.type_is_simple_restriction %}\
            obj.get{{ field.cxx_name }}().push_back(json_ext::parse_value<{{ field.restriction_cxx_base_type }}>(itemJson, "{{ type.name }}.{{ field.name }}"));
{% else %}\
            {{ field | storage_qualify(primitive_types) }} item;
            json_ext::JsonRegistry::instance().lookup("{{ field.type_name }}").deserialize(itemJson, item);
            obj.get{{ field.cxx_name }}().push_back(item);
{% endif %}\
        }
    }
{% elif field.is_uuid %}\
    {
        const auto& member = json_ext::require_member(value, "{{ field.name }}", "{{ type.name }}");
        obj.set{{ field.cxx_name }}(json_ext::parse_value<{{ field.cxx_type }}>(member, "{{ type.name }}.{{ field.name }}"));
    }
{% elif field.optional %}\
    if (auto member = json_ext::optional_member(value, "{{ field.name }}", "{{ type.name }}")) {
{% if field.type_name in primitive_types %}\
        obj.enable{{ field.cxx_name }}() = json_ext::parse_value<{{ field.cxx_type }}>(*member, "{{ type.name }}.{{ field.name }}");
{% elif field.type_is_simple_restriction %}\
        obj.enable{{ field.cxx_name }}() = json_ext::parse_value<{{ field.restriction_cxx_base_type }}>(*member, "{{ type.name }}.{{ field.name }}");
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
{% elif field.type_is_simple_restriction %}\
        obj.set{{ field.cxx_name }}(json_ext::parse_value<{{ field.restriction_cxx_base_type }}>(member, "{{ type.name }}.{{ field.name }}"));
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
    concrete_types = [m for m in type_models if not m.is_simple_restriction]
    type_entries = [(m.cxx_name, m.name) for m in concrete_types]
    type_names   = [m.cxx_name for m in concrete_types]
    return _TMPL_JSON_REGISTER_ALL.render(
        type_names=type_names,
        type_entries=type_entries,
    )


def render_accessor_type_h(type_models: list) -> str:
    type_entries = [(m.cxx_name, m.name, fnv1a32(m.name)) for m in type_models]
    return _TMPL_ACCESSOR_TYPE_H.render(type_entries=type_entries)


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


def render_type_lifecycle_shards(type_models: list) -> list[tuple[str, str]]:
    cxx_types = [m.cxx_name for m in type_models if not m.is_simple_restriction]
    return [
        (f"type_lifecycle_{index:03d}.cpp", _TMPL_TYPE_LIFECYCLE.render(cxx_types=chunk))
        for index, chunk in iter_chunks(cxx_types, GENERATED_CORE_SHARD_SIZE)
    ]


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


def render_factory_shards(elems: list) -> list[tuple[str, str]]:
    seen: set = set()
    cxx_types = []
    for e in elems:
        if e.type_name and e.cxx_type not in seen:
            seen.add(e.cxx_type)
            cxx_types.append(e.cxx_type)
    return [
        (f"factories_{index:03d}.cpp", _TMPL_FACTORY_ALL_CPP.render(cxx_types=chunk))
        for index, chunk in iter_chunks(cxx_types, GENERATED_CORE_SHARD_SIZE)
    ]


def fnv1a32(s: str) -> int:
    h = 2166136261
    for c in s.encode('utf-8'):
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


def render_cdr_register_all(type_models: list) -> str:
    # type_entries: (cxx_name, xsd_name, tag) triples for all types
    concrete_types = [m for m in type_models if not m.is_simple_restriction]
    type_entries = [(m.cxx_name, m.name, fnv1a32(m.name)) for m in concrete_types]
    seen_tags = {}
    for _, xsd_name, tag in type_entries:
        previous = seen_tags.setdefault(tag, xsd_name)
        if previous != xsd_name:
            raise ValueError(f"FNV-1a type tag collision: {previous} and {xsd_name} both map to {tag}")
    type_names   = [m.cxx_name for m in concrete_types]
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
    parser.add_argument("--subset-config", type=Path, default=None)
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

    subset_config = SubsetConfig.load(args.subset_config) if args.subset_config else None

    # Build type_name → GlobalElementModel mapping for nested class injection
    selected_global_elements = schema.global_elements
    selected_type_models = list(schema.types.values())
    if subset_config is not None:
        selected_global_elements, selected_type_models = schema.select_subset(
            subset_config.message_types,
            subset_config.accessor_types,
        )

    global_element_for_type: dict[str, GlobalElementModel] = {
        elem.type_name: elem for elem in selected_global_elements
    }

    uci_version = "2.5.0"
    generated = 0
    cdr_generated = 0

    existing_type_headers = {path.name for path in type_out_dir.glob("*.h")}
    existing_cdr_sources = {path.name for path in cdr_out_dir.glob("*.cpp")}
    existing_impl_headers = {path.name for path in impl_out_dir.glob("*.h")}
    existing_json_sources = {path.name for path in json_out_dir.glob("*.cpp")}
    written_type_headers: set[str] = set()
    written_cdr_sources: set[str] = set()
    written_impl_headers: set[str] = set()
    written_json_sources: set[str] = set()

    type_models = []
    for type_model in selected_type_models:
        name = type_model.name
        ge = global_element_for_type.get(name)
        type_header_name = f"{xsd_name_to_cxx(name)}.h"
        out_path = type_out_dir / type_header_name
        write_text_if_changed(out_path, render_type(type_model, uci_version, global_element=ge))
        written_type_headers.add(type_header_name)
        generated += 1

        if not type_model.is_simple_restriction:
            impl_header_name = f"{xsd_name_to_cxx(name)}Impl.h"
            impl_path = impl_out_dir / impl_header_name
            write_text_if_changed(impl_path, render_type_impl(type_model))
            written_impl_headers.add(impl_header_name)

            cdr_source_name = f"{xsd_name_to_cxx(name)}_cdr.cpp"
            cdr_path = cdr_out_dir / cdr_source_name
            write_text_if_changed(cdr_path, render_cdr_handler(type_model))
            written_cdr_sources.add(cdr_source_name)
            cdr_generated += 1

            json_source_name = f"{xsd_name_to_cxx(name)}_json.cpp"
            json_path = json_out_dir / json_source_name
            write_text_if_changed(json_path, render_json_handler(type_model))
            written_json_sources.add(json_source_name)

        type_models.append(type_model)

    write_text_if_changed(type_out_dir / "accessorType.h", render_accessor_type_h(type_models))
    written_type_headers.add("accessorType.h")
    write_text_if_changed(cdr_out_dir / "cdr_register_all.cpp", render_cdr_register_all(type_models))
    written_cdr_sources.add("cdr_register_all.cpp")
    write_text_if_changed(json_out_dir / "json_register_all.cpp", render_json_register_all(type_models))
    written_json_sources.add("json_register_all.cpp")
    for source_name, source in render_type_lifecycle_shards(type_models):
        write_text_if_changed(cdr_out_dir / source_name, source)
        written_cdr_sources.add(source_name)

    for elem in selected_global_elements:
        out_path = type_out_dir / f"{elem.cxx_name}.h"
        write_text_if_changed(out_path, render_global_element(elem))
        written_type_headers.add(f"{elem.cxx_name}.h")
        generated += 1

    for source_name, source in render_factory_shards(selected_global_elements):
        write_text_if_changed(cdr_out_dir / source_name, source)
        written_cdr_sources.add(source_name)
    write_text_if_changed(cdr_out_dir / "accessor_factory_all.cpp", render_accessor_factory(selected_global_elements))
    written_cdr_sources.add("accessor_factory_all.cpp")

    for stale_name in existing_type_headers - written_type_headers:
        (type_out_dir / stale_name).unlink()
    for stale_name in existing_cdr_sources - written_cdr_sources:
        (cdr_out_dir / stale_name).unlink()
    for stale_name in existing_impl_headers - written_impl_headers:
        (impl_out_dir / stale_name).unlink()
    for stale_name in existing_json_sources - written_json_sources:
        (json_out_dir / stale_name).unlink()

    print(f"arcal schema compiler: generated {generated} headers → {type_out_dir}")
    print(f"arcal schema compiler: generated {cdr_generated} CDR handlers + register_all → {cdr_out_dir}")
    print(f"arcal schema compiler: generated {cdr_generated} JSON handlers + register_all → {json_out_dir}")
    print(f"arcal schema compiler: generated factory/lifecycle shards ({len(selected_global_elements)} message types) → {cdr_out_dir}")
    if subset_config is not None:
        print(
            "arcal schema compiler: subset "
            f"arcal-{subset_config.cal_name_suffix} includes "
            f"{len(selected_global_elements)} message type(s) and {len(selected_type_models)} dependent accessor type(s)"
        )


if __name__ == "__main__":
    main()
