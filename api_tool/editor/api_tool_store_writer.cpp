#ifdef TOOLS_ENABLED

// editor/api_tool_store_writer.cpp
// Binary file writing implementation (editor-only, TOOLS_ENABLED).
// PayloadWriter + serialize helpers + ApiStoreWriter.
// Uses godot-cpp PropertyInfo/MethodInfo for serialization.

#include "api_tool_store_writer.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>

using namespace godot;

namespace api_tool {

// ============================================================================
// PayloadWriter: builds a PackedByteArray in memory
// ============================================================================

class PayloadWriter {
public:
    void write_string(const godot::String &p_str) {
        godot::PackedByteArray utf8 = p_str.to_utf8_buffer();
        write_u32(static_cast<uint32_t>(utf8.size()));
        if (utf8.size() > 0) {
            int64_t pos = buffer_.size();
            buffer_.resize(pos + utf8.size());
            std::memcpy(buffer_.ptrw() + pos, utf8.ptr(), utf8.size());
        }
    }

    void write_string_name(const godot::StringName &p_sn) {
        write_string(godot::String(p_sn));
    }

    void write_bool(bool p_v) {
        write_u32(p_v ? 1 : 0);
    }

    void write_i32(int32_t p_v) {
        write_u32(static_cast<uint32_t>(p_v));
    }

    void write_u32(uint32_t p_v) {
        int64_t pos = buffer_.size();
        buffer_.resize(pos + 4);
        buffer_.encode_u32(pos, p_v);
    }

    void write_i64(int64_t p_v) {
        int64_t pos = buffer_.size();
        buffer_.resize(pos + 8);
        buffer_.encode_u64(pos, static_cast<uint64_t>(p_v));
    }

    void write_variant(const godot::Variant &p_v) {
        write_u32(static_cast<uint32_t>(p_v.get_type()));
        switch (p_v.get_type()) {
            case godot::Variant::NIL:
                break;
            case godot::Variant::BOOL:
                write_bool(p_v);
                break;
            case godot::Variant::INT:
                write_i64(p_v);
                break;
            case godot::Variant::FLOAT: {
                double d = p_v;
                uint64_t bits;
                std::memcpy(&bits, &d, sizeof(double));
                int64_t pos = buffer_.size();
                buffer_.resize(pos + 8);
                buffer_.encode_u64(pos, bits);
                break;
            }
            case godot::Variant::STRING:
                write_string(p_v);
                break;
            default:
                break;
        }
    }

