// core/api_tool_loader.cpp
// Loading + caching layer implementation (internal).
// Uses std::shared_mutex for concurrent reads, exclusive writes.
// Utility functions loaded as single batch. Supports cache invalidation callback.

#include "api_tool_loader.h"
#include "api_tool_store.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace api_tool {

// ============================================================================
// Directory scan helpers (called under lock)
// ============================================================================

PackedStringArray ApiLoader::list_files_in_dir(const String &p_subdir) {
    String dir_path = base_dir_ + "/" + p_subdir;
    if (!DirAccess::dir_exists_absolute(dir_path)) {
        return PackedStringArray();
    }
    PackedStringArray files = DirAccess::get_files_at(dir_path);
    PackedStringArray result;
    for (int i = 0; i < files.size(); i++) {
        String fname = files[i];
        if (fname.ends_with(FILE_EXT_DATA)) {
            result.append(fname.left(fname.length() - static_cast<int>(strlen(FILE_EXT_DATA))));
        }
    }
    return result;
}

LocalVector<String> ApiLoader::get_cached_names_utility() const {
    LocalVector<String> names;
    names.reserve(utility_function_cache_.items.size());
    for (const auto &item : utility_function_cache_.items) {
        names.push_back(String(item.method.name));
    }
    return names;
}

LocalVector<String> ApiLoader::get_cached_names_singleton() const {
    LocalVector<String> names;
    names.reserve(singleton_cache_.items.size());
    for (const auto &item : singleton_cache_.items) {
        names.push_back(String(item.name));
    }
    return names;
}

