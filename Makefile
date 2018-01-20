UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LIBNAME=janus_pubsub
    LIBOUTNAME=$(LIBNAME).so
    CFLAGS=-D LINUX -std=c99 -fpic -I. -I/usr/include/janus/ \
           `pkg-config --cflags glib-2.0 jansson` -D_POSIX_C_SOURCE=200112L -c -g
    LDFLAGS=`pkg-config --libs glib-2.0 jansson`
    BUILD_DIR=build
    INSTALL_DIR=/usr/lib/x86_64-linux-gnu/janus/plugins
    LDFLAGS=-o $(LIBOUT) $(BUILD_DIR)/$(LIBNAME).o
endif
ifeq ($(UNAME_S),Darwin)
    LIBNAME=janus_pubsub
    LIBOUTNAME=$(LIBNAME).0.dylib
    CFLAGS=-D OSX -std=c99 -fpic -I. -I/usr/local/janus/include/janus/ \
           `pkg-config --cflags glib-2.0 jansson` -D_POSIX_C_SOURCE=200112L -c -g
    INSTALL_DIR=/usr/local/janus/lib/janus/plugins/
    LDFLAGS=-dynamiclib -undefined suppress -flat_namespace -o $(BUILD_DIR)/$(LIBOUTNAME)  $(BUILD_DIR)/$(LIBNAME).o
endif

LDFLAGS+= `pkg-config --libs glib-2.0 jansson`
BUILD_DIR=build


all: $(BUILD_DIR)/$(LIBNAME).o $(BUILD_DIR)/$(LIBOUTNAME)


$(BUILD_DIR)/$(LIBOUTNAME): $(BUILD_DIR)/$(LIBNAME).o
	gcc -shared $(LDFLAGS)

$(BUILD_DIR)/$(LIBNAME).o: $(BUILD_DIR)
	gcc $(CFLAGS) $(LIBNAME).c -o $(BUILD_DIR)/$(LIBNAME).o

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
clean:
	rm $(BUILD_DIR)/$(LIBOUTNAME)
	rm $(BUILD_DIR)/$(LIBNAME).o
	rm -r $(BUILD_DIR)

install:
	cp $(BUILD_DIR)/$(LIBOUTNAME) $(INSTALL_DIR)
