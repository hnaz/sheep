VERSION = 0.1
NAME = Klaatu

CC = gcc
CPP = cpp
CFLAGS = -O2 -pipe
LDFLAGS =

DESTDIR =
prefix = /usr
bindir = $(prefix)/bin

S_CFLAGS = -Wall -Wextra -Wno-unused-parameter -fPIC $(CFLAGS)
S_LDFLAGS = -ldl -rdynamic $(LDFLAGS)

ifeq ($(D),1)
S_CFLAGS += -O0 -g
endif

ifeq ($(V),1)
Q =
cmd = $(2)
else
Q = @
cmd = echo $(1); $(2)
endif

all: sheep

install: sheep
	mkdir -p $(DESTDIR)$(bindir)
	cp sheep/sheep $(DESTDIR)$(bindir)

sheep: sheep/sheep

include sheep/Makefile
sheep-obj := $(addprefix sheep/, $(sheep-obj))

sheep/sheep: $(sheep-obj)
	$(Q)$(call cmd, "   LD     $@",					\
		$(CC) -o $@ $^ $(S_LDFLAGS))

$(sheep-obj): include/sheep/config.h sheep/make.deps

include/sheep/config.h: Makefile
	$(Q)$(call cmd, "   CF     $@",					\
		rm -f $@;						\
		echo "#define SHEEP_VERSION \"$(VERSION)\"" >> $@;	\
		echo "#define SHEEP_NAME \"$(NAME)\"" >> $@)

sheep/make.deps:
	$(Q)$(call cmd, "   MK     $@",					\
		rm -f $@;						\
		$(foreach obj,$(sheep-obj),				\
			$(CPP) -Iinclude -MM -MT $(obj)			\
				$(basename $(obj)).c >> $@; ))

ifneq ($(MAKECMDGOALS),clean)
-include sheep/make.deps
endif

clean := sheep/sheep $(sheep-obj) include/sheep/config.h sheep/make.deps

clean:
	$(Q)$(foreach subdir,$(sort $(dir $(clean))),			\
		$(call cmd, "   CL     $(subdir)",			\
			rm -f $(filter $(subdir)%,$(clean)); ))

%.o: %.c
	$(Q)$(call cmd, "   CC     $@",					\
		$(CC) $(S_CFLAGS) -Iinclude -o $@ -c $<)

.PHONY: all sheep clean
