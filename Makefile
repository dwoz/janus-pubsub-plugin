.PHONY: clean all install

LIB_NAME=libjanus_pubsub
CFLAGS = -v -std=c99 -g -D HAVE_SRTP_2
LDFLAGS = -v -std=c99 -g -shared
INCLUDE_DIRS = -I./src

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  CFLAGS+=-DLINUX
  LIB_EXT=.so
  INSTALL_LIB_DIR=/usr/lib/x86_64-linux-gnu/janus/plugins
  INSTALL_CONF_DIR=/etc/janus/
endif
ifeq ($(UNAME_S),Darwin)
  CFLAGS += -DOSX
  LIB_EXT = .0.dylib
  LDFLAGS += -dynamiclib -undefined dynamic_lookup
  INCLUDE_DIRS += \
    -I/usr/local/janus/include/janus/ \
    -I/usr/local/opt/openssl/include \
    -I/usr/local/opt/curl/include
  INSTALL_LIB_DIR=/usr/local/janus/lib/janus/plugins/
  INSTALL_CONF_DIR=/usr/local/janus/etc/janus
endif

LIB_OUT_NAME=$(LIB_NAME)$(LIB_EXT)
PKG_CFG_CFLAGS = `pkg-config --cflags glib-2.0 jansson libcurl`
PKG_CFG_LDFLAGS = `pkg-config --libs glib-2.0 jansson libcurl`

PUBSUB_LIBS = \
  src/janus_pubsub.o \
  src/session.o \
  src/stream.o

CFLAGS += $(PKG_CFG_CFLAGS)
CFLAGS += $(INCLUDE_DIRS)
src = $(wildcard src/*.c)
obj = $(src:.c=.o)


all: $(LIB_OUT_NAME)

$(LIB_OUT_NAME): $(obj)
	$(CC) $(LDFLAGS)  \
	 -o $(LIB_OUT_NAME) $^

clean:
	rm -f $(obj)
	rm -f $(LIB_OUT_NAME)

install:
	cp $(LIB_OUT_NAME) $(INSTALL_LIB_DIR)
	cp janus.plugin.pubsub.cfg.sample $(INSTALL_CONF_DIR)
