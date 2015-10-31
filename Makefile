CC ?= gcc
PKG_CONFIG := pkg-config

CFLAGS ?= -Wall -g
INCLUDES = `$(PKG_CONFIG) --cflags glib-2.0 gtk+-3.0 x11 gl cairo`
LDFLAGS ?= 
LIBS = `$(PKG_CONFIG) --libs glib-2.0 gtk+-3.0 x11 gl cairo` -lm -lpng

RCVERSION := '$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1), branch \"$(shell git describe --tags --always --all | sed s:heads/::)\")'

PREFIX ?= /usr

rm_SRC := $(wildcard *.c)
rm_OBJ := $(rm_SRC:.c=.o)
rm_HEADERS := $(wildcard *.h)

ifndef DISABLE_MSAA
	CFLAGS += -DWITH_MSAA
endif

ifdef DEBUG
	CFLAGS += -DDEBUG
endif

all: render-matrix

render-matrix: $(rm_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o: %.c $(rm_HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

install: render-matrix
	install render-matrix $(PREFIX)/bin

clean:
	rm -f render-matrix $(rm_OBJ)

.PHONY: all clean install
