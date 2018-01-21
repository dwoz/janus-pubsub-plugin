#!/bin/make
UNAME_S := $(shell uname -s)
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
           `pkg-config --cflags glib-2.0 jansson` -D_POSIX_C_SOURCE=200112L -c -g
    INSTALL_DIR=/usr/local/janus/lib/janus/plugins/
    INSTALL_CONF_DIR=/usr/local/janus/etc/janus
    LDFLAGS=-dynamiclib -undefined suppress -flat_namespace \
            -o $(BUILD_DIR)/$(LIBOUTNAME)  $(BUILD_DIR)/$(LIBNAME).o
endif


LDFLAGS+= `pkg-config --libs glib-2.0 jansson`
BUILD_DIR=build
CONF_SRC=janus.plugin.pubsub.cfg.sample
CONF_OUT=janus.plugin.pubsub.cfg
HAVE_SRTP_2=yes


all: $(BUILD_DIR)/$(LIBNAME).o $(BUILD_DIR)/$(LIBOUTNAME) $(BUILD_DIR)/$(CONF_OUT)


$(BUILD_DIR)/$(LIBOUTNAME): $(BUILD_DIR)/$(LIBNAME).o
	gcc -shared $(LDFLAGS)


$(BUILD_DIR)/$(LIBNAME).o: $(BUILD_DIR)
	gcc $(CFLAGS) $(LIBNAME).c -o $(BUILD_DIR)/$(LIBNAME).o


$(BUILD_DIR)/$(CONF_OUT):
	cp $(CONF_SRC) $(BUILD_DIR)/$(CONF_OUT)


$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


clean:
	rm $(BUILD_DIR)/$(LIBOUTNAME)
	rm $(BUILD_DIR)/$(LIBNAME).o
	rm $(BUILD_DIR)/$(CONF_OUT)
	rm -r $(BUILD_DIR)


install:
	cp $(BUILD_DIR)/$(LIBOUTNAME) $(INSTALL_DIR)
	cp $(BUILD_DIR)/$(CONF_OUT) $(INSTALL_CONF_DIR)/$(CONF_OUT)
