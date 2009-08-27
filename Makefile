VERSION	= 0.1
NAME	= Klaatu

CC	= gcc
CPP	= cpp

CFLAGS	= -Os -pipe -Wall -Wextra -Wno-unused-parameter
LDFLAGS	=

ifeq ($(V),1)
Q	=
else
Q	= @
endif

all: sheep/sheep

include sheep/Makefile
sheep-obj	:= $(addprefix sheep/,$(sheep-obj))
sheep-clean	:= sheep/sheep $(sheep-obj) include/sheep/config.h \
		   sheep/make.deps

sheep/sheep: $(sheep-obj)
	$(Q) echo "   LD     $@";				\
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c include/sheep/config.h sheep/make.deps
	$(Q) echo "   CC     $@";				\
	$(CC) $(CFLAGS) -Iinclude -o $@ -c $<

include/sheep/config.h:
	$(Q) echo "   CONF   $@";				\
	rm -f $@;						\
	echo "#define SHEEP_VERSION \"$(VERSION)\"" >> $@;	\
	echo "#define SHEEP_NAME \"$(NAME)\"" >> $@

sheep/make.deps:
	$(Q) echo "   DEPS   $@";				\
	rm -f $@;						\
	$(foreach obj,$(sheep-obj),				\
		$(CPP) -Iinclude -MM -MT $(obj)			\
		$(basename $(obj)).c >> $@; )

ifneq ($(MAKECMDGOALS),clean)
-include sheep/make.deps
endif

clean:
	$(Q) $(foreach subdir,$(sort $(dir $(sheep-clean))),	\
		echo "   CLEAN   $(subdir)";			\
		rm -f $(filter $(subdir)%,$(sheep-clean)); )
