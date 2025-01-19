idf_component_register(SRCS book.cpp main.cpp psq.cpp max-ascii.cpp tt.cpp eval.cpp san.cpp search.cpp stats.cpp str.cpp test.cpp tui.cpp nnue.cpp INCLUDE_DIRS "")
spiffs_create_partition_image(spiffs data)
