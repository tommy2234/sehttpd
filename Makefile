.PHONY: all check clean
TARGET = sehttpd
GIT_HOOKS := .git/hooks/applied
all: $(GIT_HOOKS) $(TARGET)

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

include common.mk

CFLAGS = -I./src
CFLAGS += -O2
CFLAGS += -std=gnu99 -Wall -W -g
CFLAGS += -DUNUSED="__attribute__((unused))"
CFLAGS += -DNDEBUG
LDFLAGS = -lpthread -luring -g

# standard build rules
.SUFFIXES: .o .c
.c.o:
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) -o $@ $(CFLAGS) -c -MMD -MF $@.d $<

OBJS = \
    src/http.o \
    src/http_parser.o \
    src/http_request.o \
	src/io_uring.o \
	src/memory_pool.o \
    src/mainloop.o
deps += $(OBJS:%.o=%.o.d)

$(TARGET): $(OBJS)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) -o $@ $^ $(LDFLAGS)

check: all
	@scripts/test.sh

htstress: src/htstress.c
	$(CC) -o $@ $< -g -lpthread

clean:
	$(VECHO) "  Cleaning...\n"
	$(Q)$(RM) $(TARGET) $(OBJS) $(deps) htstress

-include $(deps)