    const godot::PackedByteArray &get_buffer() const { return buffer_; }

private:
    godot::PackedByteArray buffer_;
};

// ============================================================================
// File I/O helper: write payload to file
// ============================================================================

static godot::Error write_payload_to_file(const godot::String &p_path, const godot::PackedByteArray &p_payload) {
    godot::Ref<godot::FileAccess> f = godot::FileAccess::open(p_path, godot::FileAccess::WRITE);
    if (f.is_null()) {
        return godot::ERR_FILE_CANT_WRITE;
    }
    f->store_32(STORE_MAGIC);
    f->store_32(STORE_VERSION);
    f->store_32(0); // flags: reserved
    f->store_buffer(p_payload.ptr(), p_payload.size());
    f->close();
    return godot::OK;
}

// ============================================================================
// Serialization helpers: structs -> PayloadWriter (v3: PropertyInfo/MethodInfo)
// ============================================================================

static void serialize_property_info(PayloadWriter &w, const godot::PropertyInfo &pi) {
    w.write_u32(static_cast<uint32_t>(pi.type));
    w.write_string_name(pi.name);
    w.write_string_name(pi.class_name);
    w.write_u32(pi.hint);
    w.write_string(pi.hint_string);
    w.write_u32(pi.usage);
}

static void serialize_method_info_payload(PayloadWriter &w, const godot::MethodInfo &mi) {
    w.write_string_name(mi.name);
    w.write_u32(mi.flags);
    w.write_i32(mi.id);
    serialize_property_info(w, mi.return_val);
    // arguments
    w.write_u32(static_cast<uint32_t>(mi.arguments.size()));
    for (int i = 0; i < mi.arguments.size(); i++) {
        serialize_property_info(w, mi.arguments[i]);
    }
    // default_arguments
    w.write_u32(static_cast<uint32_t>(mi.default_arguments.size()));
    for (int i = 0; i < mi.default_arguments.size(); i++) {
        w.write_variant(mi.default_arguments[i]);
    }
    // return_val_metadata
    w.write_u32(static_cast<uint32_t>(mi.return_val_metadata));
    // arguments_metadata
    w.write_u32(static_cast<uint32_t>(mi.arguments_metadata.size()));
    for (int i = 0; i < mi.arguments_metadata.size(); i++) {
        w.write_u32(static_cast<uint32_t>(mi.arguments_metadata[i]));
    }
}

static void serialize_enum_value(PayloadWriter &w, const ApiEnumValue &v) {
    w.write_string_name(v.name);
    w.write_i64(v.value);
}

static void serialize_enum_info(PayloadWriter &w, const ApiEnumInfo &v) {
    w.write_string_name(v.name);
    w.write_bool(v.is_bitfield);
    w.write_i32(static_cast<int32_t>(v.values.size()));
    for (int i = 0; i < v.values.size(); i++) {
        serialize_enum_value(w, v.values[i]);
    }
}

static void serialize_constant_info(PayloadWriter &w, const ApiConstantInfo &v) {
    w.write_string_name(v.name);
    w.write_i64(v.value);
    w.write_bool(v.is_bitfield);
}

static void serialize_api_method_info(PayloadWriter &w, const ApiMethodInfo &ami) {
    serialize_method_info_payload(w, ami.method);
    w.write_i64(ami.hash);
    w.write_i32(static_cast<int32_t>(ami.hash_compatibility.size()));
    for (int j = 0; j < ami.hash_compatibility.size(); j++) {
        w.write_i64(ami.hash_compatibility[j]);
    }
}

// ============================================================================
// ApiStoreWriter: Header
// ============================================================================

godot::Error ApiStoreWriter::write_header(const godot::String &p_path, const ApiHeader &p_data) {
    PayloadWriter w;
    w.write_i32(p_data.version_major);
    w.write_i32(p_data.version_minor);
    w.write_i32(p_data.version_patch);
    w.write_string(p_data.version_status);
    w.write_string(p_data.version_build);
    w.write_string(p_data.version_full_name);
    w.write_string(p_data.precision);
    return write_payload_to_file(p_path, w.get_buffer());
}

// ============================================================================
// ApiStoreWriter: Utility Functions (single file, all at once)
// ============================================================================

godot::Error ApiStoreWriter::write_utility_functions(const godot::String &p_path, const godot::LocalVector<ApiUtilityFunction> &p_data) {
    PayloadWriter w;
    w.write_i32(static_cast<int32_t>(p_data.size()));
    for (int i = 0; i < p_data.size(); i++) {
        const ApiUtilityFunction &func = p_data[i];
        serialize_method_info_payload(w, func.method);
        w.write_i64(func.hash);
        w.write_string_name(func.category);
    }
    return write_payload_to_file(p_path, w.get_buffer());
}

// ============================================================================
// ApiStoreWriter: BuiltinType
// ============================================================================

godot::Error ApiStoreWriter::write_builtin_class(const godot::String &p_path, const ApiBuiltinClass &p_data) {
    PayloadWriter w;
    w.write_string_name(p_data.name);
    w.write_u32(static_cast<uint32_t>(p_data.variant_type));
    w.write_bool(p_data.has_indexing_return_type);
    if (p_data.has_indexing_return_type) {
        w.write_u32(static_cast<uint32_t>(p_data.indexing_type));
    }
    w.write_bool(p_data.is_keyed);
    w.write_bool(p_data.has_destructor);
    // members
    w.write_i32(static_cast<int32_t>(p_data.members.size()));
    for (int i = 0; i < p_data.members.size(); i++) {
        w.write_string_name(p_data.members[i].name);
        w.write_u32(static_cast<uint32_t>(p_data.members[i].type));
    }
    // constants
    w.write_i32(static_cast<int32_t>(p_data.constants.size()));
    for (int i = 0; i < p_data.constants.size(); i++) {
        serialize_constant_info(w, p_data.constants[i]);
    }
    // enums
    w.write_i32(static_cast<int32_t>(p_data.enums.size()));
    for (int i = 0; i < p_data.enums.size(); i++) {
        serialize_enum_info(w, p_data.enums[i]);
    }
    // methods
    w.write_i32(static_cast<int32_t>(p_data.methods.size()));
    for (int i = 0; i < p_data.methods.size(); i++) {
        serialize_api_method_info(w, p_data.methods[i]);
    }
    // operators
    w.write_i32(static_cast<int32_t>(p_data.operators.size()));
    for (int i = 0; i < p_data.operators.size(); i++) {
        w.write_string_name(p_data.operators[i].name);
        w.write_u32(static_cast<uint32_t>(p_data.operators[i].return_type));
        w.write_u32(static_cast<uint32_t>(p_data.operators[i].left_type));
        w.write_u32(static_cast<uint32_t>(p_data.operators[i].right_type));
    }
    // constructors
    w.write_i32(static_cast<int32_t>(p_data.constructors.size()));
    for (int i = 0; i < p_data.constructors.size(); i++) {
        w.write_i32(static_cast<int32_t>(p_data.constructors[i].arguments.size()));
        for (int k = 0; k < p_data.constructors[i].arguments.size(); k++) {
            serialize_property_info(w, p_data.constructors[i].arguments[k]);
        }
    }
    return write_payload_to_file(p_path, w.get_buffer());
}

// ============================================================================
// ApiStoreWriter: Class
// ============================================================================

godot::Error ApiStoreWriter::write_class(const godot::String &p_path, const ApiClass &p_data) {
    PayloadWriter w;
    w.write_string_name(p_data.name);
    w.write_string_name(p_data.inherits);
    w.write_string_name(p_data.api_type);
    w.write_bool(p_data.is_refcounted);
    w.write_bool(p_data.is_instantiable);
    // methods
    w.write_i32(static_cast<int32_t>(p_data.methods.size()));
    for (int i = 0; i < p_data.methods.size(); i++) {
        serialize_api_method_info(w, p_data.methods[i]);
    }
    // signals
    w.write_i32(static_cast<int32_t>(p_data.signals.size()));
    for (int i = 0; i < p_data.signals.size(); i++) {
        w.write_string_name(p_data.signals[i].name);
        w.write_i32(static_cast<int32_t>(p_data.signals[i].arguments.size()));
        for (int j = 0; j < p_data.signals[i].arguments.size(); j++) {
            serialize_property_info(w, p_data.signals[i].arguments[j]);
        }
    }
    // properties
    w.write_i32(static_cast<int32_t>(p_data.properties.size()));
    for (int i = 0; i < p_data.properties.size(); i++) {
        serialize_property_info(w, p_data.properties[i].property);
        w.write_string_name(p_data.properties[i].setter);
        w.write_string_name(p_data.properties[i].getter);
    }
    // enums
    w.write_i32(static_cast<int32_t>(p_data.enums.size()));
    for (int i = 0; i < p_data.enums.size(); i++) {
        serialize_enum_info(w, p_data.enums[i]);
    }
    // constants
    w.write_i32(static_cast<int32_t>(p_data.constants.size()));
    for (int i = 0; i < p_data.constants.size(); i++) {
        serialize_constant_info(w, p_data.constants[i]);
    }
    return write_payload_to_file(p_path, w.get_buffer());
}

// ============================================================================
// ApiStoreWriter: Global Enum
// ============================================================================

godot::Error ApiStoreWriter::write_global_enum(const godot::String &p_path, const ApiEnumInfo &p_data) {
    PayloadWriter w;
    serialize_enum_info(w, p_data);
    return write_payload_to_file(p_path, w.get_buffer());
}

// ============================================================================
// ApiStoreWriter: Global Constant
// ============================================================================

godot::Error ApiStoreWriter::write_global_constant(const godot::String &p_path, const ApiConstantInfo &p_data) {
    PayloadWriter w;
    serialize_constant_info(w, p_data);
    return write_payload_to_file(p_path, w.get_buffer());
}

// ============================================================================
// ApiStoreWriter: Singletons (all in one file)
// ============================================================================

godot::Error ApiStoreWriter::write_singletons(const godot::String &p_path, const godot::LocalVector<ApiSingleton> &p_data) {
    PayloadWriter w;
    w.write_i32(static_cast<int32_t>(p_data.size()));
    for (int i = 0; i < p_data.size(); i++) {
        w.write_string_name(p_data[i].name);
        w.write_string_name(p_data[i].type);
        w.write_bool(p_data[i].user_created);
        w.write_bool(p_data[i].editor_only);
    }
    return write_payload_to_file(p_path, w.get_buffer());
}

// ============================================================================
// ApiStoreWriter: Native Structures (all in one file)
// ============================================================================

godot::Error ApiStoreWriter::write_native_structures(const godot::String &p_path, const godot::LocalVector<ApiNativeStructure> &p_data) {
    PayloadWriter w;
    w.write_i32(static_cast<int32_t>(p_data.size()));
    for (int i = 0; i < p_data.size(); i++) {
        w.write_string(p_data[i].name);
        w.write_string(p_data[i].format);
    }
    return write_payload_to_file(p_path, w.get_buffer());
}

// ============================================================================
// ApiStoreWriter: Class Document
// ============================================================================

static void serialize_method_document(PayloadWriter &w, const ApiMethodDocument &d) {
    w.write_string_name(d.name);
    w.write_string(d.description);
}

static void serialize_signal_document(PayloadWriter &w, const ApiSignalDocument &d) {
    w.write_string_name(d.name);
    w.write_string(d.description);
    w.write_i32(static_cast<int32_t>(d.arguments.size()));
    for (int i = 0; i < d.arguments.size(); i++) {
        serialize_property_info(w, d.arguments[i]);
    }
}

static void serialize_property_document(PayloadWriter &w, const ApiPropertyDocument &d) {
    w.write_string_name(d.name);
    w.write_string(d.description);
}

static void serialize_member_document(PayloadWriter &w, const ApiMemberDocument &d) {
    w.write_string_name(d.name);
    w.write_string(d.description);
}

static void serialize_constant_document(PayloadWriter &w, const ApiConstantDocument &d) {
    w.write_string_name(d.name);
    w.write_string(d.description);
}

static void serialize_enum_value_document(PayloadWriter &w, const ApiEnumValueDocument &d) {
    w.write_string_name(d.name);
    w.write_string(d.description);
}

static void serialize_enum_document(PayloadWriter &w, const ApiEnumDocument &d) {
    w.write_string_name(d.name);
    w.write_i32(static_cast<int32_t>(d.values.size()));
    for (int i = 0; i < d.values.size(); i++) {
        serialize_enum_value_document(w, d.values[i]);
    }
}

static void serialize_operator_document(PayloadWriter &w, const ApiOperatorDocument &d) {
    w.write_string_name(d.name);
    w.write_string(d.description);
}

static void serialize_constructor_document(PayloadWriter &w, const ApiConstructorDocument &d) {
    w.write_i32(d.index);
    w.write_string(d.description);
}

godot::Error ApiStoreWriter::write_class_document(const godot::String &p_path, const ApiClassDocument &p_data) {
    PayloadWriter w;
    w.write_string(p_data.name);
    w.write_string(p_data.brief_description);
    w.write_string(p_data.description);
    w.write_i32(static_cast<int32_t>(p_data.methods.size()));
    for (int i = 0; i < p_data.methods.size(); i++) {
        serialize_method_document(w, p_data.methods[i]);
    }
    w.write_i32(static_cast<int32_t>(p_data.signals.size()));
    for (int i = 0; i < p_data.signals.size(); i++) {
        serialize_signal_document(w, p_data.signals[i]);
    }
    w.write_i32(static_cast<int32_t>(p_data.properties.size()));
    for (int i = 0; i < p_data.properties.size(); i++) {
        serialize_property_document(w, p_data.properties[i]);
    }
    w.write_i32(static_cast<int32_t>(p_data.enums.size()));
    for (int i = 0; i < p_data.enums.size(); i++) {
        serialize_enum_document(w, p_data.enums[i]);
    }
    return write_payload_to_file(p_path, w.get_buffer());
}

godot::Error ApiStoreWriter::write_builtin_class_document(const godot::String &p_path, const ApiBuiltinClassDocument &p_data) {
    PayloadWriter w;
    w.write_string(p_data.name);
    w.write_string(p_data.brief_description);
    w.write_string(p_data.description);
    w.write_i32(static_cast<int32_t>(p_data.methods.size()));
    for (int i = 0; i < p_data.methods.size(); i++) {
        serialize_method_document(w, p_data.methods[i]);
    }
    w.write_i32(static_cast<int32_t>(p_data.members.size()));
    for (int i = 0; i < p_data.members.size(); i++) {
        serialize_member_document(w, p_data.members[i]);
    }
    w.write_i32(static_cast<int32_t>(p_data.constants.size()));
    for (int i = 0; i < p_data.constants.size(); i++) {
        serialize_constant_document(w, p_data.constants[i]);
    }
    w.write_i32(static_cast<int32_t>(p_data.operators.size()));
    for (int i = 0; i < p_data.operators.size(); i++) {
        serialize_operator_document(w, p_data.operators[i]);
    }
    w.write_i32(static_cast<int32_t>(p_data.constructors.size()));
    for (int i = 0; i < p_data.constructors.size(); i++) {
        serialize_constructor_document(w, p_data.constructors[i]);
    }
    return write_payload_to_file(p_path, w.get_buffer());
}

godot::Error ApiStoreWriter::write_utility_function_document(const godot::String &p_path, const ApiUtilityFunctionDocument &p_data) {
    PayloadWriter w;
    w.write_string(p_data.name);
    w.write_string(p_data.description);
    return write_payload_to_file(p_path, w.get_buffer());
}

godot::Error ApiStoreWriter::write_global_enum_document(const godot::String &p_path, const ApiGlobalEnumDocument &p_data) {
    PayloadWriter w;
    w.write_string(p_data.name);
    w.write_i32(static_cast<int32_t>(p_data.values.size()));
    for (int i = 0; i < p_data.values.size(); i++) {
        serialize_enum_value_document(w, p_data.values[i]);
    }
    return write_payload_to_file(p_path, w.get_buffer());
}

godot::Error ApiStoreWriter::write_global_constant_document(const godot::String &p_path, const ApiGlobalConstantDocument &p_data) {
    PayloadWriter w;
    w.write_string(p_data.name);
    w.write_string(p_data.description);
    return write_payload_to_file(p_path, w.get_buffer());
}

} // namespace api_tool

#endif // TOOLS_ENABLED