#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "esp_system.h"

namespace storage
{
    esp_err_t read_int32(const char* storage_name, const char *variable_name, int32_t *out_value);
    esp_err_t write_int32(const char* storage_name, const char *variable_name, int32_t value);
    esp_err_t read_string(const char* storage_name, const char *variable_name, char* out_string, size_t *max_length);
    esp_err_t write_string(const char* storage_name, const char *variable_name, const char* value);

    void format_nvs();
    void init_nvs();
}

#endif