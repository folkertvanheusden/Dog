LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := Dog
LOCAL_SRC_FILES := ../../../eval.cpp ../../../eval_par.cpp ../../../main.cpp ../../../psq.cpp ../../../tt.cpp

LOCAL_CPPFLAGS := -std=gnu++17 -Wall -fPIC -DVERSION=\"0.8\" -DNAME_EXTRA=\"\" -fexceptions -I../../../include/ -Wno-c++11-narrowing # whatever g++ flags you like
LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -fPIC -pie   # whatever ld flags you like

#LOCAL_LDFLAGS := -shared

include $(BUILD_SHARED_LIBRARY)
