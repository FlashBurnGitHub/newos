# app makefile
MY_TARGETDIR := $(APPS_BUILD_DIR)/rldtest
MY_SRCDIR := $(APPS_DIR)/rldtest
MY_TARGET :=  $(MY_TARGETDIR)/rldtest
ifeq ($(call FINDINLIST,$(MY_TARGET),$(ALL)),1)

MY_SRCS := \
	rldtest.c

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LIBS := $(MY_TARGETDIR)/librldtest.so -lc -lnewos -lsupc++
MY_LIBPATHS :=
MY_DEPS := $(MY_TARGETDIR)/librldtest.so $(MY_TARGETDIR)/girlfriend.so
MY_GLUE := $(APPSGLUE)

include templates/app.mk

# build the librldtest.so object
MY_TARGETDIR := $(APPS_BUILD_DIR)/rldtest
MY_SRCDIR := $(APPS_DIR)/rldtest
MY_TARGET := $(MY_TARGETDIR)/librldtest.so

MY_SRCS := \
	shared.c

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LINKSCRIPT := $(LIBS_LDSCRIPT)

include templates/lib.mk

# build the girlfriend.so object
MY_TARGETDIR := $(APPS_BUILD_DIR)/rldtest
MY_SRCDIR := $(APPS_DIR)/rldtest
MY_TARGET := $(MY_TARGETDIR)/girlfriend.so

MY_SRCS := \
	girlfriend.c

MY_INCLUDES := $(STDINCLUDE)
MY_CFLAGS := $(USER_CFLAGS)
MY_LINKSCRIPT := $(LIBS_LDSCRIPT)

include templates/lib.mk

endif
