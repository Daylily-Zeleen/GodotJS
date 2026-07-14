#include "jsb_variant_util.h"
#include <godot_cpp/variant/array.hpp>


namespace jsb::internal
{
    Variant VariantUtil::structured_clone(const Variant& p_variant, ReferentialVariantMap<Variant>& p_clone_map, bool& r_valid, int p_recursion_count)
    {
        if (p_recursion_count == 0)
        {
            r_valid = true;
        }

        Variant* existing_clone = p_clone_map.getptr(p_variant);

        if (existing_clone)
        {
            return *existing_clone;
        }

        Variant clone;

        switch (p_variant.get_type())
        {
            case Variant::Type::OBJECT:
                ERR_PRINT("Structured clone cannot clone Godot Objects. Godot Objects must be transferred");
                r_valid = false;
                clone = Variant();
                break;
            case Variant::Type::DICTIONARY:
            {
                Dictionary original = p_variant;
                Dictionary dict_clone;
                dict_clone.set_typed(original.get_typed_key_builtin(), original.get_typed_key_class_name(), original.get_typed_key_script(),
                        original.get_typed_value_builtin(), original.get_typed_value_class_name(), original.get_typed_value_script());

                if (p_recursion_count > MAX_RECURSION)
                {
                    ERR_PRINT("Max recursion reached");
                    r_valid = false;
                    return dict_clone;
                }

                p_recursion_count++;

                // GDExtension: use Dictionary::keys() instead of get_key_list()
                {
                    Array keys = original.keys();
                    for (int idx = 0; idx < keys.size(); idx++)
                    {
                        const Variant& key = keys[idx];
                        dict_clone[structured_clone(key, p_clone_map, r_valid, p_recursion_count)] =
                            structured_clone(original[key], p_clone_map, r_valid, p_recursion_count);
                    }
                }

                clone = dict_clone;
                break;
            }
            case Variant::Type::ARRAY:
            {
                Array original = p_variant;
                Array arr_clone;
                arr_clone.set_typed(original.get_typed_builtin(), original.get_typed_class_name(), original.get_typed_script());

                if (p_recursion_count > MAX_RECURSION)
                {
                    ERR_PRINT("Max recursion reached");
                    r_valid = false;
                    return arr_clone;
                }

                p_recursion_count++;

                int element_count = original.size();
                arr_clone.resize(element_count);

                for (int i = 0; i < element_count; i++)
                {
                    arr_clone.set(i, structured_clone(original.get(i), p_clone_map, r_valid, p_recursion_count));
                }

                clone = arr_clone;
                break;
            }
            case Variant::Type::PACKED_BYTE_ARRAY:
                clone = p_variant.operator PackedByteArray().duplicate();
                break;
            case Variant::Type::PACKED_INT32_ARRAY:
                clone = p_variant.operator PackedInt32Array().duplicate();
                break;
            case Variant::Type::PACKED_INT64_ARRAY:
                clone = p_variant.operator PackedInt64Array().duplicate();
                break;
            case Variant::Type::PACKED_FLOAT32_ARRAY:
                clone = p_variant.operator PackedFloat32Array().duplicate();
                break;
            case Variant::Type::PACKED_FLOAT64_ARRAY:
                clone = p_variant.operator PackedFloat64Array().duplicate();
                break;
            case Variant::Type::PACKED_STRING_ARRAY:
                clone = p_variant.operator PackedStringArray().duplicate();
                break;
            case Variant::Type::PACKED_VECTOR2_ARRAY:
                clone = p_variant.operator PackedVector2Array().duplicate();
                break;
            case Variant::Type::PACKED_VECTOR3_ARRAY:
                clone = p_variant.operator PackedVector3Array().duplicate();
                break;
            case Variant::Type::PACKED_COLOR_ARRAY:
                clone = p_variant.operator PackedColorArray().duplicate();
                break;
            case Variant::Type::PACKED_VECTOR4_ARRAY:
                clone = p_variant.operator PackedVector4Array().duplicate();
                break;
            default:
                clone = p_variant;
        }

        p_clone_map[p_variant] = clone;
        return clone;
    }

    const String &VariantUtil::get_variant_operator_name(Variant::Operator p_op) {
#define __VAR_OP_TO_TEXT(op) { Variant::Operator::op, (#op) }
        static const HashMap<Variant::Operator, String> search = {
            // comparison
            __VAR_OP_TO_TEXT(OP_EQUAL),
            __VAR_OP_TO_TEXT(OP_NOT_EQUAL),
            __VAR_OP_TO_TEXT(OP_LESS),
            __VAR_OP_TO_TEXT(OP_LESS_EQUAL),
            __VAR_OP_TO_TEXT(OP_GREATER),
            __VAR_OP_TO_TEXT(OP_GREATER_EQUAL),
            // mathematic
            __VAR_OP_TO_TEXT(OP_ADD),
            __VAR_OP_TO_TEXT(OP_SUBTRACT),
            __VAR_OP_TO_TEXT(OP_MULTIPLY),
            __VAR_OP_TO_TEXT(OP_DIVIDE),
            __VAR_OP_TO_TEXT(OP_NEGATE),
            __VAR_OP_TO_TEXT(OP_POSITIVE),
            __VAR_OP_TO_TEXT(OP_MODULE),
            __VAR_OP_TO_TEXT(OP_POWER),
            // bitwise
            __VAR_OP_TO_TEXT(OP_SHIFT_LEFT),
            __VAR_OP_TO_TEXT(OP_SHIFT_RIGHT),
            __VAR_OP_TO_TEXT(OP_BIT_AND),
            __VAR_OP_TO_TEXT(OP_BIT_OR),
            __VAR_OP_TO_TEXT(OP_BIT_XOR),
            __VAR_OP_TO_TEXT(OP_BIT_NEGATE),
            // logic
            __VAR_OP_TO_TEXT(OP_AND),
            __VAR_OP_TO_TEXT(OP_OR),
            __VAR_OP_TO_TEXT(OP_XOR),
            __VAR_OP_TO_TEXT(OP_NOT),
            // containment
            __VAR_OP_TO_TEXT(OP_IN),
            __VAR_OP_TO_TEXT(OP_MAX),
        };
        return search[p_op];
    }
}
