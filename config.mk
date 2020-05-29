LAYOUT = en

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCS = -I. -I./layouts -I${X11INC}
LIBS = -L${X11LIB} -lX11 -lXtst

# use system flags
SVKBD_CFLAGS = ${CFLAGS}
SVKBD_LDFLAGS = ${LDFLAGS} ${LIBS}
SVKBD_CPPFLAGS = ${CPPFLAGS} ${INCS} -DVERSION=\"VERSION\" -DLAYOUT=\"layout.${LAYOUT}.h\"
