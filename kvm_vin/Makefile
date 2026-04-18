R_DESTDIR ?=
PREFIX ?= /usr/local

CC ?= gcc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O3
LDFLAGS ?=


# =====
_PLUGIN = kvm_vin

_CFLAGS = -MD -c -std=c17 -Wall -Wextra -D_GNU_SOURCE -DMK_WITH_AX -DWITH_AX -I/opt/include $(CFLAGS)
_LDFLAGS = -lm -pthread -lrt -Wl,-rpath=/opt/lib -L/opt/lib -lax_sys -lax_ivps -lax_engine -lax_proton -lax_mipi $(LDFLAGS)

_SRCS = $(shell ls src/libs/frame.c src/libs/logging.c src/ustreamer/encoders/ax_hw/ax_comm*.c src/ustreamer/encoders/ax_hw/ax_cap*.c src/*.c)

_BUILD = build


# =====
ifneq ($(shell sh -c 'uname 2>/dev/null || echo Unknown'),FreeBSD)
override _LDFLAGS += -latomic
endif

ifneq ($(MK_WITH_PTHREAD_NP),)
override _CFLAGS += -DWITH_PTHREAD_NP
endif


# =====
$(_PLUGIN): $(_SRCS:%.c=$(_BUILD)/%.o)
	$(info == SO $@)
	$(ECHO) $(CC) $^ -o $@ $(_LDFLAGS)


$(_BUILD)/%.o: %.c
	$(info -- CC $<)
	$(ECHO) mkdir -p $(dir $@) || true
	$(ECHO) $(CC) $< -o $@ $(_CFLAGS)



install: $(_PLUGIN)
	mkdir -p $(R_DESTDIR)$(PREFIX)/lib/ustreamer/kvm_vision
	install -m755 $(_PLUGIN) $(R_DESTDIR)$(PREFIX)/lib/ustreamer/kvm_vision/$(PLUGIN)


clean:
	rm -rf $(_PLUGIN) $(_BUILD)


_OBJS = $(_SRCS:%.c=$(_BUILD)/%.o)
-include $(_OBJS:%.o=%.d)
