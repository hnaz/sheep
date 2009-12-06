VERSION		= 0.1
NAME		= Klaatu

CC		= gcc
CPP		= cpp
CFLAGS		= -O2 -pipe
LDFLAGS		=

DESTDIR		=
prefix		= /usr
bindir		= $(prefix)/bin

S_CFLAGS	= -Wall -Wextra -Wno-unused-parameter -fPIC $(CFLAGS)
S_LDFLAGS	= -ldl -rdynamic $(LDFLAGS)

ifeq ($(D),1)
S_CFLAGS	+= -O0 -g
endif

ifeq ($(V),1)
Q		=
cmd		= $(2)
else
Q		= @
cmd		= echo $(1); $(2)
endif

PHONY		:= all
all: sheep

PHONY		+= install
install: sheep
	mkdir -p $(DESTDIR)$(bindir)
	cp sheep/sheep $(DESTDIR)$(bindir)

PHONY		+= sheep
sheep: sheep/sheep

include sheep/Makefile
sheep-obj	:= $(addprefix sheep/, $(sheep-obj))

sheep/sheep: $(sheep-obj)
	$(Q)$(call cmd, "   LD     $@",					\
		$(CC) -o $@ $^ $(S_LDFLAGS))

clean		:= sheep/sheep

$(sheep-obj): include/sheep/config.h sheep/make.deps

clean		+= $(sheep-obj)

include/sheep/config.h:
	$(Q)$(call cmd, "   CF     $@",					\
		rm -f $@;						\
		echo "#define SHEEP_VERSION \"$(VERSION)\"" >> $@;	\
		echo "#define SHEEP_NAME \"$(NAME)\"" >> $@)

clean		+= include/sheep/config.h

sheep/make.deps:
	$(Q)$(call cmd, "   MK     $@",					\
		rm -f $@;						\
		$(foreach obj,$(sheep-obj),				\
			$(CPP) -Iinclude -MM -MT $(obj)			\
				$(basename $(obj)).c >> $@; ))
clean		+= sheep/make.deps

ifneq ($(MAKECMDGOALS),clean)
-include sheep/make.deps
endif

PHONY		+= clean
clean:
	$(Q)$(foreach subdir,$(sort $(dir $(clean))),			\
		$(call cmd, "   CL     $(subdir)",			\
			rm -f $(filter $(subdir)%,$(clean)); ))

.PHONY: $(PHONY)

%.o: %.c
	$(Q)$(call cmd, "   CC     $@",					\
		$(CC) $(S_CFLAGS) -Iinclude -o $@ -c $<)
