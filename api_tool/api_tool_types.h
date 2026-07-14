#pragma once

// api_tool_types.h
// All API data structure definitions, mirroring Godot extension_api.json (with docs).
// Reuses godot-cpp types directly: PropertyInfo, MethodInfo, Variant::Type, MethodFlags,
// PropertyHint, PropertyUsageFlags, GDExtensionClassMethodArgumentMetadata.
// No redundant type definitions.

#include "godot_cpp/core/property_info.hpp"
#include "godot_cpp/core/object.hpp"
#include <cstdint>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/templates/local_vector.hpp>

namespace api_tool {

// ============================================================================
// File format constants
// ============================================================================

constexpr uint32_t STORE_MAGIC = 0x41504946; // "APIF"
constexpr uint32_t STORE_VERSION = 3; // v3: PropertyInfo/MethodInfo reuse

// ============================================================================
// Directory/file name constants
// ============================================================================

constexpr const char *DIR_UTILITY_FUNCTIONS = "utility_functions";
constexpr const char *DIR_BUILTIN_CLASSES = "builtin_classes";
constexpr const char *DIR_CLASSES = "classes";
constexpr const char *DIR_CONSTANTS = "constants";
constexpr const char *DIR_SINGLETONS = "singletons";
constexpr const char *DIR_NATIVE_STRUCTURES = "native_structures";
constexpr const char *DIR_DOC_CLASSES = "documents/classes";
constexpr const char *DIR_DOC_BUILTIN_CLASSES = "documents/builtin_classes";
constexpr const char *DIR_DOC_UTILITY_FUNCTIONS = "documents/utility_functions";
constexpr const char *DIR_DOC_GLOBAL_ENUMS = "documents/global_enums";
constexpr const char *DIR_DOC_GLOBAL_CONSTANTS = "documents/global_constants";
constexpr const char *FILE_EXT_DATA = ".api";
constexpr const char *FILE_EXT_DOC = ".doc";
constexpr const char *FILE_HEADER = "header.api";
constexpr const char *FILE_UTILITY_FUNCTIONS = "utility_functions.api";

// ============================================================================
// Type parsing helpers (using godot::Variant::Type, no redefinition)
// ============================================================================

inline godot::Variant::Type parse_variant_type(const godot::String &p_type_name) {
    using VT = godot::Variant::Type;
    if (p_type_name == "Nil" || p_type_name.is_empty()) return VT::NIL;
    if (p_type_name == "bool") return VT::BOOL;
    if (p_type_name == "int") return VT::INT;
    if (p_type_name == "float") return VT::FLOAT;
    if (p_type_name == "String") return VT::STRING;
    if (p_type_name == "Vector2") return VT::VECTOR2;
    if (p_type_name == "Vector2i") return VT::VECTOR2I;
    if (p_type_name == "Rect2") return VT::RECT2;
    if (p_type_name == "Rect2i") return VT::RECT2I;
    if (p_type_name == "Vector3") return VT::VECTOR3;
    if (p_type_name == "Vector3i") return VT::VECTOR3I;
    if (p_type_name == "Transform2D") return VT::TRANSFORM2D;
    if (p_type_name == "Plane") return VT::PLANE;
    if (p_type_name == "Quaternion") return VT::QUATERNION;
    if (p_type_name == "AABB") return VT::AABB;
    if (p_type_name == "Basis") return VT::BASIS;
    if (p_type_name == "Transform3D") return VT::TRANSFORM3D;
    if (p_type_name == "Projection") return VT::PROJECTION;
    if (p_type_name == "Vector4") return VT::VECTOR4;
    if (p_type_name == "Vector4i") return VT::VECTOR4I;
    if (p_type_name == "RID") return VT::RID;
    if (p_type_name == "Object") return VT::OBJECT;
    if (p_type_name == "Callable") return VT::CALLABLE;
    if (p_type_name == "Signal") return VT::SIGNAL;
    if (p_type_name == "Dictionary") return VT::DICTIONARY;
    if (p_type_name == "Array") return VT::ARRAY;
    if (p_type_name == "PackedByteArray") return VT::PACKED_BYTE_ARRAY;
    if (p_type_name == "PackedInt32Array") return VT::PACKED_INT32_ARRAY;
    if (p_type_name == "PackedInt64Array") return VT::PACKED_INT64_ARRAY;
    if (p_type_name == "PackedFloat32Array") return VT::PACKED_FLOAT32_ARRAY;
    if (p_type_name == "PackedFloat64Array") return VT::PACKED_FLOAT64_ARRAY;
    if (p_type_name == "PackedStringArray") return VT::PACKED_STRING_ARRAY;
    if (p_type_name == "PackedVector2Array") return VT::PACKED_VECTOR2_ARRAY;
    if (p_type_name == "PackedVector3Array") return VT::PACKED_VECTOR3_ARRAY;
    if (p_type_name == "PackedColorArray") return VT::PACKED_COLOR_ARRAY;
    if (p_type_name == "PackedVector4Array") return VT::PACKED_VECTOR4_ARRAY;
    return VT::NIL;
}

inline bool is_object_type(const godot::StringName &p_type) {
    godot::String s(p_type);
    if (s.is_empty() || s == "Nil") return false;
    char32_t c = s[0];
    if (c < 'A' || c > 'Z') return false;
    if (s == "AABB" || s == "RID") return false;
    return true;
}

inline bool is_enum_type(const godot::StringName &p_type) {
    godot::String s(p_type);
    return s.contains(".") || s.begins_with("enum::");
}

inline godot::StringName extract_enum_class(const godot::StringName &p_type) {
    godot::String s(p_type);
    if (s.begins_with("enum::")) {
        s = s.substr(6);
    }
    int dot = s.find(".");
    if (dot >= 0) {
        return godot::StringName(s.left(dot));
    }
    return godot::StringName(s);
}

// ============================================================================
// Meta string -> GDExtensionClassMethodArgumentMetadata conversion
// ============================================================================

inline GDExtensionClassMethodArgumentMetadata parse_argument_metadata(const godot::StringName &p_meta) {
    godot::String s(p_meta);
    if (s == "int8") return GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT8;
    if (s == "int16") return GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT16;
    if (s == "int32") return GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT32;
    if (s == "int64") return GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT64;
    if (s == "uint8") return GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT8;
    if (s == "uint16") return GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT16;
    if (s == "uint32") return GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT32;
    if (s == "uint64") return GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT64;
    if (s == "float") return GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_FLOAT;
    if (s == "double") return GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_DOUBLE;
    return GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE;
}

// ============================================================================
// Header / Metadata
// ============================================================================

struct ApiHeader {
    int32_t version_major = 0;
    int32_t version_minor = 0;
    int32_t version_patch = 0;
    godot::String version_status;
    godot::String version_build;
    godot::String version_full_name;
    godot::String precision;
};

// ============================================================================
// MethodInfo wrapper (reuses godot::MethodInfo + JSON-specific fields)
// ============================================================================

struct ApiMethodInfo {
    godot::MethodInfo method; // Reuse godot-cpp: name, return_val, flags, arguments, default_arguments, metadata
    int64_t hash = 0;
    godot::LocalVector<int64_t> hash_compatibility;
};

// ============================================================================
// PropertyInfo wrapper (reuses godot::PropertyInfo + setter/getter + doc)
// ============================================================================

struct ApiPropertyInfo {
    godot::PropertyInfo property; // Reuse godot-cpp: type, name, class_name, hint, hint_string, usage
    godot::StringName setter;
    godot::StringName getter;
};

// ============================================================================
// SignalInfo with PropertyInfo arguments
// ============================================================================

struct ApiSignalInfo {
    godot::StringName name;
    godot::LocalVector<godot::PropertyInfo> arguments;
};

// ============================================================================
// Enum / Constant
// ============================================================================

struct ApiEnumValue {
    godot::StringName name;
    int64_t value = 0;
};

struct ApiEnumInfo {
    godot::StringName name;
    bool is_bitfield = false;
    godot::LocalVector<ApiEnumValue> values;
};

struct ApiConstantInfo {
    godot::StringName name;
    int64_t value = 0;
    bool is_bitfield = false;
};

// ============================================================================
// Operator / Constructor / Member
// ============================================================================

struct ApiOperatorInfo {
    godot::StringName name;
    godot::Variant::Type return_type = godot::Variant::NIL;
    godot::Variant::Type left_type = godot::Variant::NIL;
    godot::Variant::Type right_type = godot::Variant::NIL;
};

struct ApiConstructorInfo {
    godot::LocalVector<godot::PropertyInfo> arguments;
};

struct ApiMemberInfo {
    godot::StringName name;
    godot::Variant::Type type = godot::Variant::NIL;
};

// ============================================================================
// Utility Function (reuses MethodInfo)
// ============================================================================

struct ApiUtilityFunction {
    godot::MethodInfo method; // Reuse MethodInfo (name, return_val, flags, args, etc.)
    int64_t hash = 0;
    godot::StringName category;
};

// ============================================================================
// Builtin Class
// ============================================================================

struct ApiBuiltinClass {
    godot::StringName name;
    godot::Variant::Type variant_type = godot::Variant::NIL;
    bool has_indexing_return_type = false;
    godot::Variant::Type indexing_type = godot::Variant::NIL;
    bool is_keyed = false;
    bool has_destructor = false;
    godot::LocalVector<ApiMemberInfo> members;
    godot::LocalVector<ApiConstantInfo> constants;
    godot::LocalVector<ApiEnumInfo> enums;
    godot::LocalVector<ApiMethodInfo> methods;
    godot::LocalVector<ApiOperatorInfo> operators;
    godot::LocalVector<ApiConstructorInfo> constructors;
};

// ============================================================================
// Class
// ============================================================================

struct ApiClass {
    godot::StringName name;
    godot::StringName inherits;
    godot::StringName api_type;
    bool is_refcounted = false;
    bool is_instantiable = true;
    godot::LocalVector<ApiMethodInfo> methods;
    godot::LocalVector<ApiSignalInfo> signals;
    godot::LocalVector<ApiPropertyInfo> properties;
    godot::LocalVector<ApiEnumInfo> enums;
    godot::LocalVector<ApiConstantInfo> constants;
};

// ============================================================================
// Singleton / Native Structure
// ============================================================================

struct ApiSingleton {
    godot::StringName name;
    godot::StringName type;
    bool user_created = false;
    bool editor_only = false;
};

struct ApiNativeStructure {
    godot::String name;
    godot::String format;
};

#pragma region Document structures
// ============================================================================
// Document sub-structures (stored in .doc files, generated during parsing)
// ============================================================================

struct ApiMethodDocument {
    godot::String name;
    godot::String description;
};

struct ApiSignalDocument {
    godot::String name;
    godot::String description;
    godot::LocalVector<godot::PropertyInfo> arguments;
};

struct ApiPropertyDocument {
    godot::String name;
    godot::String description;
};

struct ApiMemberDocument {
    godot::String name;
    godot::String description;
};

struct ApiConstantDocument {
    godot::String name;
    godot::String description;
};

struct ApiEnumValueDocument {
    godot::String name;
    godot::String description;
};

struct ApiEnumDocument {
    godot::String name;
    godot::LocalVector<ApiEnumValueDocument> values;
};

struct ApiOperatorDocument {
    godot::String name;
    godot::String description;
};

struct ApiConstructorDocument {
    int32_t index = 0;
    godot::String description;
};

// ============================================================================
// Top-level document structures (one .doc file per entity)
// ============================================================================

struct ApiClassDocument {
    godot::String name;
    godot::String brief_description;
    godot::String description;
    godot::LocalVector<ApiMethodDocument> methods;
    godot::LocalVector<ApiSignalDocument> signals;
    godot::LocalVector<ApiPropertyDocument> properties;
    godot::LocalVector<ApiEnumDocument> enums;
};

struct ApiBuiltinClassDocument {
    godot::String name;
    godot::String brief_description;
    godot::String description;
    godot::LocalVector<ApiMethodDocument> methods;
    godot::LocalVector<ApiSignalDocument> signals;
    godot::LocalVector<ApiPropertyDocument> properties;
    godot::LocalVector<ApiEnumDocument> enums;
    godot::LocalVector<ApiMemberDocument> members;
    godot::LocalVector<ApiConstantDocument> constants;
    godot::LocalVector<ApiOperatorDocument> operators;
    godot::LocalVector<ApiConstructorDocument> constructors;
};

struct ApiUtilityFunctionDocument {
    godot::String name;
    godot::String description;
};

struct ApiGlobalEnumDocument {
    godot::String name;
    godot::LocalVector<ApiEnumValueDocument> values;
};

struct ApiGlobalConstantDocument {
    godot::String name;
    godot::String description;
};
#pragma endregion Document structures

// ============================================================================
// Cache invalidation callback type
// ============================================================================

using CacheInvalidatedCallback = void (*)(void *userdata);

} // namespace api_tool