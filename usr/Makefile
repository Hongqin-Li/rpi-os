-include ../config.mk

LIBC_A = ../libc/lib/libc.a
LIBC_SPEC = ../libc/lib/musl-gcc.specs
LIBC_LIBS = $(wildcard ../libc/lib/*)
LIBC_INCS = $(wildcard ../libc/obj/include/**/*) $(wildcard ../libc/include/**/*)
LIBC_DEPS = $(LIBC_LIBS) $(LIBC_INCS)

OBJ = ../obj/usr
LIBC_SPEC_OUT = $(OBJ)/musl-gcc.specs

USR_CC := $(CC) -specs $(LIBC_SPEC_OUT)

# -z max-page-size: https://stackoverflow.com/questions/33005638/how-to-change-alignment-of-code-segment-in-elf
CFLAGS = -std=gnu99 -O3 -MMD -MP -static -z max-page-size=4096 \
  -fno-omit-frame-pointer \
  -I../libc/obj/include/ \
  -I../libc/arch/aarch64/ \
  -I../libc/arch/generic/

BIN := $(OBJ)/bin
SRC := src

USER_DIRS := $(shell find $(SRC) -maxdepth 1 -mindepth 1 -type d)
USER_BINS := $(USER_DIRS:$(SRC)/%=$(BIN)/%)

all:
	$(MAKE) -C ../libc
	mkdir -p $(dir $(LIBC_SPEC_OUT))
	# Replace "/usr/local/musl" to "../libc"
	sed -e "s/\/usr\/local\/musl/..\/libc/g" $(LIBC_SPEC) > $(LIBC_SPEC_OUT)
	$(MAKE) $(USER_BINS)

$(OBJ)/%.c.o: %.c $(LIBC_DEPS)
	mkdir -p $(dir $@)
	$(USR_CC) $(CFLAGS) -c -o $@ $<
	
# Same as `bin/NAME: $(OBJ)/NAME/*.c.o $(LIBC_A)`
# NOTE: Static library should be at last.
.SECONDEXPANSION:
$(BIN)/%: $$(addsuffix .o, $$(addprefix $(OBJ)/, $$(shell find $(SRC)/% -name "*.c")))
	mkdir -p $(dir $@)
	$(USR_CC) $(CFLAGS) -o $@ $^
	$(OBJDUMP) -S -d $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).asm
	$(OBJDUMP) -x $@ > $(OBJ)/src/$(notdir $@)/$(notdir $@).hdr
	$(STRIP) --strip-debug $@

clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all clean
