#include "register_types.h"

#include "weaver/jsb_weaver.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

#ifdef TOOLS_ENABLED
#include "weaver-editor/jsb_weaver_editor.h"
#endif

static Ref<ResourceFormatLoaderGodotJSScript> resource_loader_js;
static Ref<ResourceFormatSaverGodotJSScript> resource_saver_js;

void jsb_initialize_module(ModuleInitializationLevel p_level)
{
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        GDREGISTER_CLASS(GodotJSScript);

        jsb::impl::GlobalInitialize::init();

        // register javascript language
        GodotJSScriptLanguage* script_language_js = memnew(GodotJSScriptLanguage());
        Engine::get_singleton()->register_script_language(script_language_js);

        resource_loader_js.instantiate();
        ResourceLoader::get_singleton()->add_resource_format_loader(resource_loader_js);

        resource_saver_js.instantiate();
        ResourceSaver::get_singleton()->add_resource_format_saver(resource_saver_js);
    }
#ifdef TOOLS_ENABLED
    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR)
    {
        GDREGISTER_INTERNAL_CLASS(GodotJSEditorHelper);
        GDREGISTER_INTERNAL_CLASS(GodotJSEditorProgress);
        GDREGISTER_INTERNAL_CLASS(GodotJSEditorPlugin);
        EditorPlugins::add_by_type<GodotJSEditorPlugin>();
    }
#endif
}

void jsb_uninitialize_module(ModuleInitializationLevel p_level)
{
    if (p_level == MODULE_INITIALIZATION_LEVEL_CORE)
    {
        ResourceLoader::get_singleton()->remove_resource_format_loader(resource_loader_js);
        resource_loader_js.unref();

        ResourceSaver::get_singleton()->remove_resource_format_saver(resource_saver_js);
        resource_saver_js.unref();

        GodotJSScriptLanguage *script_language_js = GodotJSScriptLanguage::get_singleton();
        jsb_check(script_language_js);
        Engine::get_singleton()->unregister_script_language(script_language_js);
        memdelete(script_language_js);
    }
}

extern "C"
{
    GDExtensionBool GDE_EXPORT jsb_gdextension_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization)
    {
        GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(jsb_initialize_module);
        init_obj.register_terminator(jsb_uninitialize_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_CORE);

        return init_obj.init();
    }
}
