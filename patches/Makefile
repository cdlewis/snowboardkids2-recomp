TARGET = patches.elf

CC      ?= clang
LD      ?= ld.lld

CFLAGS   := -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
			-fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset \
			-Wall -Wextra -Wno-incompatible-library-redeclaration -Wno-unused-parameter -Wno-unknown-pragmas -Wno-unused-variable -Wno-unused-but-set-variable -Wno-missing-braces -Wno-unsupported-floating-point-opt -Wno-switch
CPPFLAGS := -nostdinc -DF3DEX_GBI_2 -D_LANGUAGE_C -DMIPS -DM2CTX -I dummy_headers -I ../lib/sk2-decomp/include -I ../lib/sk2-decomp/build/include -I ../lib/sk2-decomp/lib/ultralib/include -I ../lib/sk2-decomp/lib/ultralib/include/PR -I ../lib/rt64/include
CPPFLAGS += $(EXTRA_CPPFLAGS)
LDFLAGS  := -nostdlib -T patches.ld -T syms.ld -Map patches.map --unresolved-symbols=ignore-all --emit-relocs

DECOMP_DIR := ../lib/sk2-decomp
COURSE_DEFINITION_DIR := $(DECOMP_DIR)/config/courses
COURSE_DEFINITION_GENERATED_DIR := $(DECOMP_DIR)/build/include/generated/course_definitions
COURSE_DEFINITION_IDS := $(COURSE_DEFINITION_GENERATED_DIR)/course_ids.inc
PYTHON ?= python3

C_SRCS := $(wildcard *.c)
C_OBJS := $(C_SRCS:.c=.o)
C_DEPS := $(C_SRCS:.c=.d)

$(TARGET): $(C_OBJS) patches.ld syms.ld
	$(LD) $(C_OBJS) $(LDFLAGS) -o $@

$(C_OBJS): %.o : %.c $(COURSE_DEFINITION_IDS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -MMD -MF $(@:.o=.d) -c -o $@

# race/course.h includes generated course-definition fragments, so build them
# from the decomp's YAML definitions before compiling any patch.
$(COURSE_DEFINITION_IDS): $(wildcard $(COURSE_DEFINITION_DIR)/*.yaml) $(DECOMP_DIR)/tools/generate_course_definitions.py
	@mkdir -p $(COURSE_DEFINITION_GENERATED_DIR)
	$(PYTHON) $(DECOMP_DIR)/tools/generate_course_definitions.py --definitions $(COURSE_DEFINITION_DIR) --out $(COURSE_DEFINITION_GENERATED_DIR)

clean:
	rm -rf *.o *.d $(TARGET)

-include $(C_DEPS)

.PHONY: clean
