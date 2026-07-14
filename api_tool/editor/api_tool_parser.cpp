#ifdef TOOLS_ENABLED

// editor/api_tool_parser.cpp
// JSON parsing implementation (TOOLS_ENABLED only).
// Parses extension_api.json using godot-cpp PropertyInfo/MethodInfo types.
// All functions return Error with proper error messages.

#include "api_tool_parser.h"
#include "api_tool_store_writer.h"
#include "../api_tool_types.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace api_tool {

// ============================================================================
// JSON helpers: safe field access (inline wrappers for readability)
// ============================================================================

static inline bool dict_has(const Dictionary &d, const String &key) {
    return d.has(key);
}

static inline Variant dict_get(const Dictionary &d, const String &key, const Variant &def = Variant()) {
    return d.has(key) ? d[key] : def;
}

static inline StringName dict_get_string_name(const Dictionary &d, const String &key) {
    return StringName(d[key]);
}

// ============================================================================
// Sub-structure parsers: JSON -> godot-cpp PropertyInfo/MethodInfo
// ============================================================================

static PropertyInfo parse_property_info(const Dictionary &d) {
    PropertyInfo pi;
    StringName type_str = dict_get_string_name(d, "type");
    pi.type = parse_variant_type(String(type_str));
    pi.name = dict_get_string_name(d, "name");
    if (is_object_type(type_str) || pi.type == Variant::OBJECT) {
        pi.class_name = type_str;
    }
    if (dict_has(d, "property")) {
        Dictionary prop = d["property"];
        pi.hint = dict_has(prop, "hint") ? uint32_t(int32_t(prop["hint"])) : PROPERTY_HINT_NONE;
        pi.hint_string = dict_has(prop, "hint_string") ? String(prop["hint_string"]) : "";
        pi.usage = dict_has(prop, "usage") ? uint32_t(int64_t(prop["usage"])) : PROPERTY_USAGE_DEFAULT;
    }
    return pi;
}

static ApiMethodInfo parse_method(const Dictionary &d) {
    ApiMethodInfo ami;
    ami.method.name = dict_get_string_name(d, "name");

    // Build flags from JSON booleans
    uint32_t flags = GDEXTENSION_METHOD_FLAG_NORMAL;
    if (dict_has(d, "is_const") && bool(d["is_const"])) flags |= GDEXTENSION_METHOD_FLAG_CONST;
    if (dict_has(d, "is_vararg") && bool(d["is_vararg"])) flags |= GDEXTENSION_METHOD_FLAG_VARARG;
    if (dict_has(d, "is_static") && bool(d["is_static"])) flags |= GDEXTENSION_METHOD_FLAG_STATIC;
    if (dict_has(d, "is_virtual") && bool(d["is_virtual"])) flags |= GDEXTENSION_METHOD_FLAG_VIRTUAL;
    if (dict_has(d, "is_required") && bool(d["is_required"])) flags |= GDEXTENSION_METHOD_FLAG_VIRTUAL_REQUIRED;
    ami.method.flags = flags;

    ami.hash = dict_has(d, "hash") ? int64_t(d["hash"]) : 0;

    if (dict_has(d, "hash_compatibility")) {
        Array compat = d["hash_compatibility"];
        ami.hash_compatibility.reserve(compat.size());
        for (int i = 0; i < compat.size(); i++) {
            ami.hash_compatibility.push_back(int64_t(compat[i]));
        }
    }

    // return_val
    if (dict_has(d, "return_value")) {
        Dictionary rv = d["return_value"];
        ami.method.return_val = parse_property_info(rv);
        if (dict_has(rv, "meta")) {
            ami.method.return_val_metadata = parse_argument_metadata(dict_get_string_name(rv, "meta"));
        }
    }

    // arguments
    if (dict_has(d, "arguments")) {
        Array args = d["arguments"];
        ami.method.arguments.reserve(args.size());
        ami.method.arguments_metadata.reserve(args.size());
        for (int i = 0; i < args.size(); i++) {
            Dictionary ad = args[i];
            ami.method.arguments.push_back(parse_property_info(ad));
            if (dict_has(ad, "meta")) {
                ami.method.arguments_metadata.push_back(parse_argument_metadata(dict_get_string_name(ad, "meta")));
            } else {
                ami.method.arguments_metadata.push_back(GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE);
            }
        }
    }
    return ami;
}

