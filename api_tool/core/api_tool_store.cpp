// core/api_tool_store.cpp
// Binary file reading implementation (runtime, core/).
// PayloadReader + deserialize helpers + ApiStoreReader.
// Uses godot-cpp PropertyInfo/MethodInfo for serialization.

#include "api_tool_store.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>

using namespace godot;

namespace api_tool {

// ============================================================================
// PayloadReader: reads from a PackedByteArray
// ============================================================================

class PayloadReader {
public:
    explicit PayloadReader(const PackedByteArray &p_buffer)
        : buffer_(p_buffer), pos_(0) {}

    String read_string() {
        uint32_t len = read_u32();
        if (len == 0) return String();
        PackedByteArray temp = buffer_.slice(pos_, pos_ + len);
        pos_ += len;
        return temp.get_string_from_utf8();
    }

    StringName read_string_name() {
        return StringName(read_string());
    }

    bool read_bool() {
        return read_u32() != 0;
    }

    int32_t read_i32() {
        return static_cast<int32_t>(read_u32());
    }

    uint32_t read_u32() {
        uint32_t v = buffer_.decode_u32(pos_);
        pos_ += 4;
        return v;
    }

    int64_t read_i64() {
        int64_t v = static_cast<int64_t>(buffer_.decode_u64(pos_));
        pos_ += 8;
        return v;
    }

    Variant read_variant() {
        Variant::Type type = static_cast<Variant::Type>(read_u32());
        switch (type) {
            case Variant::NIL:
                return Variant();
            case Variant::BOOL:
                return Variant(read_bool());
            case Variant::INT:
                return Variant(read_i64());
            case Variant::FLOAT: {
                uint64_t bits = buffer_.decode_u64(pos_);
                pos_ += 8;
                double d;
                std::memcpy(&d, &bits, sizeof(double));
                return Variant(d);
            }
            case Variant::STRING:
                return Variant(read_string());
            default:
                return Variant();
        }
    }

    bool eof() const { return pos_ >= buffer_.size(); }

private:
    PackedByteArray buffer_;
    int64_t pos_ = 0;
};

// ============================================================================
// File I/O helper: read payload from file
// ============================================================================

static Error read_payload_from_file(const String &p_path, PackedByteArray &r_payload) {
    Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
    Error err = FileAccess::get_open_error();
    ERR_FAIL_COND_V_MSG(err, err, vformat("[API Tool] Can't read payload from file (%s): %s", UtilityFunctions::error_string(err), p_path));
    uint32_t magic = f->get_32();

    ERR_FAIL_COND_V_MSG(magic != STORE_MAGIC, ERR_FILE_UNRECOGNIZED, vformat("[API Tool] Can't read payload from file (%s): %s", UtilityFunctions::error_string(ERR_FILE_UNRECOGNIZED), p_path));

    uint32_t version = f->get_32();
    (void)version;
    uint32_t flags = f->get_32();
    (void)flags;
    uint64_t remaining = f->get_length() - f->get_position();
    if (remaining == 0) {
        r_payload = PackedByteArray();
        return OK;
    }
    r_payload = f->get_buffer(static_cast<int64_t>(remaining));
    return OK;
}

// ============================================================================
// Deserialization helpers: PayloadReader -> structs (v3: PropertyInfo/MethodInfo)
// ============================================================================

static PropertyInfo deserialize_property_info(PayloadReader &r) {
    PropertyInfo pi;
    pi.type = static_cast<Variant::Type>(r.read_u32());
    pi.name = r.read_string_name();
    pi.class_name = r.read_string_name();
    pi.hint = r.read_u32();
    pi.hint_string = r.read_string();
    pi.usage = r.read_u32();
    return pi;
}

static void deserialize_method_info_core(PayloadReader &r, MethodInfo &mi) {
    mi.name = r.read_string_name();
    mi.flags = r.read_u32();
    mi.id = r.read_i32();
    mi.return_val = deserialize_property_info(r);
    // arguments
    uint32_t arg_count = r.read_u32();
    mi.arguments.reserve(arg_count);
    for (uint32_t i = 0; i < arg_count; i++) {
        mi.arguments.push_back(deserialize_property_info(r));
    }
    // default_arguments
    uint32_t def_arg_count = r.read_u32();
    mi.default_arguments.reserve(def_arg_count);
    for (uint32_t i = 0; i < def_arg_count; i++) {
        mi.default_arguments.push_back(r.read_variant());
    }
    // return_val_metadata
    mi.return_val_metadata = static_cast<GDExtensionClassMethodArgumentMetadata>(r.read_u32());
    // arguments_metadata
    uint32_t meta_count = r.read_u32();
    mi.arguments_metadata.reserve(meta_count);
    for (uint32_t i = 0; i < meta_count; i++) {
        mi.arguments_metadata.push_back(static_cast<GDExtensionClassMethodArgumentMetadata>(r.read_u32()));
    }
}

static ApiMethodInfo deserialize_api_method_info(PayloadReader &r) {
    ApiMethodInfo ami;
    deserialize_method_info_core(r, ami.method);
    ami.hash = r.read_i64();
    // hash_compatibility
    uint32_t compat_count = r.read_u32();
    ami.hash_compatibility.reserve(compat_count);
    for (uint32_t i = 0; i < compat_count; i++) {
        ami.hash_compatibility.push_back(r.read_i64());
    }
    return ami;
}

static ApiEnumValue deserialize_enum_value(PayloadReader &r) {
    ApiEnumValue v;
    v.name = r.read_string_name();
    v.value = r.read_i64();
    return v;
}

static ApiEnumInfo deserialize_enum_info(PayloadReader &r) {
    ApiEnumInfo v;
    v.name = r.read_string_name();
    v.is_bitfield = r.read_bool();
    int32_t count = r.read_i32();
    v.values.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        v.values.push_back(deserialize_enum_value(r));
    }
    return v;
}

