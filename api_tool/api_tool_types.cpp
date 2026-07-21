#include "api_tool_types.h"
#include <godot_cpp/templates/hash_map.hpp>


using namespace godot;

namespace api_tool {

const godot::String &get_variant_operator_name(godot::Variant::Operator p_op) {
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