static ApiEnumInfo parse_enum(const Dictionary &d) {
    ApiEnumInfo info;
    info.name = dict_get_string_name(d, "name");
    info.is_bitfield = dict_has(d, "is_bitfield") ? bool(d["is_bitfield"]) : false;
    if (dict_has(d, "values")) {
        Array values = d["values"];
        info.values.reserve(values.size());
        for (int i = 0; i < values.size(); i++) {
            Dictionary ev = values[i];
            ApiEnumValue v;
            v.name = dict_get_string_name(ev, "name");
            v.value = int64_t(ev["value"]);
            info.values.push_back(v);
        }
    }
    return info;
}

static ApiSignalInfo parse_signal(const Dictionary &d) {
    ApiSignalInfo info;
    info.name = dict_get_string_name(d, "name");
    if (dict_has(d, "arguments")) {
        Array args = d["arguments"];
        info.arguments.reserve(args.size());
        for (int i = 0; i < args.size(); i++) {
            info.arguments.push_back(parse_property_info(Dictionary(args[i])));
        }
    }
    return info;
}

static ApiPropertyInfo parse_api_property(const Dictionary &d) {
    ApiPropertyInfo info;
    info.property = parse_property_info(d);
    info.setter = dict_get_string_name(d, "setter");
    info.getter = dict_get_string_name(d, "getter");
    return info;
}

static ApiOperatorInfo parse_operator(const Dictionary &d) {
    ApiOperatorInfo info;
    info.name = dict_get_string_name(d, "name");
    info.return_type = parse_variant_type(String(dict_get_string_name(d, "return_type")));
    if (dict_has(d, "left_type")) {
        info.left_type = parse_variant_type(String(dict_get_string_name(d, "left_type")));
    }
    if (dict_has(d, "right_type")) {
        info.right_type = parse_variant_type(String(dict_get_string_name(d, "right_type")));
    }
    return info;
}

static ApiConstructorInfo parse_constructor(const Dictionary &d) {
    ApiConstructorInfo info;
    if (dict_has(d, "arguments")) {
        Array args = d["arguments"];
        info.arguments.reserve(args.size());
        for (int i = 0; i < args.size(); i++) {
            info.arguments.push_back(parse_property_info(Dictionary(args[i])));
        }
    }
    return info;
}

static ApiMemberInfo parse_member(const Dictionary &d) {
    ApiMemberInfo info;
    info.name = dict_get_string_name(d, "name");
    info.type = parse_variant_type(String(dict_get_string_name(d, "type")));
    return info;
}

// ============================================================================
// Directory preparation
// ============================================================================

Error ApiParser::prepare_output_dirs(const String &p_output_dir) {
    DirAccess::make_dir_recursive_absolute(p_output_dir);

    const char *subdirs[] = {
        DIR_UTILITY_FUNCTIONS,
        DIR_BUILTIN_CLASSES,
        DIR_CLASSES,
        DIR_CONSTANTS,
        DIR_SINGLETONS,
        DIR_NATIVE_STRUCTURES,
        // Document subdirectories (sharded by entity type)
        DIR_DOC_CLASSES,
        DIR_DOC_BUILTIN_CLASSES,
        DIR_DOC_UTILITY_FUNCTIONS,
        DIR_DOC_GLOBAL_ENUMS,
        DIR_DOC_GLOBAL_CONSTANTS,
    };
    for (const char *subdir : subdirs) {
        String path = p_output_dir + String("/") + subdir;
        if (DirAccess::dir_exists_absolute(path)) {
            PackedStringArray files = DirAccess::get_files_at(path);
            for (int i = 0; i < files.size(); i++) {
                DirAccess::remove_absolute(path + String("/") + files[i]);
            }
        }
        DirAccess::make_dir_recursive_absolute(path);
    }

    return OK;
}

// ============================================================================
// Header parser
// ============================================================================

Error ApiParser::parse_and_write_header(const Dictionary &p_root, const String &p_output_dir) {
    ERR_FAIL_COND_V_MSG(!dict_has(p_root, "header"), ERR_PARSE_ERROR, "[API Tool] JSON missing 'header' section");

    Dictionary hdr = p_root["header"];
    ApiHeader header;
    header.version_major = int32_t(dict_get(hdr, "version_major", 0));
    header.version_minor = int32_t(dict_get(hdr, "version_minor", 0));
    header.version_patch = int32_t(dict_get(hdr, "version_patch", 0));
    header.version_status = String(dict_get(hdr, "version_status", String()));
    header.version_build = String(dict_get(hdr, "version_build", String()));
    header.version_full_name = String(dict_get(hdr, "version_full_name", String()));
    header.precision = String(dict_get(hdr, "precision", String()));

    String path = p_output_dir + String("/") + String(FILE_HEADER);
    return ApiStoreWriter::write_header(path, header);
}

