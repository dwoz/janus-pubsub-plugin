# WIP
tests: src/common.0.dylib
       gcc -v -std=c99 -g \
         -DUNIT_TESTING \
         `pkg-config --cflags glib-2.0 jansson cmocka` \
         `pkg-config --libs glib-2.0 jansson cmocka` \
          -I./src \
          -I /Users/daniel.wozniak/local/include \
          -L/Users/daniel.wozniak/local/lib \
         -o tests \
          src/tests/test.c src/common.0.dylib

src/common.0.dylib: src/common.o
       gcc -v -g -shared -dynamiclib \
         -DUNIT_TESTING \
         $(PKG_CFG_CFLAGS) \
         -o src/common.0.dylib src/common.o

src/common.o:
	gcc -v -g -fPIC -c \
         $(PKG_CFG_CFLAGS) \
         -DUNIT_TESTING \
         -o src/common.o src/common.c