LocalVector<String> ApiLoader::get_cached_names_native() const {
    LocalVector<String> names;
    names.reserve(native_structure_cache_.items.size());
    for (const auto &item : native_structure_cache_.items) {
        names.push_back(item.name);
    }
    return names;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ApiLoader::ApiLoader() = default;

ApiLoader::~ApiLoader() {
    clear();
}

// ============================================================================
// Initialize
// ============================================================================

Error ApiLoader::initialize(const String &p_base_dir) {
    std::unique_lock lock(mutex_);

    base_dir_ = p_base_dir;
    loaded_ = false;

    // Try to load header
    String header_path = base_dir_ + "/" + FILE_HEADER;
    Error err = ApiStoreReader::read_header(header_path, header_);
    ERR_FAIL_COND_V_MSG(err, err, "[API Tool] Initialize failed.");
    loaded_ = true;
    return loaded_ ? OK : err;
}

// ============================================================================
// Clear cache + notify callback (req 5)
// ============================================================================

void ApiLoader::clear() {
    std::unique_lock lock(mutex_);
    utility_function_cache_.clear_data();
    all_utility_functions_loaded_ = false;
    builtin_class_cache_.clear_data();
    class_cache_.clear_data();
    enum_cache_.clear_data();
    constant_cache_.clear_data();
    singleton_cache_.clear_data();
    all_singletons_loaded_ = false;
    native_structure_cache_.clear_data();
    all_native_structures_loaded_ = false;
    header_ = ApiHeader();
    loaded_ = false;

    // Call cache invalidation callback if set (req 5)
    if (on_cache_invalidated_) {
        on_cache_invalidated_(cache_callback_userdata_);
    }
}

void ApiLoader::set_cache_invalidated_callback(CacheInvalidatedCallback p_callback, void *p_userdata) {
    on_cache_invalidated_ = p_callback;
    cache_callback_userdata_ = p_userdata;
}

bool ApiLoader::is_loaded() const {
    std::shared_lock lock(mutex_);
    return loaded_;
}

const ApiHeader &ApiLoader::get_header() const {
    return header_;
}

// ============================================================================
// Load all utility functions from single file (req 4)
// ============================================================================

void ApiLoader::ensure_all_utility_functions() {
    if (all_utility_functions_loaded_) return;

    String path = base_dir_ + "/" + FILE_UTILITY_FUNCTIONS;
    LocalVector<ApiUtilityFunction> funcs;
    Error err = ApiStoreReader::read_utility_functions(path, funcs);
    all_utility_functions_loaded_ = true; // 防止无用的尝试加载，等待重新生成清除缓存

    ERR_FAIL_COND_MSG(err, "[API Tool] load utility functions failed: " + UtilityFunctions::error_string(err));

    for (int i = 0; i < funcs.size(); i++) {
        const StringName &name = funcs[i].method.name;
        if (!utility_function_cache_.name_to_index.has(name)) {
            utility_function_cache_.insert(name, funcs[i]);
        }
    }
}

// ============================================================================
// Load all singletons from single file
// ============================================================================

void ApiLoader::ensure_all_singletons() {
    if (all_singletons_loaded_) return;

    String path = base_dir_ + "/" + DIR_SINGLETONS + "/singletons" + FILE_EXT_DATA;
    LocalVector<ApiSingleton> singletons;
    Error err = ApiStoreReader::read_singletons(path, singletons);
    ERR_FAIL_COND_MSG(err, "[API Tool] load singletons failed: " + UtilityFunctions::error_string(err));

    for (int i = 0; i < singletons.size(); i++) {
        const StringName &name = singletons[i].name;
        if (!singleton_cache_.name_to_index.has(name)) {
            singleton_cache_.insert(name, singletons[i]);
        }
    }
    all_singletons_loaded_ = true;
}

// ============================================================================
// Load all native structures from single file
// ============================================================================

void ApiLoader::ensure_all_native_structures() {
    if (all_native_structures_loaded_) return;

    String path = base_dir_ + "/" + DIR_NATIVE_STRUCTURES + "/native_structures" + FILE_EXT_DATA;
    LocalVector<ApiNativeStructure> structs;
    Error err = ApiStoreReader::read_native_structures(path, structs);
    ERR_FAIL_COND_MSG(err, "[API Tool] load native structures failed: " + UtilityFunctions::error_string(err));
    for (int i = 0; i < structs.size(); i++) {
        const String &name = structs[i].name;
        StringName sn(name);
        if (!native_structure_cache_.name_to_index.has(sn)) {
            native_structure_cache_.insert(sn, structs[i]);
        }
    }
    all_native_structures_loaded_ = true;
}

// ============================================================================
// ensure_*: check cache -> load -> cache -> return
// ============================================================================

const ApiUtilityFunction *ApiLoader::ensure_utility_function(const StringName &p_name) {
    ensure_all_utility_functions();
    return utility_function_cache_.find(p_name);
}

const ApiBuiltinClass *ApiLoader::ensure_builtin_class(const StringName &p_name) {
    const ApiBuiltinClass *cached = builtin_class_cache_.find(p_name);
    if (cached) return cached;

    String path = base_dir_ + "/" + String(DIR_BUILTIN_CLASSES) + "/" + String(p_name) + FILE_EXT_DATA;
    ApiBuiltinClass data;
    Error err = ApiStoreReader::read_builtin_class(path, data);
    ERR_FAIL_COND_V_MSG(err, nullptr, vformat("[API Tool] load builtin class %s failed: %s", p_name, UtilityFunctions::error_string(err)));

    builtin_class_cache_.insert(p_name, data);
    return builtin_class_cache_.find(p_name);
}

const ApiClass *ApiLoader::ensure_class(const StringName &p_name) {
    const ApiClass *cached = class_cache_.find(p_name);
    if (cached) return cached;

    String path = base_dir_ + "/" + DIR_CLASSES + "/" + String(p_name) + FILE_EXT_DATA;
    ApiClass data;
    Error err = ApiStoreReader::read_class(path, data);
    ERR_FAIL_COND_V_MSG(err, nullptr, vformat("[API Tool] load class %s failed: %s", p_name, UtilityFunctions::error_string(err)));

    class_cache_.insert(p_name, data);
    return class_cache_.find(p_name);
}

const ApiEnumInfo *ApiLoader::ensure_global_enum(const StringName &p_name) {
    const ApiEnumInfo *cached = enum_cache_.find(p_name);
    if (cached) return cached;

    String path = base_dir_ + "/" + DIR_CONSTANTS + "/" + String(p_name) + FILE_EXT_DATA;
    ApiEnumInfo data;
    Error err = ApiStoreReader::read_global_enum(path, data);
    ERR_FAIL_COND_V_MSG(err, nullptr, vformat("[API Tool] load global enum %s failed: %s", p_name, UtilityFunctions::error_string(err)));

    enum_cache_.insert(p_name, data);
    return enum_cache_.find(p_name);
}

const ApiConstantInfo *ApiLoader::ensure_global_constant(const StringName &p_name) {
    const ApiConstantInfo *cached = constant_cache_.find(p_name);
    if (cached) return cached;

    String path = base_dir_ + "/" + DIR_CONSTANTS + "/" + String(p_name) + FILE_EXT_DATA;
    ApiConstantInfo data;
    Error err = ApiStoreReader::read_global_constant(path, data);
    ERR_FAIL_COND_V_MSG(err, nullptr, vformat("[API Tool] load global constant %s failed: %s", p_name, UtilityFunctions::error_string(err)));

    constant_cache_.insert(p_name, data);
    return constant_cache_.find(p_name);
}

const ApiSingleton *ApiLoader::ensure_singleton(const StringName &p_name) {
    ensure_all_singletons();
    return singleton_cache_.find(p_name);
}

const ApiNativeStructure *ApiLoader::ensure_native_structure(const StringName &p_name) {
    ensure_all_native_structures();
    return native_structure_cache_.find(p_name);
}

#ifdef TOOLS_ENABLED
// ============================================================================
// Document queries (no cache, direct file read, TOOLS_ENABLED only)
// ============================================================================

std::unique_ptr<ApiClassDocument> ApiLoader::find_document(const StringName &p_name) {
#ifdef TOOLS_ENABLED
    // Try class first
    String path = base_dir_ + "/" + String(DIR_DOC_CLASSES) + "/" + String(p_name) + String(FILE_EXT_DOC);
    auto doc = std::make_unique<ApiClassDocument>();
    Error err = ApiStoreReader::read_document(path, *doc);
    if (err == OK) {
        return doc;
    }
    // Try builtin class
    path = base_dir_ + "/" + String(DIR_DOC_BUILTIN_CLASSES) + "/" + String(p_name) + String(FILE_EXT_DOC);
    err = ApiStoreReader::read_document(path, *doc);
    if (err == OK) {
        return doc;
    }
    return nullptr;
#else
    return nullptr;
#endif
}

std::unique_ptr<ApiUtilityFunctionDocument> ApiLoader::find_utility_function_document(const StringName &p_name) {
#ifdef TOOLS_ENABLED
    String path = base_dir_ + "/" + String(DIR_DOC_UTILITY_FUNCTIONS) + "/" + String(p_name) + String(FILE_EXT_DOC);
    auto doc = std::make_unique<ApiUtilityFunctionDocument>();
    Error err = ApiStoreReader::read_utility_function_document(path, *doc);
    if (err != OK) return nullptr;
    return doc;
#else
    return nullptr;
#endif
}

std::unique_ptr<ApiGlobalEnumDocument> ApiLoader::find_global_enum_document(const StringName &p_name) {
#ifdef TOOLS_ENABLED
    String path = base_dir_ + "/" + String(DIR_DOC_GLOBAL_ENUMS) + "/" + String(p_name) + String(FILE_EXT_DOC);
    auto doc = std::make_unique<ApiGlobalEnumDocument>();
    Error err = ApiStoreReader::read_global_enum_document(path, *doc);
    if (err != OK) return nullptr;
    return doc;
#else
    return nullptr;
#endif
}

std::unique_ptr<ApiGlobalConstantDocument> ApiLoader::find_global_constant_document(const StringName &p_name) {
#ifdef TOOLS_ENABLED
    String path = base_dir_ + "/" + String(DIR_DOC_GLOBAL_CONSTANTS) + "/" + String(p_name) + String(FILE_EXT_DOC);
    auto doc = std::make_unique<ApiGlobalConstantDocument>();
    Error err = ApiStoreReader::read_global_constant_document(path, *doc);
    if (err != OK) return nullptr;
    return doc;
#else
    return nullptr;
#endif
}
#endif // TOOLS_ENABLED

// ============================================================================
// Public query interfaces (shared_lock for reads, unique_lock for cache misses)
// ============================================================================

const ApiUtilityFunction *ApiLoader::get_utility_function(const StringName &p_name) {
    // First try shared lock (concurrent read)
    {
        std::shared_lock lock(mutex_);
        const ApiUtilityFunction *result = utility_function_cache_.find(p_name);
        if (result) return result;
        if (all_utility_functions_loaded_) return nullptr;
    }
    // Need exclusive lock to load
    std::unique_lock lock(mutex_);
    return ensure_utility_function(p_name);
}

const ApiBuiltinClass *ApiLoader::get_builtin_class(const StringName &p_name) {
    {
        std::shared_lock lock(mutex_);
        const ApiBuiltinClass *result = builtin_class_cache_.find(p_name);
        if (result) return result;
    }
    std::unique_lock lock(mutex_);
    return ensure_builtin_class(p_name);
}

const ApiClass *ApiLoader::get_class(const StringName &p_name) {
    {
        std::shared_lock lock(mutex_);
        const ApiClass *result = class_cache_.find(p_name);
        if (result) return result;
    }
    std::unique_lock lock(mutex_);
    return ensure_class(p_name);
}

const ApiEnumInfo *ApiLoader::get_global_enum(const StringName &p_name) {
    {
        std::shared_lock lock(mutex_);
        const ApiEnumInfo *result = enum_cache_.find(p_name);
        if (result) return result;
    }
    std::unique_lock lock(mutex_);
    return ensure_global_enum(p_name);
}

const ApiConstantInfo *ApiLoader::get_global_constant(const StringName &p_name) {
    {
        std::shared_lock lock(mutex_);
        const ApiConstantInfo *result = constant_cache_.find(p_name);
        if (result) return result;
    }
    std::unique_lock lock(mutex_);
    return ensure_global_constant(p_name);
}

const ApiSingleton *ApiLoader::get_singleton(const StringName &p_name) {
    {
        std::shared_lock lock(mutex_);
        const ApiSingleton *result = singleton_cache_.find(p_name);
        if (result) return result;
        if (all_singletons_loaded_) return nullptr;
    }
    std::unique_lock lock(mutex_);
    return ensure_singleton(p_name);
}

const ApiNativeStructure *ApiLoader::get_native_structure(const StringName &p_name) {
    {
        std::shared_lock lock(mutex_);
        const ApiNativeStructure *result = native_structure_cache_.find(p_name);
        if (result) return result;
        if (all_native_structures_loaded_) return nullptr;
    }
    std::unique_lock lock(mutex_);
    return ensure_native_structure(p_name);
}

// ============================================================================
// List interfaces (shared_lock for concurrent reads)
// ============================================================================

PackedStringArray ApiLoader::list_utility_functions() {
    std::shared_lock lock(mutex_);
    ensure_all_utility_functions();
    PackedStringArray result;
    for (const auto &item : utility_function_cache_.items) {
        result.append(String(item.method.name));
    }
    return result;
}

godot::PackedStringArray ApiLoader::list_builtin_classes() {
    std::shared_lock lock(mutex_);
    return list_files_in_dir(String(DIR_BUILTIN_CLASSES));
}

PackedStringArray ApiLoader::list_classes() {
    std::shared_lock lock(mutex_);
    return list_files_in_dir(DIR_CLASSES);
}

PackedStringArray ApiLoader::list_global_enums() {
    std::shared_lock lock(mutex_);
    return list_files_in_dir(DIR_CONSTANTS);
}

PackedStringArray ApiLoader::list_global_constants() {
    std::shared_lock lock(mutex_);
    return list_files_in_dir(DIR_CONSTANTS);
}

PackedStringArray ApiLoader::list_singletons() {
    std::shared_lock lock(mutex_);
    ensure_all_singletons();
    PackedStringArray result;
    for (const auto &item : singleton_cache_.items) {
        result.append(String(item.name));
    }
    return result;
}

PackedStringArray ApiLoader::list_native_structures() {
    std::shared_lock lock(mutex_);
    ensure_all_native_structures();
    PackedStringArray result;
    for (const auto &item : native_structure_cache_.items) {
        result.append(item.name);
    }
    return result;
}

// ============================================================================
// Count queries (no lock needed for directory scans)
// ============================================================================

int32_t ApiLoader::get_utility_function_count() {
    std::shared_lock lock(mutex_);
    if (all_utility_functions_loaded_) {
        return utility_function_cache_.size();
    }
    // Count from file if not loaded yet
    String path = base_dir_ + "/" + FILE_UTILITY_FUNCTIONS;
    if (FileAccess::file_exists(path)) {
        // Read count from file header
        Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
        if (f.is_valid()) {
            f->get_32(); // magic
            f->get_32(); // version
            f->get_32(); // flags
            return static_cast<int32_t>(f->get_32()); // first u32 of payload = count
        }
    }
    return 0;
}

int32_t ApiLoader::get_builtin_class_count() {
    std::shared_lock lock(mutex_);
    return list_files_in_dir(String(DIR_BUILTIN_CLASSES)).size();
}

int32_t ApiLoader::get_class_count() {
    std::shared_lock lock(mutex_);
    return list_files_in_dir(DIR_CLASSES).size();
}

int32_t ApiLoader::get_global_enum_count() {
    std::shared_lock lock(mutex_);
    return list_files_in_dir(DIR_CONSTANTS).size();
}

int32_t ApiLoader::get_global_constant_count() {
    std::shared_lock lock(mutex_);
    return list_files_in_dir(DIR_CONSTANTS).size();
}

} // namespace api_tool
