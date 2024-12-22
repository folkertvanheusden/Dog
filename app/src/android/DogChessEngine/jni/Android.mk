LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := Dog
LOCAL_SRC_FILES := ../../../eval.cpp ../../../eval_par.cpp ../../../main.cpp ../../../psq.cpp ../../../tt.cpp

LOCAL_CPPFLAGS += -std=gnu++17 -Wall -DVERSION=\"1.2\" -DNAME_EXTRA=\"\" -fexceptions -frtti -I../../../include/ -Wno-c++11-narrowing #-fPIC -pie
LOCAL_LDFLAGS += -pthread #-pie
LOCAL_LDLIBS += -pthread -llog

include $(BUILD_EXECUTABLE)