static ApiConstantInfo deserialize_constant_info(PayloadReader &r) {
    ApiConstantInfo v;
    v.name = r.read_string_name();
    v.value = r.read_i64();
    v.is_bitfield = r.read_bool();
    return v;
}

static ApiSignalInfo deserialize_signal_info(PayloadReader &r) {
    ApiSignalInfo v;
    v.name = r.read_string_name();
    int32_t count = r.read_i32();
    v.arguments.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        v.arguments.push_back(deserialize_property_info(r));
    }
    return v;
}

static ApiPropertyInfo deserialize_api_property_info(PayloadReader &r) {
    ApiPropertyInfo v;
    v.property = deserialize_property_info(r);
    v.setter = r.read_string_name();
    v.getter = r.read_string_name();
    return v;
}

static ApiOperatorInfo deserialize_operator_info(PayloadReader &r) {
    ApiOperatorInfo v;
    v.name = r.read_string_name();
    v.return_type = static_cast<Variant::Type>(r.read_u32());
    v.left_type = static_cast<Variant::Type>(r.read_u32());
    v.right_type = static_cast<Variant::Type>(r.read_u32());
    return v;
}

static ApiConstructorInfo deserialize_constructor_info(PayloadReader &r) {
    ApiConstructorInfo v;
    int32_t count = r.read_i32();
    v.arguments.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        v.arguments.push_back(deserialize_property_info(r));
    }
    return v;
}

static ApiMemberInfo deserialize_member_info(PayloadReader &r) {
    ApiMemberInfo v;
    v.name = r.read_string_name();
    v.type = static_cast<Variant::Type>(r.read_u32());
    return v;
}

// ============================================================================
// LocalVector deserialize helper
// ============================================================================

template<typename T>
static void deserialize_local_vector(PayloadReader &r, LocalVector<T> &vec,
        T (*deserializer)(PayloadReader &)) {
    int32_t count = r.read_i32();
    vec.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        vec.push_back(deserializer(r));
    }
}

// ============================================================================
// ApiStoreReader: Header
// ============================================================================

Error ApiStoreReader::read_header(const String &p_path, ApiHeader &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data.version_major = r.read_i32();
    r_data.version_minor = r.read_i32();
    r_data.version_patch = r.read_i32();
    r_data.version_status = r.read_string();
    r_data.version_build = r.read_string();
    r_data.version_full_name = r.read_string();
    r_data.precision = r.read_string();
    return OK;
}

// ============================================================================
// ApiStoreReader: Utility Functions (single file)
// ============================================================================

