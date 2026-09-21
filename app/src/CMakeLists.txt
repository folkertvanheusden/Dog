idf_component_register(SRCS book.cpp main.cpp max-ascii.cpp tt.cpp eval.cpp eval-stats.cpp san.cpp search.cpp stats.cpp str.cpp test.cpp tui.cpp nnue.cpp led_strip_encoder.c
	PRIV_REQUIRES spiffs console esp_driver_uart nvs_flash esp_wifi esp_driver_gpio esp_timer esp_netif esp_http_client esp_driver_usb_serial_jtag esp_driver_rmt esp_netif bootloader_support lwip esp_driver_usb_serial_jtag
	INCLUDE_DIRS . ../include)
spiffs_create_partition_image(spiffs ../data FLASH_IN_PROJECT)

set(PROJECT_VERSION_MAJOR 4)
set(PROJECT_VERSION_MINOR 10)
set(DOG_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.3")

execute_process(
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ./describe-app.sh
    OUTPUT_VARIABLE GIT_REV_DOG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND bash "-c" "git describe --always --dirty"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/include/libchess/
    OUTPUT_VARIABLE GIT_REV_LC
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND bash "-c" "git describe --always --dirty"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/fathom
    OUTPUT_VARIABLE GIT_REV_FATHOM
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ESP32 with one of: WEMOS32, ESP32_S3_QTPY, ESP32_S3_XIAO, ESP32_S3_WAVESHARE
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DOG_VERSION="${DOG_VERSION}"
    ESP32
    ESP32_S3_XIAO
    GIT_REV_DOG="${GIT_REV_DOG}"
    GIT_REV_LC="${GIT_REV_LC}"
    GIT_REV_FATHOM="${GIT_REV_FATHOM}"
    BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    BUILD_TARGET="regular"
)
