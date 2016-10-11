VERSION = 1.0

CC ?= gcc
CXX ?= g++

LIB_OBJS := Libnorsim.o Libnorsim_helpers.o libnorsim_iface.o PageManager.o SyscallsCache.o
PRG_OBJS := main.o

CFLAGS := -pipe -D_GNU_SOURCE=1 -fstack-protector-all
CFLAGS_WRN := -Wall -Wextra
CFLAGS_WRN += -Wnonnull -Winit-self -Wignored-qualifiers -Wunused -Wundef -Wshadow -Wpointer-arith
CFLAGS_WRN += -Wcast-align -Wwrite-strings -Waggregate-return -Wmissing-declarations
CFLAGS_WRN += -Wmissing-noreturn -Winline -Wno-unused-result -Wno-missing-declarations
CFLAGS_DBG := -ggdb -O0
CFLAGS_REL := -g0 -O3 -flto
CFLAGS_DEP := -MD -MP

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
CFLAGS += -DLOGGERFILE_ENABLE
CFLAGS += -DLOGGER_PRINTBUFFER_SIZE="256"
CFLAGS += $(CFLAGS_CUSTOM)

CXXFLAGS := $(CFLAGS) -std=c++14
CFLAGS += -std=c11

all : $(PRG) $(LIB)

$(LIB) : $(LIB_OBJS)
		$(CXX) $^ -o $(LIB).$(VERSION) $(CXXFLAGS) -shared -Wl,-soname,$(LIB) -Wl,-soname,$(LIB).$(VERSION) -ldl
		ln -snf $(LIB).$(VERSION) $(LIB)

$(PRG) : $(PRG_OBJS)
		$(CC) $^ -o $(PRG) $(CFLAGS)

clean :
		find . -name "*.o" -o -name "*.d" -o -name "*.so.*" -o -name "*.so" | xargs rm -f
		rm -f $(PRG)

$(LIB_OBJS) : %.o : %.cpp
		$(CXX) -c $< -o $@ $(CXXFLAGS) -fPIC

$(PRG_OBJS) : %.o : %.c
		$(CC) -c $< -o $@ $(CFLAGS)

.PHONY : all clean

-include $(wildcard *.d)
