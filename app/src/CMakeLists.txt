idf_component_register(SRCS book.cpp main.cpp max-ascii.cpp tt.cpp eval.cpp san.cpp search.cpp stats.cpp str.cpp test.cpp tui.cpp nnue.cpp
	PRIV_REQUIRES spiffs console esp_driver_uart nvs_flash esp_wifi esp_driver_gpio esp_timer esp_netif esp_http_client esp_driver_usb_serial_jtag
	INCLUDE_DIRS . ../include)
spiffs_create_partition_image(spiffs data)
