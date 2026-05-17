R_DESTDIR ?=
PREFIX ?= /usr/local

CC ?= gcc
CXX ?= g++
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O3
LDFLAGS ?=


# =====
_PLUGIN = libmaixcam_lib.so

_CFLAGS = -fPIC -MD -c -std=c17 -Wall -Wno-write-strings -D_GNU_SOURCE -DMK_WITH_AX -DWITH_AX -Iinclude -Iinclude/maixcam2 -Isample/common -I/opt/include $(CFLAGS)
_LDFLAGS = -shared -lm -pthread -lrt -Wl,-rpath=/opt/lib -L/opt/lib -lax_sys -lax_ivps -lax_engine -lax_proton -lax_venc -lax_mipi -lax_vdec -lax_vo -lax_ae -lax_awb -lax_audio -lax_audio_3a -lsamplerate -ltinyalsa -lstdc++ $(LDFLAGS)

_SRCS = $(shell ls src/*.cpp)
#_SRCS += $(shell ls basic/*.cpp)
_SRCC = $(shell ls sample/common/*.c)

_BUILD = build


# =====
ifneq ($(shell sh -c 'uname 2>/dev/null || echo Unknown'),FreeBSD)
override _LDFLAGS += -latomic
endif

ifneq ($(MK_WITH_PTHREAD_NP),)
override _CFLAGS += -DWITH_PTHREAD_NP
endif

_CXXFLAGS = $(subst -std=c17,-std=c++17, $(_CFLAGS))


# =====
$(_PLUGIN): $(_SRCS:%.cpp=$(_BUILD)/%.o) $(_SRCC:%.c=$(_BUILD)/%.o)
	$(info == SO $@)
	$(ECHO) $(CXX) $^ -o $@ $(_LDFLAGS)


$(_BUILD)/%.o: %.c
	$(info -- CC $<)
	$(ECHO) mkdir -p $(dir $@) || true
	$(ECHO) $(CC) $< -o $@ $(_CFLAGS)


$(_BUILD)/%.o: %.cpp
	$(info -- CXX $<)
	$(ECHO) mkdir -p $(dir $@) || true
	$(ECHO) $(CXX) $< -o $@ $(_CXXFLAGS)



install: $(_PLUGIN)
	mkdir -p $(R_DESTDIR)$(PREFIX)/lib/ustreamer/kvm_vision
	install -m755 $(_PLUGIN) $(R_DESTDIR)$(PREFIX)/lib/ustreamer/kvm_vision/$(PLUGIN)


clean:
	rm -rf $(_PLUGIN) $(_BUILD)


_OBJS = $(_SRCS:%.cpp=$(_BUILD)/%.o)
_OBJS += $(_SRCC:%.c=$(_BUILD)/%.o)
-include $(_OBJS:%.o=%.d)