// ============================================================================
// Utility Functions parser (single file)
// ============================================================================

Error ApiParser::parse_and_write_utility_functions(const Dictionary &p_root, const String &p_output_dir) {
    ERR_FAIL_COND_V_MSG(!dict_has(p_root, "utility_functions"), ERR_PARSE_ERROR, "[API Tool] JSON missing 'utility_functions' section");

    Array funcs = p_root["utility_functions"];
    LocalVector<ApiUtilityFunction> all_funcs;
    all_funcs.reserve(funcs.size());

    String doc_dir = p_output_dir + String("/") + String(DIR_DOC_UTILITY_FUNCTIONS);

    for (int i = 0; i < funcs.size(); i++) {
        Dictionary fd = funcs[i];
        ApiUtilityFunction func;
        func.method.name = dict_get_string_name(fd, "name");

        // Build flags
        uint32_t flags = GDEXTENSION_METHOD_FLAG_NORMAL;
        if (dict_has(fd, "is_vararg") && bool(fd["is_vararg"])) flags |= GDEXTENSION_METHOD_FLAG_VARARG;
        func.method.flags = flags;

        func.hash = dict_has(fd, "hash") ? int64_t(fd["hash"]) : 0;
        func.category = dict_get_string_name(fd, "category");

        // return_type -> return_val PropertyInfo
        if (dict_has(fd, "return_type")) {
            StringName ret_type = dict_get_string_name(fd, "return_type");
            func.method.return_val.type = parse_variant_type(String(ret_type));
            if (is_object_type(ret_type)) {
                func.method.return_val.class_name = ret_type;
            }
        }

        if (dict_has(fd, "arguments")) {
            Array args = fd["arguments"];
            func.method.arguments.reserve(args.size());
            func.method.arguments_metadata.reserve(args.size());
            for (int j = 0; j < args.size(); j++) {
                Dictionary ad = args[j];
                func.method.arguments.push_back(parse_property_info(ad));
                if (dict_has(ad, "meta")) {
                    func.method.arguments_metadata.push_back(parse_argument_metadata(dict_get_string_name(ad, "meta")));
                } else {
                    func.method.arguments_metadata.push_back(GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE);
                }
            }
        }

        all_funcs.push_back(func);

        // Write document file (single pass, no separate document parsing)
        ApiUtilityFunctionDocument doc;
        doc.name = String(func.method.name);
        if (dict_has(fd, "description")) {
            doc.description = String(fd["description"]);
        }
        String doc_path = doc_dir + String("/") + String(func.method.name) + String(FILE_EXT_DOC);
        ApiStoreWriter::write_utility_function_document(doc_path, doc);
    }

    String path = p_output_dir + String("/") + String(FILE_UTILITY_FUNCTIONS);
    return ApiStoreWriter::write_utility_functions(path, all_funcs);
}

// ============================================================================
// Builtin Types parser
// ============================================================================

