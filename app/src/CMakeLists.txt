idf_component_register(SRCS book.cpp main.cpp psq.cpp max-ascii.cpp tt.cpp eval.cpp eval_par.cpp search.cpp stats.cpp str.cpp test.cpp tui.cpp INCLUDE_DIRS "")
spiffs_create_partition_image(spiffs data)
