idf_component_register(SRCS "main.c" "csi_handler.c" "provision.c"
                        INCLUDE_DIRS "."
                        REQUIRES esp_wifi esp_event nvs_flash esp_netif
                                 esp_websocket_client cJSON freertos)