Error ApiParser::parse_and_write_builtin_classes(const Dictionary &p_root, const String &p_output_dir) {
    ERR_FAIL_COND_V_MSG(!dict_has(p_root, "builtin_classes"), ERR_PARSE_ERROR, "[API Tool] JSON missing 'builtin_classes' section");

    Array classes = p_root["builtin_classes"];
    String dir = p_output_dir + String("/") + String(DIR_BUILTIN_CLASSES);
    String doc_dir = p_output_dir + String("/") + String(DIR_DOC_BUILTIN_CLASSES);
    Error overall = OK;

    for (int i = 0; i < classes.size(); i++) {
        Dictionary cd = classes[i];
        ApiBuiltinClass bt;
        bt.name = dict_get_string_name(cd, "name");
        bt.variant_type = parse_variant_type(String(bt.name));

        if (dict_has(cd, "indexing_return_type")) {
            bt.has_indexing_return_type = true;
            bt.indexing_type = parse_variant_type(String(dict_get_string_name(cd, "indexing_return_type")));
        }
        bt.is_keyed = dict_has(cd, "is_keyed") ? bool(cd["is_keyed"]) : false;
        bt.has_destructor = dict_has(cd, "has_destructor") ? bool(cd["has_destructor"]) : false;

        // Build document in parallel
        ApiBuiltinClassDocument doc;
        doc.name = String(bt.name);
        if (dict_has(cd, "description")) {
            doc.description = String(cd["description"]);
        }
        if (dict_has(cd, "brief_description")) {
            doc.brief_description = String(cd["brief_description"]);
        }

        if (dict_has(cd, "members")) {
            Array members = cd["members"];
            bt.members.reserve(members.size());
            doc.members.reserve(members.size());
            for (int j = 0; j < members.size(); j++) {
                Dictionary md = members[j];
                bt.members.push_back(parse_member(md));
                ApiMemberDocument mdoc;
                mdoc.name = String(dict_get_string_name(md, "name"));
                if (dict_has(md, "description")) {
                    mdoc.description = String(md["description"]);
                }
                doc.members.push_back(mdoc);
            }
        }

        if (dict_has(cd, "constants")) {
            Array constants = cd["constants"];
            bt.constants.reserve(constants.size());
            doc.constants.reserve(constants.size());
            for (int j = 0; j < constants.size(); j++) {
                Dictionary c = constants[j];
                ApiConstantInfo ci;
                ci.name = dict_get_string_name(c, "name");
                ci.value = dict_has(c, "value") ? int64_t(c["value"]) : 0;
                bt.constants.push_back(ci);
                ApiConstantDocument cdoc;
                cdoc.name = String(ci.name);
                if (dict_has(c, "description")) {
                    cdoc.description = String(c["description"]);
                }
                doc.constants.push_back(cdoc);
            }
        }

        if (dict_has(cd, "enums")) {
            Array enums = cd["enums"];
            bt.enums.reserve(enums.size());
            for (int j = 0; j < enums.size(); j++) {
                Dictionary ed = enums[j];
                bt.enums.push_back(parse_enum(ed));
                // Build enum document
                ApiEnumDocument edoc;
                edoc.name = String(dict_get_string_name(ed, "name"));
                if (dict_has(ed, "values")) {
                    Array values = ed["values"];
                    edoc.values.reserve(values.size());
                    for (int k = 0; k < values.size(); k++) {
                        Dictionary ev = values[k];
                        ApiEnumValueDocument evdoc;
                        evdoc.name = String(dict_get_string_name(ev, "name"));
                        if (dict_has(ev, "description")) {
                            evdoc.description = String(ev["description"]);
                        }
                        edoc.values.push_back(evdoc);
                    }
                }
                doc.enums.push_back(edoc);
            }
        }

        if (dict_has(cd, "methods")) {
            Array methods = cd["methods"];
            bt.methods.reserve(methods.size());
            doc.methods.reserve(methods.size());
            for (int j = 0; j < methods.size(); j++) {
                Dictionary md = methods[j];
                bt.methods.push_back(parse_method(md));
                ApiMethodDocument mdoc;
                mdoc.name = String(dict_get_string_name(md, "name"));
                if (dict_has(md, "description")) {
                    mdoc.description = String(md["description"]);
                }
                doc.methods.push_back(mdoc);
            }
        }

        if (dict_has(cd, "operators")) {
            Array operators = cd["operators"];
            bt.operators.reserve(operators.size());
            doc.operators.reserve(operators.size());
            for (int j = 0; j < operators.size(); j++) {
                Dictionary od = operators[j];
                bt.operators.push_back(parse_operator(od));
                ApiOperatorDocument odoc;
                odoc.name = String(dict_get_string_name(od, "name"));
                if (dict_has(od, "description")) {
                    odoc.description = String(od["description"]);
                }
                doc.operators.push_back(odoc);
            }
        }

        if (dict_has(cd, "constructors")) {
            Array constructors = cd["constructors"];
            bt.constructors.reserve(constructors.size());
            doc.constructors.reserve(constructors.size());
            for (int j = 0; j < constructors.size(); j++) {
                Dictionary ctor_d = constructors[j];
                bt.constructors.push_back(parse_constructor(ctor_d));
                ApiConstructorDocument cdoc;
                cdoc.index = j;
                if (dict_has(ctor_d, "description")) {
                    cdoc.description = String(ctor_d["description"]);
                }
                doc.constructors.push_back(cdoc);
            }
        }

        // Write main data file
        String path = dir + String("/") + String(bt.name) + String(FILE_EXT_DATA);
        Error err = ApiStoreWriter::write_builtin_class(path, bt);
        if (err != OK) {
            ERR_PRINT("[API Tool] Failed to write builtin type: " + String(bt.name));
            overall = err;
        }

        // Write document file
        String doc_path = doc_dir + String("/") + String(bt.name) + String(FILE_EXT_DOC);
        ApiStoreWriter::write_builtin_class_document(doc_path, doc);
    }

    return overall;
}

