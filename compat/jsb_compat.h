#ifndef GODOTJS_COMPAT_H
#define GODOTJS_COMPAT_H

#include "jsb_engine_version_comparison.h"
#include "jsb_paged_allocator.h"
#include "jsb_ring_buffer.h"
#include "jsb_rw_lock.h"

// GDExtension compatibility: Variant::PTRBuiltInMethod was removed in godot-cpp 4.7
// Define them here for backward compatibility (matches Godot 4.2 engine signatures)
namespace godot {
using PTRBuiltInMethod = GDExtensionPtrBuiltInMethod; // void (*)(void* base, const void** args, void* r_ret, int argcount);
using PTRGetter = GDExtensionPtrGetter; //void (*)(void* base, void* r_value);
using PTRSetter = GDExtensionPtrSetter; //void (*)(void* base, const void* p_value);

using ValidatedUtilityFunction = void (*)(Variant *r_ret, const Variant **p_args, int p_argcount);
using ValidatedOperatorEvaluator = void (*)(const Variant *left, const Variant *right, Variant *r_ret);

// GDExtension compatibility: these types are not defined in godot-cpp's Variant class
using ValidatedBuiltInMethod = void (*)(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret);
using ValidatedSetter = void (*)(Variant *base, const Variant *value);
using ValidatedGetter = void (*)(const Variant *base, Variant *value);
using ValidatedConstructor = void (*)(Variant *r_base, const Variant **p_args);

using ObjectInstanceID = decltype(Object().get_instance_id());
} //namespace godot

#define TTR(text) (text)
#define SNAME(text) [] {static StringName sn {text}; return sn; }()

static void object_get_instance_binding(Object *p_obj, void *p_token, const GDExtensionInstanceBindingCallbacks *p_callbacks) {
	::godot::gdextension_interface::object_get_instance_binding(p_obj, p_token, p_callbacks);
}

template <typename StrArray>
static godot::String string_join(const godot::String &separator, const StrArray &parts) {
	if (parts.is_empty())
		return {};
	else if (parts.size() == 1)
		return parts[0];

	const int this_length = separator.length();

	int new_size = (parts.size() - 1) * this_length;
	for (const String &part : parts) {
		new_size += part.length();
	}
	new_size += 1;

	String ret;
	ret.resize(new_size);
	char32_t *ret_ptrw = ret.ptrw();
	const char32_t *this_ptr = separator.ptr();

	bool first = true;
	for (const String &part : parts) {
		if (first) {
			first = false;
		} else if (this_length) {
			memcpy(ret_ptrw, this_ptr, this_length * sizeof(char32_t));
			ret_ptrw += this_length;
		}

		const int part_length = part.length();
		if (part_length) {
			memcpy(ret_ptrw, part.ptr(), part_length * sizeof(char32_t));
			ret_ptrw += part_length;
		}
	}

	*ret_ptrw = 0;

	return ret;
}

#endif
