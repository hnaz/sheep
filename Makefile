VERSION = 0.1
NAME = Klaatu

CC = gcc
CPP = cpp
CFLAGS = -Os -pipe
LDFLAGS =

SCFLAGS = -Wall -Wextra -Wno-unused-parameter $(CFLAGS)
SLDFLAGS = $(LDFLAGS)

ifeq ($(V),1)
Q	=
cmd	= $(2)
else
Q	= @
cmd	= echo $(1); $(2)
endif

all: sheep/sheep

include sheep/Makefile
sheep-obj	:= $(addprefix sheep/, $(sheep-obj))
sheep-clean	:= sheep/sheep $(sheep-obj) include/sheep/config.h	\
		   sheep/make.deps

sheep/sheep: $(sheep-obj)
	$(Q)$(call cmd, "   LD     $@",					\
		$(CC) -o $@ $^ $(SLDFLAGS))

%.o: %.c include/sheep/config.h sheep/make.deps
	$(Q)$(call cmd, "   CC     $@",					\
		$(CC) $(SCFLAGS) -Iinclude -o $@ -c $<)

include/sheep/config.h:
	$(Q)$(call cmd, "   CONF   $@",					\
		rm -f $@;						\
		echo "#define SHEEP_VERSION \"$(VERSION)\"" >> $@;	\
		echo "#define SHEEP_NAME \"$(NAME)\"" >> $@)

sheep/make.deps:
	$(Q)$(call cmd, "   DEPS   $@",					\
		rm -f $@;						\
		$(foreach obj,$(sheep-obj),				\
			$(CPP) -Iinclude -MM -MT $(obj)			\
				$(basename $(obj)).c >> $@; ))

ifneq ($(MAKECMDGOALS),clean)
-include sheep/make.deps
endif

clean:
	$(Q)$(foreach subdir,$(sort $(dir $(sheep-clean))),		\
		$(call cmd, "   CLEAN   $(subdir)",			\
			rm -f $(filter $(subdir)%,$(sheep-clean)); ))