// ============================================================================
// Classes parser
// ============================================================================

Error ApiParser::parse_and_write_classes(const Dictionary &p_root, const String &p_output_dir) {
    ERR_FAIL_COND_V_MSG(!dict_has(p_root, "classes"), ERR_PARSE_ERROR, "[API Tool] JSON missing 'classes' section");

    Array classes = p_root["classes"];
    String dir = p_output_dir + String("/") + String(DIR_CLASSES);
    String doc_dir = p_output_dir + String("/") + String(DIR_DOC_CLASSES);
    Error overall = OK;

    for (int i = 0; i < classes.size(); i++) {
        Dictionary cd = classes[i];
        ApiClass cls;
        cls.name = dict_get_string_name(cd, "name");
        cls.inherits = dict_get_string_name(cd, "inherits");
        cls.api_type = dict_get_string_name(cd, "api_type");
        cls.is_refcounted = dict_has(cd, "is_refcounted") ? bool(cd["is_refcounted"]) : false;
        cls.is_instantiable = dict_has(cd, "is_instantiable") ? bool(cd["is_instantiable"]) : true;

        // Build document in parallel
        ApiClassDocument doc;
        doc.name = String(cls.name);
        if (dict_has(cd, "description")) {
            doc.description = String(cd["description"]);
        }
        if (dict_has(cd, "brief_description")) {
            doc.brief_description = String(cd["brief_description"]);
        }

        if (dict_has(cd, "methods")) {
            Array methods = cd["methods"];
            cls.methods.reserve(methods.size());
            doc.methods.reserve(methods.size());
            for (int j = 0; j < methods.size(); j++) {
                Dictionary md = methods[j];
                cls.methods.push_back(parse_method(md));
                ApiMethodDocument mdoc;
                mdoc.name = String(dict_get_string_name(md, "name"));
                if (dict_has(md, "description")) {
                    mdoc.description = String(md["description"]);
                }
                doc.methods.push_back(mdoc);
            }
        }

        if (dict_has(cd, "signals")) {
            Array signals = cd["signals"];
            cls.signals.reserve(signals.size());
            doc.signals.reserve(signals.size());
            for (int j = 0; j < signals.size(); j++) {
                Dictionary sd = signals[j];
                cls.signals.push_back(parse_signal(sd));
                ApiSignalDocument sdoc;
                sdoc.name = String(dict_get_string_name(sd, "name"));
                if (dict_has(sd, "description")) {
                    sdoc.description = String(sd["description"]);
                }
                if (dict_has(sd, "arguments")) {
                    Array args = sd["arguments"];
                    sdoc.arguments.reserve(args.size());
                    for (int k = 0; k < args.size(); k++) {
                        sdoc.arguments.push_back(parse_property_info(Dictionary(args[k])));
                    }
                }
                doc.signals.push_back(sdoc);
            }
        }

        if (dict_has(cd, "properties")) {
            Array properties = cd["properties"];
            cls.properties.reserve(properties.size());
            doc.properties.reserve(properties.size());
            for (int j = 0; j < properties.size(); j++) {
                Dictionary pd = properties[j];
                cls.properties.push_back(parse_api_property(pd));
                ApiPropertyDocument pdoc;
                pdoc.name = String(dict_get_string_name(pd, "name"));
                if (dict_has(pd, "description")) {
                    pdoc.description = String(pd["description"]);
                }
                doc.properties.push_back(pdoc);
            }
        }

        if (dict_has(cd, "enums")) {
            Array enums = cd["enums"];
            cls.enums.reserve(enums.size());
            doc.enums.reserve(enums.size());
            for (int j = 0; j < enums.size(); j++) {
                Dictionary ed = enums[j];
                cls.enums.push_back(parse_enum(ed));
                ApiEnumDocument edoc;
                edoc.name = String(dict_get_string_name(ed, "name"));
                if (dict_has(ed, "values")) {
                    Array values = ed["values"];
                    edoc.values.reserve(values.size());
                    for (int k = 0; k < values.size(); k++) {
                        Dictionary ev = values[k];
                        ApiEnumValueDocument evdoc;
                        evdoc.name = String(dict_get_string_name(ev, "name"));
                        if (dict_has(ev, "description")) {
                            evdoc.description = String(ev["description"]);
                        }
                        edoc.values.push_back(evdoc);
                    }
                }
                doc.enums.push_back(edoc);
            }
        }

        if (dict_has(cd, "constants")) {
            Array constants = cd["constants"];
            cls.constants.reserve(constants.size());
            for (int j = 0; j < constants.size(); j++) {
                Dictionary c = constants[j];
                ApiConstantInfo ci;
                ci.name = dict_get_string_name(c, "name");
                ci.value = dict_has(c, "value") ? int64_t(c["value"]) : 0;
                cls.constants.push_back(ci);
            }
        }

        // Write main data file
        String path = dir + String("/") + String(cls.name) + String(FILE_EXT_DATA);
        Error err = ApiStoreWriter::write_class(path, cls);
        if (err != OK) {
            ERR_PRINT("[API Tool] Failed to write class: " + String(cls.name));
            overall = err;
        }

        // Write document file
        String doc_path = doc_dir + String("/") + String(cls.name) + String(FILE_EXT_DOC);
        ApiStoreWriter::write_class_document(doc_path, doc);
    }

    return overall;
}

