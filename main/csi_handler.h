#pragma once
#include "esp_wifi.h"

void csi_init(void);
void csi_start(void);
void csi_stop(void);
void csi_set_device_id(const char *device_id);