Error ApiStoreReader::read_utility_functions(const String &p_path, LocalVector<ApiUtilityFunction> &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    int32_t count = r.read_i32();
    r_data.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        ApiUtilityFunction func;
        deserialize_method_info_core(r, func.method);
        func.hash = r.read_i64();
        func.category = r.read_string_name();
        r_data.push_back(func);
    }
    return OK;
}

// ============================================================================
// ApiStoreReader: BuiltinClass
// ============================================================================

Error ApiStoreReader::read_builtin_class(const String &p_path, ApiBuiltinClass &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data.name = r.read_string_name();
    r_data.variant_type = static_cast<Variant::Type>(r.read_u32());
    r_data.has_indexing_return_type = r.read_bool();
    if (r_data.has_indexing_return_type) {
        r_data.indexing_type = static_cast<Variant::Type>(r.read_u32());
    }
    r_data.is_keyed = r.read_bool();
    r_data.has_destructor = r.read_bool();
    deserialize_local_vector(r, r_data.members, deserialize_member_info);
    deserialize_local_vector(r, r_data.constants, deserialize_constant_info);
    deserialize_local_vector(r, r_data.enums, deserialize_enum_info);
    // methods: special handling for ApiMethodInfo
    {
        int32_t count = r.read_i32();
        r_data.methods.reserve(count);
        for (int32_t i = 0; i < count; i++) {
            r_data.methods.push_back(deserialize_api_method_info(r));
        }
    }
    deserialize_local_vector(r, r_data.operators, deserialize_operator_info);
    deserialize_local_vector(r, r_data.constructors, deserialize_constructor_info);
    return OK;
}

// ============================================================================
// ApiStoreReader: Class
// ============================================================================

Error ApiStoreReader::read_class(const String &p_path, ApiClass &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data.name = r.read_string_name();
    r_data.inherits = r.read_string_name();
    r_data.api_type = r.read_string_name();
    r_data.is_refcounted = r.read_bool();
    r_data.is_instantiable = r.read_bool();
    // methods
    {
        int32_t count = r.read_i32();
        r_data.methods.reserve(count);
        for (int32_t i = 0; i < count; i++) {
            r_data.methods.push_back(deserialize_api_method_info(r));
        }
    }
    deserialize_local_vector(r, r_data.signals, deserialize_signal_info);
    deserialize_local_vector(r, r_data.properties, deserialize_api_property_info);
    deserialize_local_vector(r, r_data.enums, deserialize_enum_info);
    deserialize_local_vector(r, r_data.constants, deserialize_constant_info);
    return OK;
}

// ============================================================================
// ApiStoreReader: Global Enum
// ============================================================================

Error ApiStoreReader::read_global_enum(const String &p_path, ApiEnumInfo &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data = deserialize_enum_info(r);
    return OK;
}

// ============================================================================
// ApiStoreReader: Global Constant
// ============================================================================

Error ApiStoreReader::read_global_constant(const String &p_path, ApiConstantInfo &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data = deserialize_constant_info(r);
    return OK;
}

// ============================================================================
// ApiStoreReader: Singletons (single file)
// ============================================================================

Error ApiStoreReader::read_singletons(const String &p_path, LocalVector<ApiSingleton> &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    int32_t count = r.read_i32();
    r_data.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        ApiSingleton s;
        s.name = r.read_string_name();
        s.type = r.read_string_name();
        s.user_created = r.read_bool();
        s.editor_only = r.read_bool();
        r_data.push_back(s);
    }
    return OK;
}

// ============================================================================
// ApiStoreReader: Native Structures (single file)
// ============================================================================

Error ApiStoreReader::read_native_structures(const String &p_path, LocalVector<ApiNativeStructure> &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    int32_t count = r.read_i32();
    r_data.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        ApiNativeStructure ns;
        ns.name = r.read_string();
        ns.format = r.read_string();
        r_data.push_back(ns);
    }
    return OK;
}

#ifdef TOOLS_ENABLED
// ============================================================================
// ApiStoreReader: Class Document
// ============================================================================

static ApiMethodDocument deserialize_method_document(PayloadReader &r) {
    ApiMethodDocument d;
    d.name = r.read_string_name();
    d.description = r.read_string();
    return d;
}