// ============================================================================
// Global Enums parser
// ============================================================================

Error ApiParser::parse_and_write_global_enums(const Dictionary &p_root, const String &p_output_dir) {
    ERR_FAIL_COND_V_MSG(!dict_has(p_root, "global_enums"), ERR_PARSE_ERROR, "[API Tool] JSON missing 'global_enums' section");

    Array enums = p_root["global_enums"];
    String dir = p_output_dir + String("/") + String(DIR_CONSTANTS);
    String doc_dir = p_output_dir + String("/") + String(DIR_DOC_GLOBAL_ENUMS);
    Error overall = OK;

    for (int i = 0; i < enums.size(); i++) {
        Dictionary ed = enums[i];
        ApiEnumInfo info = parse_enum(Dictionary(ed));

        // Write main data file
        String path = dir + String("/") + String(info.name) + String(FILE_EXT_DATA);
        Error err = ApiStoreWriter::write_global_enum(path, info);
        if (err != OK) {
            ERR_PRINT("[API Tool] Failed to write enum: " + String(info.name));
            overall = err;
        }

        // Write document file
        ApiGlobalEnumDocument doc;
        doc.name = String(info.name);
        if (dict_has(ed, "values")) {
            Array values = ed["values"];
            doc.values.reserve(values.size());
            for (int k = 0; k < values.size(); k++) {
                Dictionary ev = values[k];
                ApiEnumValueDocument evdoc;
                evdoc.name = String(dict_get_string_name(ev, "name"));
                if (dict_has(ev, "description")) {
                    evdoc.description = String(ev["description"]);
                }
                doc.values.push_back(evdoc);
            }
        }
        String doc_path = doc_dir + String("/") + String(info.name) + String(FILE_EXT_DOC);
        ApiStoreWriter::write_global_enum_document(doc_path, doc);
    }

    return overall;
}

// ============================================================================
// Global Constants parser
// ============================================================================

