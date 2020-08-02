LAYOUT = sxmo

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xinerama, comment if you don't want it
XINERAMALIBS = -L${X11LIB} -lXinerama
XINERAMAFLAGS = -DXINERAMA

# includes and libs
INCS = -I. -I./layouts -I${X11INC} -I/usr/include/freetype2
LIBS = -L${X11LIB} -lX11 -lXtst -lfontconfig -lXft ${XINERAMALIBS}

# use system flags
SVKBD_CFLAGS = ${CFLAGS}
SVKBD_LDFLAGS = ${LDFLAGS} ${LIBS}
SVKBD_CPPFLAGS = ${CPPFLAGS} ${INCS} -DVERSION=\"VERSION\" ${XINERAMAFLAGS} -DLAYOUT=\"layout.${LAYOUT}.h\"
