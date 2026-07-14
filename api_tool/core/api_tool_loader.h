#pragma once

// core/api_tool_loader.h
// Loading + caching layer (internal).
// Lazily loads API data from disk on demand, caches for subsequent queries.
// Uses std::shared_mutex (RWLock) for concurrent reads, exclusive writes.
// Utility functions are loaded as a single batch from one file.
// Supports cache invalidation callback for external notification.

#include "../api_tool_types.h"
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <deque>
#include <memory>
#include <shared_mutex>

namespace api_tool {

class ApiLoader {
public:
    ApiLoader();
    ~ApiLoader();

    // Initialize: set data root directory, load header.
    godot::Error initialize(const godot::String &p_base_dir);

    // Clear all cached data. Calls on_cache_invalidated callback if set.
    void clear();

    // Set cache invalidation callback (req 5).
    void set_cache_invalidated_callback(CacheInvalidatedCallback p_callback, void *p_userdata);

    bool is_loaded() const;
    const ApiHeader &get_header() const;

    // ---- Name-based queries (returns nullptr if not found) ----
    // Pointers valid until clear() is called.

    const ApiUtilityFunction *get_utility_function(const godot::StringName &p_name);
    const ApiBuiltinClass *get_builtin_class(const godot::StringName &p_name);
    const ApiClass *get_class(const godot::StringName &p_name);
    const ApiEnumInfo *get_global_enum(const godot::StringName &p_name);
    const ApiConstantInfo *get_global_constant(const godot::StringName &p_name);
    const ApiSingleton *get_singleton(const godot::StringName &p_name);
    const ApiNativeStructure *get_native_structure(const godot::StringName &p_name);

#ifdef TOOLS_ENABLED
    // ---- Document queries (no cache, direct file read, TOOLS_ENABLED only) ----
    // Returns std::unique_ptr<T> (caller owns). Returns nullptr if file missing/corrupted.
std::unique_ptr<ApiClassDocument> find_document(const godot::StringName &p_name);
    std::unique_ptr<ApiUtilityFunctionDocument> find_utility_function_document(const godot::StringName &p_name);
    std::unique_ptr<ApiGlobalEnumDocument> find_global_enum_document(const godot::StringName &p_name);
    std::unique_ptr<ApiGlobalConstantDocument> find_global_constant_document(const godot::StringName &p_name);
#endif // TOOLS_ENABLED

    // ---- List all names ----

    godot::PackedStringArray list_utility_functions();
    godot::PackedStringArray list_builtin_classes();
    godot::PackedStringArray list_classes();
    godot::PackedStringArray list_global_enums();
    godot::PackedStringArray list_global_constants();
    godot::PackedStringArray list_singletons();
    godot::PackedStringArray list_native_structures();

    // ---- Count queries (no lock for simple file counts) ----

    int32_t get_utility_function_count();
    int32_t get_builtin_class_count();
    int32_t get_class_count();
    int32_t get_global_enum_count();
    int32_t get_global_constant_count();

private:
    // Stable-pointer cache: deque stores data, HashMap stores name->index.
    // Pointers into deque are stable after push_back (deque block structure).
    template<typename T>
    struct TypedCache {
        std::deque<T> items;
        godot::HashMap<godot::StringName, int32_t> name_to_index;

        const T *find(const godot::StringName &p_name) const {
            auto it = name_to_index.find(p_name);
            if (it == name_to_index.end()) return nullptr;
            return &items[it->value];
        }

        void insert(const godot::StringName &p_name, const T &p_item) {
            int32_t idx = static_cast<int32_t>(items.size());
            items.push_back(p_item);
            name_to_index[p_name] = idx;
        }

        void clear_data() {
            items.clear();
            name_to_index.clear();
        }

        int32_t size() const {
            return static_cast<int32_t>(items.size());
        }
    };

    // Ensure-load: check cache -> load from file -> cache -> return pointer.
    const ApiUtilityFunction *ensure_utility_function(const godot::StringName &p_name);
    const ApiBuiltinClass *ensure_builtin_class(const godot::StringName &p_name);
    const ApiClass *ensure_class(const godot::StringName &p_name);
    const ApiEnumInfo *ensure_global_enum(const godot::StringName &p_name);
    const ApiConstantInfo *ensure_global_constant(const godot::StringName &p_name);
    const ApiSingleton *ensure_singleton(const godot::StringName &p_name);
    const ApiNativeStructure *ensure_native_structure(const godot::StringName &p_name);

    // Load all utility functions from single file (req 4).
    void ensure_all_utility_functions();
    // Load all singletons from single file.
    void ensure_all_singletons();
    // Load all native structures from single file.
    void ensure_all_native_structures();

    // Directory scan helpers (no lock - caller must hold lock).
    godot::PackedStringArray list_files_in_dir(const godot::String &p_subdir);
    godot::LocalVector<godot::String> get_cached_names_utility() const;
    godot::LocalVector<godot::String> get_cached_names_singleton() const;
    godot::LocalVector<godot::String> get_cached_names_native() const;

    godot::String base_dir_;
    ApiHeader header_;
    bool loaded_ = false;

    // Cache invalidation callback (req 5)
    CacheInvalidatedCallback on_cache_invalidated_ = nullptr;
    void *cache_callback_userdata_ = nullptr;

    // Caches
    TypedCache<ApiUtilityFunction> utility_function_cache_;
    bool all_utility_functions_loaded_ = false;

    TypedCache<ApiBuiltinClass> builtin_class_cache_;
    TypedCache<ApiClass> class_cache_;
    TypedCache<ApiEnumInfo> enum_cache_;
    TypedCache<ApiConstantInfo> constant_cache_;

    TypedCache<ApiSingleton> singleton_cache_;
    bool all_singletons_loaded_ = false;

    TypedCache<ApiNativeStructure> native_structure_cache_;
    bool all_native_structures_loaded_ = false;

    // RWLock: shared_mutex for concurrent reads, exclusive writes (req 3)
    mutable std::shared_mutex mutex_;
};

} // namespace api_tool
