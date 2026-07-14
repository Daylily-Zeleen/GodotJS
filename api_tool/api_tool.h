#pragma once

// api_tool.h
// Minimal, stable public interface for the api_tool module.
// This file rarely changes. Detailed type definitions are in api_tool_types.h.
// Provides lazy-loading queries, listing, and generation of API data.

#include <cstdint>
#include <memory>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace api_tool {

// Forward declarations (full definitions in api_tool_types.h)
struct ApiHeader;
struct ApiUtilityFunction;
struct ApiBuiltinClass;
struct ApiClass;
struct ApiEnumInfo;
struct ApiConstantInfo;
struct ApiSingleton;
struct ApiNativeStructure;
struct ApiClassDocument;
struct ApiBuiltinClassDocument;
struct ApiUtilityFunctionDocument;
struct ApiGlobalEnumDocument;
struct ApiGlobalConstantDocument;

// ============================================================================
// Core interface (stable)
// ============================================================================

// Initialize the module with a data root directory. Called at module load.
// Returns true on success.
bool initialize(const godot::String &p_base_dir);

// Check if API data is loaded.
bool is_loaded();

// Get version info (convenience, no need to include api_tool_types.h).
void get_version(int32_t &r_major, int32_t &r_minor, int32_t &r_patch);

// Get the full header (requires api_tool_types.h to dereference).
const ApiHeader &get_header();

// ============================================================================
// Name-based queries (lazy-loaded + cached, thread-safe)
// Returns nullptr if not found. Pointers valid until cache is invalidated.
// ============================================================================

const ApiUtilityFunction *find_utility_function(const godot::StringName &p_name);
const ApiBuiltinClass *find_builtin_class(const godot::StringName &p_name);
const ApiClass *find_class(const godot::StringName &p_name);
const ApiEnumInfo *find_global_enum(const godot::StringName &p_name);
const ApiConstantInfo *find_global_constant(const godot::StringName &p_name);
const ApiSingleton *find_singleton(const godot::StringName &p_name);
const ApiNativeStructure *find_native_structure(const godot::StringName &p_name);

// ============================================================================
// Document queries (no cache, direct file read, TOOLS_ENABLED only)
// Returns std::unique_ptr<T> (caller owns). Returns nullptr if file missing/corrupted.
// ============================================================================
std::unique_ptr<ApiClassDocument> find_class_document(const godot::StringName &p_name);
std::unique_ptr<ApiBuiltinClassDocument> find_builtin_class_document(const godot::StringName &p_name);
std::unique_ptr<ApiUtilityFunctionDocument> find_utility_function_document(const godot::StringName &p_name);
std::unique_ptr<ApiGlobalEnumDocument> find_global_enum_document(const godot::StringName &p_name);
std::unique_ptr<ApiGlobalConstantDocument> find_global_constant_document(const godot::StringName &p_name);

// ============================================================================
// List all names
// ============================================================================

godot::PackedStringArray list_utility_functions();
godot::PackedStringArray list_builtin_classes();
godot::PackedStringArray list_classes();
godot::PackedStringArray list_global_enums();
godot::PackedStringArray list_global_constants();
godot::PackedStringArray list_singletons();
godot::PackedStringArray list_native_structures();

// ============================================================================
// Count queries
// ============================================================================

int32_t get_utility_function_count();
int32_t get_builtin_class_count();
int32_t get_class_count();
int32_t get_global_enum_count();
int32_t get_global_constant_count();

// ============================================================================
// Editor-only: API generation (only TOOLS_ENABLED)
// Cache is invalidated internally at the start of generate().
// ============================================================================

#ifdef TOOLS_ENABLED
// Generate API data by launching Godot subprocess with --dump-extension-api-with-docs,
// then parsing the generated JSON and writing binary files.
// Storage path follows project setting: application/config/use_hidden_project_data_directory
//   true  -> res://.godot/.api_dumping/
//   false -> res://godot/.api_dumping/
// Cache is invalidated at the start.
// Returns godot::OK on success, error code on failure.
godot::Error generate();
#endif // TOOLS_ENABLED

} // namespace api_tool