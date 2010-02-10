# Versioning
VERSION = 0.1
NAME = Klaatu

# Tools
CC = gcc
CPP = cpp
CFLAGS = -O2 -pipe
LDFLAGS =

# Paths
DESTDIR =
prefix = /usr
bindir = $(prefix)/bin
libdir = $(prefix)/lib

# Compilation parameters
SCFLAGS = -Wall -Wextra -Wno-unused-parameter -fPIC -Iinclude $(CFLAGS)
SLDFLAGS = -ldl $(LDFLAGS)

# Debug
ifeq ($(D),1)
SCFLAGS += -O0 -g
endif

# Build parameters
ifeq ($(V),1)
Q =
cmd = $(2)
else
Q = @
cmd = echo $(1); $(2)
endif

# User targets
all: libsheep sheep lib

install: install-libsheep install-sheep install-lib

libsheep: sheep/libsheep-$(VERSION).so

sheep: sheep/sheep

# Build targets
include sheep/Makefile
libsheep-obj := $(addprefix sheep/, $(libsheep-obj))
sheep-obj := $(addprefix sheep/, $(sheep-obj))

sheep/libsheep-$(VERSION).so: $(libsheep-obj)
	$(Q)$(call cmd, "   LD     $@",					\
		$(CC) $(SCFLAGS) -o $@ $^ $(SLDFLAGS) -shared)

install-libsheep: sheep/libsheep-$(VERSION).so
	mkdir -p $(DESTDIR)$(libdir)
	cp $^ $(DESTDIR)$(libdir)
	ln -s $^ $(DESTDIR)$(libdir)/libsheep.so

sheep/sheep: sheep/libsheep-$(VERSION).so $(sheep-obj)
	$(Q)$(call cmd, "   LD     $@",					\
		$(CC) $(SCFLAGS) -Lsheep -o $@ $(sheep-obj) -lsheep-$(VERSION))

install-sheep: sheep/sheep
	mkdir -p $(DESTDIR)$(bindir)
	cp $^ $(DESTDIR)$(bindir)

$(libsheep-obj): include/sheep/config.h sheep/make.deps

include/sheep/config.h: Makefile
	$(Q)$(call cmd, "   CF     $@",					\
		rm -f $@;						\
		echo "#define SHEEP_VERSION \"$(VERSION)\"" >> $@;	\
		echo "#define SHEEP_NAME \"$(NAME)\"" >> $@)

sheep/make.deps:
	$(Q)$(call cmd, "   MK     $@",					\
		rm -f $@;						\
		$(foreach obj,$(libsheep-obj),				\
			$(CPP) -Iinclude -MM -MT $(obj)			\
				$(basename $(obj)).c >> $@; ))

include lib/Makefile
lib := $(addprefix lib/, $(lib))

lib: $(lib)

install-lib: $(lib)
	mkdir -p $(DESTDIR)$(libdir)/sheep-$(VERSION)
	cp $^ $(DESTDIR)$(libdir)/sheep-$(VERSION)

$(lib): sheep/libsheep-$(VERSION).so
	$(Q)$(call cmd, "   LD     $@",					\
		$(CC) $(SCFLAGS) -Lsheep -o $@ $(basename $@).c		\
			-lsheep-$(VERSION) -shared			\
			$($(subst lib/,,$(basename $@))-LDFLAGS))

# Cleanup
ifneq ($(MAKECMDGOALS),clean)
-include sheep/make.deps
endif

clean := sheep/libsheep-$(VERSION).so $(libsheep-obj)
clean += sheep/sheep $(sheep-obj)
clean += include/sheep/config.h sheep/make.deps
clean += $(lib)

clean:
	$(Q)$(foreach subdir,$(sort $(dir $(clean))),			\
		$(call cmd, "   CL     $(subdir)",			\
			rm -f $(filter $(subdir)%,$(clean)); ))

# Build library
%.o: %.c
	$(Q)$(call cmd, "   CC     $@",					\
		$(CC) $(SCFLAGS) -o $@ -c $<)

# Misc
PHONY := all libsheep sheep lib clean
PHONY += install install-libsheep install-sheep install-lib
.PHONY: $(PHONY)
