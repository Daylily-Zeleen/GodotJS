#ifndef GODOTJS_RESOURCE_SAVER_H
#define GODOTJS_RESOURCE_SAVER_H

#include "../compat/jsb_compat.h"

class ResourceFormatSaverGodotJSScript : public ResourceFormatSaver
{
    bool add_uid_to_source(String &p_r_source, const String &p_path, ResourceUID::ID p_uid = ResourceUID::INVALID_ID) const;

public:
    virtual Error save(const Ref<Resource>& p_resource, const String& p_path, uint32_t p_flags = 0) override;
    virtual void get_recognized_extensions(const Ref<Resource>& p_resource, List<String>* p_extensions) const override;
    virtual bool recognize(const Ref<Resource>& p_resource) const override;
	virtual Error set_uid(const String&p_path, ResourceUID::ID p_uid) override;
};

#endif