Error ApiParser::parse_and_write_global_constants(const Dictionary &p_root, const String &p_output_dir) {
    ERR_FAIL_COND_V_MSG(!dict_has(p_root, "global_constants"), ERR_PARSE_ERROR, "[API Tool] JSON missing 'global_constants' section");

    Array constants = p_root["global_constants"];
    String dir = p_output_dir + String("/") + String(DIR_CONSTANTS);
    String doc_dir = p_output_dir + String("/") + String(DIR_DOC_GLOBAL_CONSTANTS);
    Error overall = OK;

    for (int i = 0; i < constants.size(); i++) {
        Dictionary cd = constants[i];
        ApiConstantInfo info;
        info.name = dict_get_string_name(cd, "name");
        info.value = int64_t(cd["value"]);
        info.is_bitfield = dict_has(cd, "is_bitfield") ? bool(cd["is_bitfield"]) : false;

        // Write main data file
        String path = dir + String("/") + String(info.name) + String(FILE_EXT_DATA);
        Error err = ApiStoreWriter::write_global_constant(path, info);
        if (err != OK) {
            ERR_PRINT("[API Tool] Failed to write constant: " + String(info.name));
            overall = err;
        }

        // Write document file
        ApiGlobalConstantDocument doc;
        doc.name = String(info.name);
        if (dict_has(cd, "description")) {
            doc.description = String(cd["description"]);
        }
        String doc_path = doc_dir + String("/") + String(info.name) + String(FILE_EXT_DOC);
        ApiStoreWriter::write_global_constant_document(doc_path, doc);
    }

    return overall;
}

// ============================================================================
// Singletons parser (single file)
// ============================================================================

Error ApiParser::parse_and_write_singletons(const Dictionary &p_root, const String &p_output_dir) {
    ERR_FAIL_COND_V_MSG(!dict_has(p_root, "singletons"), ERR_PARSE_ERROR, "[API Tool] JSON missing 'singletons' section");

    Array singletons = p_root["singletons"];
    LocalVector<ApiSingleton> all_singletons;
    all_singletons.reserve(singletons.size());

    for (int i = 0; i < singletons.size(); i++) {
        Dictionary sd = singletons[i];
        ApiSingleton singleton;
        singleton.name = dict_get_string_name(sd, "name");
        singleton.type = dict_get_string_name(sd, "type");
        singleton.user_created = dict_has(sd, "user_created") ? bool(sd["user_created"]) : false;
        singleton.editor_only = dict_has(sd, "editor_only") ? bool(sd["editor_only"]) : false;
        all_singletons.push_back(singleton);
    }

    String path = p_output_dir + String("/") + String(DIR_SINGLETONS) + String("/singletons") + String(FILE_EXT_DATA);
    return ApiStoreWriter::write_singletons(path, all_singletons);
}

// ============================================================================
// Native Structures parser (single file)
// ============================================================================

Error ApiParser::parse_and_write_native_structures(const Dictionary &p_root, const String &p_output_dir) {
    ERR_FAIL_COND_V_MSG(!dict_has(p_root, "native_structures"), ERR_PARSE_ERROR, "[API Tool] JSON missing 'native_structures' section");

    Array structs = p_root["native_structures"];
    LocalVector<ApiNativeStructure> all_structs;
    all_structs.reserve(structs.size());

    for (int i = 0; i < structs.size(); i++) {
        Dictionary sd = structs[i];
        ApiNativeStructure ns;
        ns.name = String(sd["name"]);
        ns.format = String(sd["format"]);
        all_structs.push_back(ns);
    }

    String path = p_output_dir + String("/") + String(DIR_NATIVE_STRUCTURES) + String("/native_structures") + String(FILE_EXT_DATA);
    return ApiStoreWriter::write_native_structures(path, all_structs);
}

// ============================================================================
// Main entry point
// ============================================================================

Error ApiParser::generate(const Dictionary &p_json_root, const String &p_output_dir) {
    Error err = prepare_output_dirs(p_output_dir);
    ERR_FAIL_COND_V_MSG(err, err, "[API Tool] " + UtilityFunctions::error_string(err) + ": Failed to prepare output directories");

    err = parse_and_write_header(p_json_root, p_output_dir);
    if (err != OK) return err;

    err = parse_and_write_utility_functions(p_json_root, p_output_dir);
    if (err != OK) return err;

    err = parse_and_write_builtin_classes(p_json_root, p_output_dir);
    if (err != OK) return err;

    err = parse_and_write_classes(p_json_root, p_output_dir);
    if (err != OK) return err;

    err = parse_and_write_global_enums(p_json_root, p_output_dir);
    if (err != OK) return err;

    err = parse_and_write_global_constants(p_json_root, p_output_dir);
    if (err != OK) return err;

    err = parse_and_write_singletons(p_json_root, p_output_dir);
    if (err != OK) return err;

    err = parse_and_write_native_structures(p_json_root, p_output_dir);
    if (err != OK) return err;

    UtilityFunctions::print("[API Tool] API dump generated successfully to: " + p_output_dir);
    return OK;
}

} // namespace api_tool

#endif // TOOLS_ENABLED