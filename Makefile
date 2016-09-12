VERSION=1.0

CC ?= gcc

LIB_OBJS := libnorsim.o libnorsim_syscalls.o
PRG_OBJS := main.o

CFLAGS := -pipe -std=c11 -D_GNU_SOURCE=1 -fstack-protector-all
CFLAGS_LIB := -fPIC
CFLAGS_WRN := -Wall -Wextra -Wwrite-strings -Wbad-function-cast -Wshadow -Wpointer-arith -Wmissing-declarations -Wundef -Wunreachable-code -Wuninitialized -Wmissing-noreturn -Wmissing-braces -Winit-self -Wcast-align -Wnested-externs -Winline -Wredundant-decls -Wparentheses -Wno-unused-result -Wno-missing-declarations
#-Wcast-qual
CFLAGS_DBG := -ggdb -O0
CFLAGS_REL := -g0 -O3
CFLAGS_DEP := -MD -MP

CFLAGS_LIBS := -ldl

LIB_NAME := norsim
PRG := main
LIB := lib$(LIB_NAME).so

ifdef 32BIT
CFLAGS += -m32
endif
ifdef BUILD_DEBUG
CFLAGS += $(CFLAGS_DBG)
else
CFLAGS += $(CFLAGS_REL)
endif
CFLAGS += $(CFLAGS_WRN)
CFLAGS += $(CFLAGS_LIBS)
CFLAGS += $(CFLAGS_DEP)
CFLAGS += -DVERSION=\"$(VERSION)\"
CFLAGS += $(CFLAGS_CUSTOM)

all : $(PRG) $(LIB)

$(LIB) : $(LIB_OBJS)
		$(CC) $^ -o $(LIB).$(VERSION) $(CFLAGS) $(CFLAGS_LIB) -shared -Wl,-soname,$(LIB) -Wl,-soname,$(LIB).$(VERSION)
		ln -snf $(LIB).$(VERSION) $(LIB)

$(PRG) : $(PRG_OBJS)
		$(CC) $^ -o $(PRG) $(CFLAGS)

clean :
		find . -name "*.o" -o -name "*.d" -o -name "*.so.*" -o -name "*.so" | xargs rm -f
		rm -f $(PRG)

$(LIB_OBJS) : %.o : %.c
		$(CC) -c $< -o $@ $(CFLAGS) $(CFLAGS_LIB)

$(PRG_OBJS) : %.o : %.c
		$(CC) -c $< -o $@ $(CFLAGS)

.PHONY : all clean

-include $(wildcard *.d)
