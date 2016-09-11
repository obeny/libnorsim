CC=gcc

LIB_OBJS := libnorsim.o
PRG_OBJS := main.o

CFLAGS := -pipe -std=c11 -D_GNU_SOURCE
CFLAGS_LIB := -fPIC
CFLAGS_WRN := -Wall -Wextra -Wno-unused-result
CFLAGS_DBG := -ggdb -O0
CFLAGS_REL := -g0 -O2

CFLAGS_LIBS := -ldl

LIB_NAME := norsim
PRG := main
LIB := lib$(LIB_NAME).so

ifdef BUILD_DEBUG
CFLAGS += $(CFLAGS_DBG)
else
CFLAGS += $(CFLAGS_REL)
endif
CFLAGS += $(CFLAGS_WRN)
CFLAGS += $(CFLAGS_LIBS)

all : $(PRG) $(LIB)

$(LIB) : $(LIB_OBJS)
		$(CC) $^ -o $(LIB) $(CFLAGS) $(CFLAGS_LIB) -shared -Wl,-soname,$(LIB)

$(PRG) : $(PRG_OBJS) $(LIB)
		$(CC) $^ -o $(PRG) $(CFLAGS) -L. -l$(LIB_NAME)

clean :
		find . -name "*.o" | xargs rm -f
		rm -f $(PRG) $(LIB)

$(LIB_OBJS) : %.o : %.c
		$(CC) -c $< -o $@ $(CFLAGS) $(CFLAGS_LIB)

$(PRG_OBJS) : %.o : %.c
		$(CC) -c $< -o $@ $(CFLAGS)

.PHONY : all clean
