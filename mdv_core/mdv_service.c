#include "mdv_service.h"
#include <mdv_log.h>


bool mdv_service_init(mdv_service *svc, char const *cfg_file_path)
{
    if (!mdv_load_config(cfg_file_path, &svc->config))
    {
        MDV_LOGE("Service initialization failed. Can't load '%s'\n", cfg_file_path);
        return false;
    }

    if (!mdv_metainf_open(&svc->db.metainf, svc->config.storage.path.ptr))
    {
        MDV_LOGE("DB meta information loading was failed. Path: '%s'\n", svc->config.storage.path.ptr);
        return false;
    }

    mdv_metainf_validate(&svc->db.metainf);
    mdv_metainf_flush(&svc->db.metainf);

    // Print node information to log
    MDV_LOGI("Storage version: %u", svc->db.metainf.version.value);
    char tmp[33];
    mdv_string uuid_str = mdv_str_static(tmp);
    if (mdv_uuid_to_str(&svc->db.metainf.uuid.value, &uuid_str))
        MDV_LOGI("Node UUID: %s", uuid_str.ptr);

    return true;
}


void mdv_service_free(mdv_service *svc)
{
    mdv_metainf_close();
}


bool mdv_service_start(mdv_service *svc)
{
    return true;
}
