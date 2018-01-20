LIBNAME=janus_pubsub
CFLAGS=-std=c99 -fpic -I. -I/usr/include/janus/ `pkg-config --cflags glib-2.0 jansson` -D_POSIX_C_SOURCE=200112L -c -g
LDFLAGS=`pkg-config --libs glib-2.0 jansson`
BUILD_DIR=build
INSTALL_DIR=/usr/lib/x86_64-linux-gnu/janus/plugins

all: $(BUILD_DIR)/$(LIBNAME).o $(BUILD_DIR)/$(LIBNAME).so


$(BUILD_DIR)/$(LIBNAME).so: $(BUILD_DIR)/$(LIBNAME).o
	gcc -shared -o $(BUILD_DIR)/$(LIBNAME).so $(BUILD_DIR)/$(LIBNAME).o -lpthread $(LDFLAGS)


$(BUILD_DIR)/$(LIBNAME).o: $(BUILD_DIR)
	gcc $(CFLAGS) $(LIBNAME).c -o $(BUILD_DIR)/$(LIBNAME).o

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
clean:
	rm $(BUILD_DIR)/$(LIBNAME).o
	rm $(BUILD_DIR)/$(LIBNAME).so
	rm -r $(BUILD_DIR)

install:
	cp $(BUILD_DIR)/$(LIBNAME).so $(INSTALL_DIR)
