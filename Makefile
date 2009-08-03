CC	= gcc
CPP	= cpp

CFLAGS	= -Os -pipe -Wall -Wextra -Wno-unused-parameter
LDFLAGS	=

all: sheep/sheep

include sheep/Makefile
sheep-src := $(addprefix sheep/,$(sheep-src))
sheep-obj := $(sheep-src:.c=.o)

sheep/sheep: $(sheep-obj)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c sheep/make.deps
	$(CC) $(CFLAGS) -Iinclude -o $@ -c $<

sheep/make.deps: $(sheep-src)
	rm -f sheep/make.deps
	$(foreach obj,$(sheep-obj), \
		$(CPP) -Iinclude -MM -MT $(obj) \
		$(basename $(obj)).c >> sheep/make.deps; )

ifneq ($(MAKECMDGOALS),clean)
-include sheep/make.deps
endif

clean:
	rm -f sheep/sheep $(sheep-obj) sheep/make.deps
