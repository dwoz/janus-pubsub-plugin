#!/bin/make
UNAME_S := $(shell uname -s)
.PHONY: clean common
ifeq ($(UNAME_S),Linux)
    LIBNAME=janus_pubsub
    LIBOUTNAME=$(LIBNAME).so
    CFLAGS=-D LINUX -D HAVE_SRTP_2 -std=c99 -fpic -I. -I/usr/include/janus/ \
           `pkg-config --cflags glib-2.0 jansson` -D_POSIX_C_SOURCE=200112L -c -g
    LDFLAGS=`pkg-config --libs glib-2.0 jansson`
    BUILD_DIR=build
    # XXX: These are debian specific install locations (ones where a debian
    # package would install too). They should be generic instead.
    INSTALL_DIR=/usr/lib/x86_64-linux-gnu/janus/plugins
    INSTALL_CONF_DIR=/etc/janus/
    LDFLAGS=-o $(LIBOUT) $(BUILD_DIR)/$(LIBNAME).o
endif
ifeq ($(UNAME_S),Darwin)
    LIBNAME=janus_pubsub
    LIBOUTNAME=$(LIBNAME).0.dylib
    CFLAGS=-D OSX -D HAVE_SRTP_2 -std=c99 -fpic -I. -I/usr/local/janus/include/janus/ \
           -Duint="unsigned int" \
           -I/usr/local/opt/openssl/include \
           -I/usr/local/opt/curl/include \
           `pkg-config --cflags glib-2.0 jansson libavformat libavcodec libavutil` -D_POSIX_C_SOURCE=200112L -c -g
    INSTALL_DIR=/usr/local/janus/lib/janus/plugins/
    INSTALL_CONF_DIR=/usr/local/janus/etc/janus
    LDFLAGS=-dynamiclib -undefined suppress -flat_namespace \
            -o $(BUILD_DIR)/$(LIBOUTNAME)  $(BUILD_DIR)/$(LIBNAME).o
endif

LDFLAGS+= `pkg-config --libs glib-2.0 jansson libavformat libavcodec libavutil` \
          -L/usr/local/opt/curl/lib
BUILD_DIR=build
CONF_SRC=janus.plugin.pubsub.cfg.sample
CONF_OUT=janus.plugin.pubsub.cfg
HAVE_SRTP_2=yes

all: $(BUILD_DIR)/$(LIBNAME).o $(BUILD_DIR)/$(LIBOUTNAME) $(BUILD_DIR)/$(CONF_OUT)
LIBS=-L/Users/daniel.wozniak/local -lavformat -lm -lbz2 -lz -Wl \
   `pkg-config --libs glib-2.0 jansson` `pkg-config --cflags glib-2.0 jansson`




common: $(BUILD_DIR)/common.0.dylib

$(BUILD_DIR)/common.0.dylib: $(BUILD_DIR)/common.o
	gcc -shared -dynamiclib -undefined suppress -flat_namespace \
            -o $(BUILD_DIR)/common.0.dylib  $(BUILD_DIR)/common.o

$(BUILD_DIR)/common.o: $(BUILD_DIR)
	gcc $(CFLAGS) -g -g0 src/common.c -o $(BUILD_DIR)/common.o


tests:
	gcc -v -std=c99 -g -g0 -o tests -I./src -DDMALLOC -DDMALLOC_FUNC_CHECK \
	  `pkg-config --libs glib-2.0 jansson cmocka` `pkg-config --cflags glib-2.0 jansson cmocka`  \
          -I /Users/daniel.wozniak/local/include -L/Users/daniel.wozniak/local/lib \
          -L$(BUILD_DIR) src/tests/test.c $(BUILD_DIR)/common.0.dylib

test:
##	gcc -o test \
##          -I/Users/daniel.wozniak/local \
##          $(LIBS) test.c
	gcc -std=c99 -g -g0 -o test -I. \
          `PKG_CONFIG_PATH=/Users/daniel.wozniak/local/lib/pkgconfig/ pkg-config --cflags libavformat libavcodec libavutil` \
          `PKG_CONFIG_PATH=/Users/daniel.wozniak/local/lib/pkgconfig/ pkg-config --libs libavformat libavcodec libavutil` \
          `pkg-config --libs glib-2.0 jansson` `pkg-config --cflags glib-2.0 jansson`  test.c

listen:
#	gcc -o test \
#          -I/Users/daniel.wozniak/local \
#          $(LIBS) test.c
	gcc -std=c99 -g -g0 -o listen -I. \
          `PKG_CONFIG_PATH=/Users/daniel.wozniak/local/lib/pkgconfig/ pkg-config --cflags libavformat libavcodec libavutil` \
          `PKG_CONFIG_PATH=/Users/daniel.wozniak/local/lib/pkgconfig/ pkg-config --libs libavformat libavcodec libavutil` \
          `pkg-config --libs glib-2.0 jansson` `pkg-config --cflags glib-2.0 jansson` listen.c


alloc:
	gcc -o alloc alloc.c $(CFLAGS) $(LIBS)

$(BUILD_DIR)/$(LIBOUTNAME): $(BUILD_DIR)/$(LIBNAME).o
	gcc -shared $(LDFLAGS)


$(BUILD_DIR)/$(LIBNAME).o: $(BUILD_DIR)
	gcc $(CFLAGS) src/$(LIBNAME).c -o $(BUILD_DIR)/$(LIBNAME).o


$(BUILD_DIR)/$(CONF_OUT):
	cp $(CONF_SRC) $(BUILD_DIR)/$(CONF_OUT)


$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


clean:
	rm $(BUILD_DIR)/$(LIBOUTNAME)
	rm $(BUILD_DIR)/$(LIBNAME).o
	rm $(BUILD_DIR)/$(CONF_OUT)
	rm -r $(BUILD_DIR)
	rm tests


install:
	cp $(BUILD_DIR)/$(LIBOUTNAME) $(INSTALL_DIR)
	cp $(BUILD_DIR)/$(CONF_OUT) $(INSTALL_CONF_DIR)/$(CONF_OUT)
