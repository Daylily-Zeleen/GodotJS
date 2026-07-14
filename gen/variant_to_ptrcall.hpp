/**
 * @file variant_to_ptrcall.hpp
 * @brief Helper templates to convert ValidatedXXX (Variant* params) to GDExtensionPtrXXX calls
 * @warning This file is auto-generated. Do not edit manually.
 * 
 * NOTE: This file should only be included in .cpp files, not in header files.
 * It uses jsb_stackalloc which requires specific includes.
 */

#ifndef GODOTJS_VARIANT_TO_PTRCALL_HPP
#define GODOTJS_VARIANT_TO_PTRCALL_HPP

#include "jsb_macros.h"
#include <cstdint>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/templates/vector.hpp>

namespace godot {

/**
 * @brief Helper to convert ValidatedXXX functions (Variant* params) to GDExtensionPtrXXX calls
 * 
 * These templates wrap GDExtensionPtrXXX function pointers with ValidatedXXX signatures.
 * The ValidatedXXX functions receive Variant* parameters and convert them to raw pointers
 * before calling the underlying GDExtensionPtrXXX functions.
 */

//================================================================================
// Type size and offset calculation utilities
//================================================================================

template <size_t Int>
constexpr static size_t sum() { return Int; }

template <size_t Int, size_t... Ints>
constexpr static size_t sum() { return Int + sum<Ints...>(); }

template <typename... Args, size_t... Is>
constexpr static size_t _type_size(IndexSequence<Is...>) { return sum<sizeof(Args[Is])...>; }

template <typename... Args>
constexpr static size_t type_size() { return _type_size<Args...>(BuildIndexSequence<sizeof...(Args)>{}); }

template <size_t DestIdx, typename T, size_t CurrIdx = 0>
constexpr static size_t type_size_before(size_t val) {
	if constexpr (DestIdx > CurrIdx) {
		return sizeof(T) + val;
	} else {
		return val;
	}
}

template <size_t DestIdx, typename T, typename... Args, size_t CurrIdx = 0>
constexpr static size_t type_size_before(size_t val = 0) {
	if constexpr (DestIdx > CurrIdx) {
		return type_size_before<DestIdx, Args..., CurrIdx + 1>(val + sizeof(T));
	} else {
		return val;
	}
}

template<size_t Idx, typename T, typename ...Args>
constexpr static auto find_type_at() {
	if constexpr (Idx == 0) {return T();}
	else return find_type_at<Idx - 1, Args...>();
}

template <typename... Args, size_t Idx>
constexpr static int32_t _convert_each(const Variant** p_args, uint8_t *buff) {
	using T = decltype(find_type_at<Idx, Args...>());
	T *val_ptr = (T *)(buff + type_size_before<Idx, Args...>());
	// Special handling for Variant: use default constructor
	if constexpr (std::is_same_v<T, Variant>) {
		new (val_ptr) Variant();
	} else {
		*val_ptr = p_args[Idx]->operator T();
	}
	return Idx;
}

template <typename... Args, size_t Idx, size_t... Is>
constexpr static bool _convert_each(const Variant** p_args, uint8_t *buff) {
	_convert_each<Args..., Is...>(p_args, buff);
	return false;
}

template <typename... Args, size_t... Is>
constexpr static void _convert(const Variant** p_args, uint8_t *buff, IndexSequence<Is...>) {
	_convert_each<Args..., Is...>(p_args, buff);
}

template <typename... Args>
constexpr static void convert(const Variant** p_args, uint8_t *buff) {
	_convert<Args...>(p_args, buff, BuildIndexSequence<sizeof...(Args)>());
}

//================================================================================
// Utility Function Wrappers
//================================================================================

// ValidatedUtilityFunction: void (*)(Variant *r_ret, const Variant **p_args, int p_argcount)
// GDExtensionPtrUtilityFunction: void (*)(void* base, const void** args, void* r_ret, int argcount)

template <typename RetT, typename... Args>
struct ValidatedUtilityFunctionWrapper {
    static void call(GDExtensionPtrUtilityFunction func, Variant* r_ret, const Variant** p_args, int p_argcount) {
        if (p_argcount != sizeof...(Args)) return;
        
        RetT ret;
        uint8_t *buff = jsb_stackalloc(uint8_t, type_size<Args...>());
        
        // Convert all arguments
        convert<Args...>(p_args, buff);
        
        // Call the GDExtension utility function
        func(nullptr, reinterpret_cast<const void**>(buff), &ret, sizeof...(Args));
        
        *r_ret = ret;
    }
};

//================================================================================
// Builtin Method Wrappers
//================================================================================

// ValidatedBuiltInMethod: void (*)(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret)
// GDExtensionPtrBuiltInMethod: void (*)(void* base, const void** args, void* r_ret, int argcount)

template <typename BaseT, typename RetT, typename... Args>
struct ValidatedBuiltInMethodWrapper {
    static void call(GDExtensionPtrBuiltInMethod func, Variant* base, const Variant** p_args, int p_argcount, Variant* r_ret) {
        if (p_argcount != sizeof...(Args)) return;
        
        RetT ret;
        BaseT base_val = base->operator BaseT();
        uint8_t *buff = jsb_stackalloc(uint8_t, type_size<Args...>());
        
        // Convert all arguments
        convert<Args...>(p_args, buff);
        
        // Call the GDExtension builtin method
        func(&base_val, reinterpret_cast<const void**>(buff), &ret, sizeof...(Args));
        
        *r_ret = ret;
    }
};

//================================================================================
// Getter Wrappers
//================================================================================

// ValidatedGetter: void (*)(const Variant *base, Variant *value)
// GDExtensionPtrGetter: void (*)(void* base, void* r_value)

template <typename BaseT, typename ValueT>
struct ValidatedGetterWrapper {
    static void call(GDExtensionPtrGetter func, const Variant* base, Variant* value) {
        BaseT base_val = base->operator BaseT();
        ValueT ret;
        func(&base_val, &ret);
        *value = ret;
    }
};

//================================================================================
// Setter Wrappers
//================================================================================

// ValidatedSetter: void (*)(Variant *base, const Variant *value)
// GDExtensionPtrSetter: void (*)(void* base, const void* p_value)

template <typename BaseT, typename ValueT>
struct ValidatedSetterWrapper {
    static void call(GDExtensionPtrSetter func, Variant* base, const Variant* value) {
        BaseT base_val = base->operator BaseT();
        ValueT value_val = value->operator ValueT();
        func(&base_val, &value_val);
    }
};

//================================================================================
// Constructor Wrappers
//================================================================================

// ValidatedConstructor: void (*)(Variant *r_base, const Variant **p_args)
// GDExtensionVariantFromTypeConstructorFunc: void (*)(Variant* r_base, const Variant** p_args, int p_argcount)

template <typename BaseT, typename... Args>
struct ValidatedConstructorWrapper {
    static void call(GDExtensionVariantFromTypeConstructorFunc func, Variant* r_base, const Variant** p_args, int p_argcount) {
        if (p_argcount != sizeof...(Args)) return;
        
        BaseT ret;
        uint8_t *buff = jsb_stackalloc(uint8_t, type_size<Args...>());
        
        // Convert all arguments
        convert<Args...>(p_args, buff);
        
        // Call the GDExtension constructor
        func(&ret, reinterpret_cast<const void**>(buff), sizeof...(Args));
        
        *r_base = ret;
    }
};

//================================================================================
// Indexed Access Wrappers
//================================================================================

// GDExtensionPtrIndexedGetter: void (*)(void* base, void* r_ret, const void* p_index)
// GDExtensionPtrIndexedSetter: void (*)(void* base, const void* p_value, const void* p_index)

template <typename BaseT, typename RetT, typename IndexT>
struct ValidatedIndexedGetterWrapper {
    static void call(GDExtensionPtrIndexedGetter func, Variant* base, Variant* r_ret, const Variant* p_index) {
        BaseT base_val = base->operator BaseT();
        RetT ret;
        IndexT index_val = p_index->operator IndexT();
        func(&base_val, &ret, &index_val);
        *r_ret = ret;
    }
};

template <typename BaseT, typename ValueT, typename IndexT>
struct ValidatedIndexedSetterWrapper {
    static void call(GDExtensionPtrIndexedSetter func, Variant* base, const Variant* p_value, const Variant* p_index) {
        BaseT base_val = base->operator BaseT();
        ValueT value_val = p_value->operator ValueT();
        IndexT index_val = p_index->operator IndexT();
        func(&base_val, &value_val, &index_val);
    }
};

//================================================================================
// Keyed Access Wrappers
//================================================================================

// GDExtensionPtrKeyedGetter: void (*)(void* base, void* r_ret, const void* p_key)
// GDExtensionPtrKeyedSetter: void (*)(void* base, const void* p_value, const void* p_key)

template <typename BaseT, typename RetT, typename KeyT>
struct ValidatedKeyedGetterWrapper {
    static void call(GDExtensionPtrKeyedGetter func, Variant* base, Variant* r_ret, const Variant* p_key) {
        BaseT base_val = base->operator BaseT();
        RetT ret;
        KeyT key_val = p_key->operator KeyT();
        func(&base_val, &ret, &key_val);
        *r_ret = ret;
    }
};

template <typename BaseT, typename ValueT, typename KeyT>
struct ValidatedKeyedSetterWrapper {
    static void call(GDExtensionPtrKeyedSetter func, Variant* base, const Variant* p_value, const Variant* p_key) {
        BaseT base_val = base->operator BaseT();
        ValueT value_val = p_value->operator ValueT();
        KeyT key_val = p_key->operator KeyT();
        func(&base_val, &value_val, &key_val);
    }
};

//================================================================================
// Operator Evaluator Wrappers
//================================================================================

// ValidatedOperatorEvaluator: void (*)(const Variant *left, const Variant *right, Variant *r_ret)
// GDExtensionPtrOperatorEvaluator: void (*)(const void* p_left, const void* p_right, void* r_ret)

template <typename LeftT, typename RightT, typename RetT>
struct ValidatedOperatorEvaluatorWrapper {
    static void call(GDExtensionPtrOperatorEvaluator func, const Variant* left, const Variant* right, Variant* r_ret) {
        LeftT left_val = left->operator LeftT();
        RightT right_val = right->operator RightT();
        RetT ret;
        func(&left_val, &right_val, &ret);
        *r_ret = ret;
    }
};

} // namespace godot

#endif // GODOTJS_VARIANT_TO_PTRCALL_HPP
