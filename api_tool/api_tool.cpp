// api_tool.cpp
// Public implementation: delegates to loader, parser, generator.
// Cache clearing happens internally at start of generate() (req 10).

#include "api_tool.h"
#include "api_tool_types.h"
#include "core/api_tool_loader.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#ifdef TOOLS_ENABLED
#include "editor/api_tool_generator.h"
#include "editor/api_tool_parser.h"
#endif

using namespace godot;

namespace api_tool {

// ============================================================================
// Global loader instance
// ============================================================================

static ApiLoader s_loader;

// ============================================================================
// Core interface implementation
// ============================================================================

bool initialize(const String &p_base_dir) {
    Error err = s_loader.initialize(p_base_dir);
    return err == OK;
}

bool is_loaded() {
    return s_loader.is_loaded();
}

void get_version(int32_t &r_major, int32_t &r_minor, int32_t &r_patch) {
    const ApiHeader &hdr = s_loader.get_header();
    r_major = hdr.version_major;
    r_minor = hdr.version_minor;
    r_patch = hdr.version_patch;
}

const ApiHeader &get_header() {
    return s_loader.get_header();
}

// ============================================================================
// Query interface implementation (delegate to loader)
// ============================================================================

const ApiUtilityFunction *find_utility_function(const StringName &p_name) {
    return s_loader.get_utility_function(p_name);
}

const ApiBuiltinClass *find_builtin_class(const StringName &p_name) {
    return s_loader.get_builtin_class(p_name);
}

const ApiClass *find_class(const StringName &p_name) {
    return s_loader.get_class(p_name);
}

const ApiEnumInfo *find_global_enum(const StringName &p_name) {
    return s_loader.get_global_enum(p_name);
}

const ApiConstantInfo *find_global_constant(const StringName &p_name) {
    return s_loader.get_global_constant(p_name);
}

const ApiSingleton *find_singleton(const StringName &p_name) {
    return s_loader.get_singleton(p_name);
}

const ApiNativeStructure *find_native_structure(const StringName &p_name) {
    return s_loader.get_native_structure(p_name);
}

// ============================================================================
// Document query interface implementation (delegate to loader, no cache)
// ============================================================================

std::unique_ptr<ApiClassDocument> find_document(const StringName &p_name) {
#ifdef TOOLS_ENABLED
    return s_loader.find_document(p_name);
#else // !TOOLS_ENABLED
    return nullptr;
#endif // TOOLS_ENABLED
}

std::unique_ptr<ApiUtilityFunctionDocument> find_utility_function_document(const StringName &p_name) {
#ifdef TOOLS_ENABLED
    return s_loader.find_utility_function_document(p_name);
#else // !TOOLS_ENABLED
    return nullptr;
#endif // TOOLS_ENABLED
}

std::unique_ptr<ApiGlobalEnumDocument> find_global_enum_document(const StringName &p_name) {
#ifdef TOOLS_ENABLED
    return s_loader.find_global_enum_document(p_name);
#else // !TOOLS_ENABLED
    return nullptr;
#endif // TOOLS_ENABLED
}

std::unique_ptr<ApiGlobalConstantDocument> find_global_constant_document(const StringName &p_name) {
#ifdef TOOLS_ENABLED
    return s_loader.find_global_constant_document(p_name);
#else // !TOOLS_ENABLED
    return nullptr;
#endif // TOOLS_ENABLED
}

// ============================================================================
// List interface implementation
// ============================================================================

PackedStringArray list_utility_functions() {
    return s_loader.list_utility_functions();
}

PackedStringArray list_builtin_classes() {
    return s_loader.list_builtin_classes();
}

PackedStringArray list_classes() {
    return s_loader.list_classes();
}

PackedStringArray list_global_enums() {
    return s_loader.list_global_enums();
}

PackedStringArray list_global_constants() {
    return s_loader.list_global_constants();
}

PackedStringArray list_singletons() {
    return s_loader.list_singletons();
}

PackedStringArray list_native_structures() {
    return s_loader.list_native_structures();
}

// ============================================================================
// Count interface implementation
// ============================================================================

int32_t get_utility_function_count() {
    return s_loader.get_utility_function_count();
}

int32_t get_builtin_class_count() {
    return s_loader.get_builtin_class_count();
}

int32_t get_class_count() {
    return s_loader.get_class_count();
}

int32_t get_global_enum_count() {
    return s_loader.get_global_enum_count();
}

int32_t get_global_constant_count() {
    return s_loader.get_global_constant_count();
}

// ============================================================================
// Generate interface implementation (only TOOLS_ENABLED)
// Cache is invalidated internally at start of generate() (req 10)
// ============================================================================

static String get_api_dumping_path() {
    ProjectSettings *ps = ProjectSettings::get_singleton();
    bool use_hidden = true;
    if (ps->has_setting("application/config/use_hidden_project_data_directory")) {
        use_hidden = bool(ps->get_setting("application/config/use_hidden_project_data_directory"));
    }
    String data_dir = use_hidden ? ".godot" : "godot";
    return ps->globalize_path("res://") + data_dir + "/.api_dumping";
}

#ifdef TOOLS_ENABLED
Error generate() {
    String out_dir = get_api_dumping_path();

    // Clear cache before generation (req 10)
    s_loader.clear();

    // Use generator to launch subprocess and parse
    Error err = ApiGenerator::generate(out_dir);
    if (err != OK) {
        return err;
    }

    // Reinitialize loader with new data
    return s_loader.initialize(out_dir) == OK ? OK : FAILED;
}
#endif // TOOLS_ENABLED

} // namespace api_tool