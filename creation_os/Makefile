# CREATION OS minimal kernel — one binary, no deps.
CC      := clang
TARGET  := creation_kernel
SRC     := creation_kernel.c

UNAME_M := $(shell uname -m)
# Optional: Apple Silicon tuned ISA (fallback if clang rejects -march=armv9-a+sme).
ifeq ($(UNAME_M),arm64)
  ARCH_TRY := $(shell $(CC) -march=armv9-a+sme -E -x c /dev/null -o /dev/null 2>/dev/null && echo yes)
  ifeq ($(ARCH_TRY),yes)
    ARCH_FLAGS := -march=armv9-a+sme
  else
    ARCH_FLAGS := -mcpu=native
  endif
else
  ARCH_FLAGS :=
endif

CFLAGS  := -O2 $(ARCH_FLAGS) -Wall -Wextra -Werror -std=c11

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SRC) creation_kernel.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: $(TARGET)
	cd "$(CURDIR)" && ./$(TARGET)

clean:
	rm -f $(TARGET)
