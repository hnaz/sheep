CC	= gcc
CPP	= cpp

CFLAGS	= -Os -pipe -Wall -Wextra -Wno-unused-parameter
LDFLAGS	=

ifeq ($(V),1)
print	=
else
print	= @echo $(1);
endif

all: sheep/sheep

include sheep/Makefile
sheep-obj	:= $(addprefix sheep/,$(sheep-obj))
sheep-clean	:= sheep/sheep $(sheep-obj) sheep/make.deps

sheep/sheep: $(sheep-obj)
	$(call print,"   LD     $@") $(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c sheep/make.deps
	$(call print,"   CC     $@") $(CC) $(CFLAGS) -Iinclude -o $@ -c $<

sheep/make.deps:
	$(call print,"   DEPS   $@") rm -f sheep/make.deps;	\
	$(foreach obj,$(sheep-obj),				\
		$(CPP) -Iinclude -MM -MT $(obj)			\
		$(basename $(obj)).c >> sheep/make.deps; )

ifneq ($(MAKECMDGOALS),clean)
-include sheep/make.deps
endif

clean:
	$(call print, "   CLEAN  $(sheep-clean)") rm -f $(sheep-clean)