static ApiSignalDocument deserialize_signal_document(PayloadReader &r) {
    ApiSignalDocument d;
    d.name = r.read_string_name();
    d.description = r.read_string();
    int32_t arg_count = r.read_i32();
    d.arguments.reserve(arg_count);
    for (int32_t i = 0; i < arg_count; i++) {
        d.arguments.push_back(deserialize_property_info(r));
    }
    return d;
}

static ApiPropertyDocument deserialize_property_document(PayloadReader &r) {
    ApiPropertyDocument d;
    d.name = r.read_string_name();
    d.description = r.read_string();
    return d;
}

static ApiMemberDocument deserialize_member_document(PayloadReader &r) {
    ApiMemberDocument d;
    d.name = r.read_string_name();
    d.description = r.read_string();
    return d;
}

static ApiConstantDocument deserialize_constant_document(PayloadReader &r) {
    ApiConstantDocument d;
    d.name = r.read_string_name();
    d.description = r.read_string();
    return d;
}

static ApiEnumValueDocument deserialize_enum_value_document(PayloadReader &r) {
    ApiEnumValueDocument d;
    d.name = r.read_string_name();
    d.description = r.read_string();
    return d;
}

static ApiEnumDocument deserialize_enum_document(PayloadReader &r) {
    ApiEnumDocument d;
    d.name = r.read_string_name();
    int32_t count = r.read_i32();
    d.values.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        d.values.push_back(deserialize_enum_value_document(r));
    }
    return d;
}

static ApiOperatorDocument deserialize_operator_document(PayloadReader &r) {
    ApiOperatorDocument d;
    d.name = r.read_string_name();
    d.description = r.read_string();
    return d;
}

static ApiConstructorDocument deserialize_constructor_document(PayloadReader &r) {
    ApiConstructorDocument d;
    d.index = r.read_i32();
    d.description = r.read_string();
    return d;
}

Error ApiStoreReader::read_class_document(const godot::String &p_path, ApiClassDocument &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data.name = r.read_string();
    r_data.brief_description = r.read_string();
    r_data.description = r.read_string();
    int32_t count;
    count = r.read_i32();
    r_data.methods.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.methods.push_back(deserialize_method_document(r));
    }
    count = r.read_i32();
    r_data.signals.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.signals.push_back(deserialize_signal_document(r));
    }
    count = r.read_i32();
    r_data.properties.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.properties.push_back(deserialize_property_document(r));
    }
    count = r.read_i32();
    r_data.enums.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.enums.push_back(deserialize_enum_document(r));
    }
    return OK;
}

Error ApiStoreReader::read_builtin_class_document(const godot::String &p_path, ApiBuiltinClassDocument &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data.name = r.read_string();
    r_data.brief_description = r.read_string();
    r_data.description = r.read_string();
    int32_t count;
    count = r.read_i32();
    r_data.methods.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.methods.push_back(deserialize_method_document(r));
    }
    count = r.read_i32();
    r_data.members.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.members.push_back(deserialize_member_document(r));
    }
    count = r.read_i32();
    r_data.constants.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.constants.push_back(deserialize_constant_document(r));
    }
    count = r.read_i32();
    r_data.operators.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.operators.push_back(deserialize_operator_document(r));
    }
    count = r.read_i32();
    r_data.constructors.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.constructors.push_back(deserialize_constructor_document(r));
    }
    return OK;
}

Error ApiStoreReader::read_utility_function_document(const godot::String &p_path, ApiUtilityFunctionDocument &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data.name = r.read_string();
    r_data.description = r.read_string();
    return OK;
}

Error ApiStoreReader::read_global_enum_document(const godot::String &p_path, ApiGlobalEnumDocument &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data.name = r.read_string();
    int32_t count = r.read_i32();
    r_data.values.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        r_data.values.push_back(deserialize_enum_value_document(r));
    }
    return OK;
}

Error ApiStoreReader::read_global_constant_document(const godot::String &p_path, ApiGlobalConstantDocument &r_data) {
    PackedByteArray payload;
    Error err = read_payload_from_file(p_path, payload);
    if (err != OK) return err;
    PayloadReader r(payload);
    r_data.name = r.read_string();
    r_data.description = r.read_string();
    return OK;
}
#endif // TOOLS_ENABLED

} // namespace api_tool