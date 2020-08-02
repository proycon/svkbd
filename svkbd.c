/* See LICENSE file for copyright and license details.
 *
 * To understand svkbd, start reading main().
 */
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/extensions/XTest.h>
#include <X11/Xft/Xft.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <signal.h>
#include <sys/select.h>

#include "drw.h"
#include "util.h"



/* macros */
#define LENGTH(x)       (sizeof x / sizeof x[0])
#define TEXTW(X)        (drw_fontset_getwidth(drw, (X)))

/* enums */
enum { SchemeNorm, SchemePress, SchemeHighlight, SchemeLast };
enum { NetWMWindowType, NetLast };

/* typedefs */
typedef unsigned int uint;
typedef unsigned long ulong;

typedef struct {
	char *label;
	KeySym keysym;
	uint width;
	int x, y, w, h;
	Bool pressed;
	Bool highlighted;
} Key;

typedef struct {
	KeySym mod;
	uint button;
} Buttonmod;

/* function declarations */
static void motionnotify(XEvent *e);
static void buttonpress(XEvent *e);
static void buttonrelease(XEvent *e);
static void cleanup(void);
static void configurenotify(XEvent *e);
static void countrows();
static void drawkeyboard(void);
static void drawkey(Key *k);
static void expose(XEvent *e);
static Key *findkey(int x, int y);
static void leavenotify(XEvent *e);
static void press(Key *k, KeySym mod);
static void run(void);
static void setup(void);
static void togglelayer();
static void unpress(Key *k, KeySym mod);
static void updatekeys();

/* variables */
static int screen;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ButtonRelease] = buttonrelease,
	[ConfigureNotify] = configurenotify,
	[Expose] = expose,
	[LeaveNotify] = leavenotify,
	[MotionNotify] = motionnotify
};
static Atom netatom[NetLast];
static Display *dpy;
static Drw *drw;
static Window root, win;
static Clr* scheme[SchemeLast];
static Bool running = True, isdock = False;
static KeySym pressedmod = 0;
static int rows = 0, ww = 0, wh = 0, wx = 0, wy = 0;
static char *name = "svkbd";

Bool ispressing = False;
Bool baselayer = True;
Bool sigtermd = False;

/* configuration, allows nested code to access above variables */
#include "config.h"
#ifndef LAYOUT
#error "make sure to define LAYOUT"
#endif
#include LAYOUT

void
motionnotify(XEvent *e)
{
	XPointerMovedEvent *ev = &e->xmotion;
	int i;

	for(i = 0; i < LENGTH(keys); i++) {
		if(keys[i].keysym && ev->x > keys[i].x
				&& ev->x < keys[i].x + keys[i].w
				&& ev->y > keys[i].y
				&& ev->y < keys[i].y + keys[i].h) {
			if(keys[i].highlighted != True) {
				if(ispressing) {
					keys[i].pressed = True;
				} else {
					keys[i].highlighted = True;
				}
				drawkey(&keys[i]);
			}
			continue;
		}

		if(!IsModifierKey(keys[i].keysym) && keys[i].pressed == True) {
			unpress(&keys[i], 0);

			drawkey(&keys[i]);
		}
		if(keys[i].highlighted == True) {
			keys[i].highlighted = False;
			drawkey(&keys[i]);
		}
	}
}

void
buttonpress(XEvent *e) {
	int i;
	XButtonPressedEvent *ev = &e->xbutton;
	Key *k;
	KeySym mod = 0;

	ispressing = True;

	for(i = 0; i < LENGTH(buttonmods); i++) {
		if(ev->button == buttonmods[i].button) {
			mod = buttonmods[i].mod;
			break;
		}
	}
	if((k = findkey(ev->x, ev->y)))
		press(k, mod);
}

void
buttonrelease(XEvent *e) {
	int i;
	XButtonPressedEvent *ev = &e->xbutton;
	Key *k;
	KeySym mod = 0;

	ispressing = False;

	for(i = 0; i < LENGTH(buttonmods); i++) {
		if(ev->button == buttonmods[i].button) {
			mod = buttonmods[i].mod;
			break;
		}
	}

	if(ev->x < 0 || ev->y < 0) {
		unpress(NULL, mod);
	} else {
		if((k = findkey(ev->x, ev->y)))
			unpress(k, mod);
	}
}

void
cleanup(void) {
	int i;

	// E.g. Generally in scripts we call SIGTERM on svkbd in which case
	//      if the user is holding for example the enter key (to execute
	//      the kill or script that does the kill), that causes an issue
	//      since then X doesn't know the keyup is never coming.. (since
	//      process will be dead before finger lifts - in that case we
	//      just trigger out fake up presses for all keys
	if (sigtermd) {
		for (i = 0; i < LENGTH(keys); i++) {
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, keys[i].keysym), False, 0);
		}
	}

	for (i = 0; i < SchemeLast; i++)
		free(scheme[i]);
	drw_free(drw);
	drw_sync(drw);
	XSync(dpy, False);
	XDestroyWindow(dpy, win);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void
configurenotify(XEvent *e) {
	XConfigureEvent *ev = &e->xconfigure;

	if(ev->window == win && (ev->width != ww || ev->height != wh)) {
		ww = ev->width;
		wh = ev->height;
		drw_resize(drw, ww, wh);
		updatekeys();
	}
}

void
countrows() {
	int i = 0;

	for(i = 0, rows = 1; i < LENGTH(keys); i++) {
		if(keys[i].keysym == 0)
			rows++;
	}
}


void
drawkeyboard(void) {
	int i;

	for(i = 0; i < LENGTH(keys); i++) {
		if(keys[i].keysym != 0)
			drawkey(&keys[i]);
	}
}

void
drawkey(Key *k) {
	int x, y, w, h;
	const char *l;

	if(k->pressed)
		drw_setscheme(drw, scheme[SchemePress]);
	else if(k->highlighted)
		drw_setscheme(drw, scheme[SchemeHighlight]);
	else
		drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, k->x, k->y, k->w, k->h, 1, 1);
	drw_rect(drw, k->x, k->y, k->w, k->h, 0, 0);

	if(k->label) {
		l = k->label;
	} else {
		l = XKeysymToString(k->keysym);
	}
	h = fontsize * 2;
	y = k->y + (k->h / 2) - (h / 2);
	w = TEXTW(l);
	x = k->x + (k->w / 2) - (w / 2);
	drw_text(drw, x, y, w, h, 0, l, 0);
	drw_map(drw, win, k->x, k->y, k->w, k->h);
}

void
expose(XEvent *e) {
	XExposeEvent *ev = &e->xexpose;

	if(ev->count == 0 && (ev->window == win))
		drawkeyboard();
}

Key *
findkey(int x, int y) {
	int i;

	for(i = 0; i < LENGTH(keys); i++) {
		if(keys[i].keysym && x > keys[i].x &&
				x < keys[i].x + keys[i].w &&
				y > keys[i].y && y < keys[i].y + keys[i].h) {
			return &keys[i];
		}
	}
	return NULL;
}



void
leavenotify(XEvent *e) {
	unpress(NULL, 0);
}

void
press(Key *k, KeySym mod) {
	int i;
	k->pressed = !k->pressed;

	if(!IsModifierKey(k->keysym)) {
		for(i = 0; i < LENGTH(keys); i++) {
			if(keys[i].pressed && IsModifierKey(keys[i].keysym)) {
				XTestFakeKeyEvent(dpy,
					XKeysymToKeycode(dpy, keys[i].keysym),
					True, 0);
			}
		}
		pressedmod = mod;
		if(pressedmod) {
			XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, mod),
					True, 0);
		}
		XTestFakeKeyEvent(dpy, XKeysymToKeycode(dpy, k->keysym), True, 0);

		for(i = 0; i < LENGTH(keys); i++) {
			if(keys[i].pressed && IsModifierKey(keys[i].keysym)) {
				XTestFakeKeyEvent(dpy,
					XKeysymToKeycode(dpy, keys[i].keysym),
					False, 0);
			}
		}
	}
	drawkey(k);
}

void
unpress(Key *k, KeySym mod) {
	int i;

	if(k != NULL) {
		switch(k->keysym) {
		case XK_Cancel:
			togglelayer();
			break;
		case XK_Break:
		  running = False;
		default:
			break;
		}
	}

	for(i = 0; i < LENGTH(keys); i++) {
		if(keys[i].pressed && !IsModifierKey(keys[i].keysym)) {
			XTestFakeKeyEvent(dpy,
				XKeysymToKeycode(dpy, keys[i].keysym),
				False, 0);
			keys[i].pressed = 0;
			drawkey(&keys[i]);
			break;
		}
	}
	if(i != LENGTH(keys)) {
		if(pressedmod) {
			XTestFakeKeyEvent(dpy,
				XKeysymToKeycode(dpy, pressedmod),
				False, 0);
		}
		pressedmod = 0;

		for(i = 0; i < LENGTH(keys); i++) {
			if(keys[i].pressed) {
				XTestFakeKeyEvent(dpy,
					XKeysymToKeycode(dpy,
						keys[i].keysym), False, 0);
				keys[i].pressed = 0;
				drawkey(&keys[i]);
			}
		}
	}
}

void
run(void) {
	XEvent ev;
	int xfd;
	fd_set fds;
	struct timeval tv;


	xfd = ConnectionNumber(dpy);
	tv.tv_usec = 0;
	tv.tv_sec = 2;

	//XSync(dpy, False);
	XFlush(dpy);

	while (running) {
		FD_ZERO(&fds);
		FD_SET(xfd, &fds);
		if (select(xfd + 1, &fds, NULL, NULL, &tv)) {
			while (XPending(dpy)) {
				XNextEvent(dpy, &ev);
				if(handler[ev.type]) {
					(handler[ev.type])(&ev); /* call handler */
				}
			}
		}
	}
}

void
setup(void) {
	XSetWindowAttributes wa;
	XTextProperty str;
	XSizeHints *sizeh = NULL;
	XClassHint *ch;
	Atom atype = -1;
	int i, j, sh, sw;
	XWMHints *wmh;

	#if XINERAMA
	XineramaScreenInfo *info = NULL;
	#endif

	/* init screen */
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	#if XINERAMA
	if(XineramaIsActive(dpy)) {
		info = XineramaQueryScreens(dpy, &i);
		sw = info[0].width;
		sh = info[0].height;
		XFree(info);
	} else
	#endif
	{
		sw = DisplayWidth(dpy, screen);
		sh = DisplayHeight(dpy, screen);
	}
    drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
    drw_setscheme(drw, scheme[SchemeNorm]);

	/* init appearance */
	for (j = 0; j < SchemeLast; j++)
		scheme[j] = drw_scm_create(drw, colors[j], 2);

	/* init atoms */
	if(isdock) {
		netatom[NetWMWindowType] = XInternAtom(dpy,
				"_NET_WM_WINDOW_TYPE", False);
		atype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	}

	/* init appearance */
	countrows();
	if(!ww)
		ww = sw;
	if(!wh)
		wh = sh * rows / 32;

	if(!wx)
		wx = 0;
	if(wx < 0)
		wx = sw + wx - ww;
	if(!wy)
		wy = sh - wh;
	if(wy < 0)
		wy = sh + wy - wh;

	for(i = 0; i < LENGTH(keys); i++)
		keys[i].pressed = 0;

	wa.override_redirect = !wmborder;
	wa.border_pixel = scheme[SchemeNorm][ColFg].pixel;
	wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
	win = XCreateWindow(dpy, root, wx, wy, ww, wh, 0,
			    CopyFromParent, CopyFromParent, CopyFromParent,
			    CWOverrideRedirect | CWBorderPixel |
			    CWBackingPixel, &wa);
	XSelectInput(dpy, win, StructureNotifyMask|ButtonReleaseMask|
			ButtonPressMask|ExposureMask|LeaveWindowMask|
			PointerMotionMask);

	wmh = XAllocWMHints();
	wmh->input = False;
	wmh->flags = InputHint;
	if(!isdock) {
		sizeh = XAllocSizeHints();
		sizeh->flags = PMaxSize | PMinSize;
		sizeh->min_width = sizeh->max_width = ww;
		sizeh->min_height = sizeh->max_height = wh;
	}
	XStringListToTextProperty(&name, 1, &str);
	ch = XAllocClassHint();
	ch->res_class = name;
	ch->res_name = name;

	XSetWMProperties(dpy, win, &str, &str, NULL, 0, sizeh, wmh,
			ch);

	XFree(ch);
	XFree(wmh);
	XFree(str.value);
	if(sizeh != NULL)
		XFree(sizeh);

	if(isdock) {
		XChangeProperty(dpy, win, netatom[NetWMWindowType], XA_ATOM,
				32, PropModeReplace,
				(unsigned char *)&atype, 1);
	}

	XMapRaised(dpy, win);
	drw_resize(drw, ww, wh);
	updatekeys();
	drawkeyboard();
}


void
updatekeys() {
	int i, j;
	int x = 0, y = 0, h, base, r = rows;

	h = (wh - 1) / rows;
	for(i = 0; i < LENGTH(keys); i++, r--) {
		for(j = i, base = 0; j < LENGTH(keys) && keys[j].keysym != 0; j++)
			base += keys[j].width;
		for(x = 0; i < LENGTH(keys) && keys[i].keysym != 0; i++) {
			keys[i].x = x;
			keys[i].y = y;
			keys[i].w = keys[i].width * (ww - 1) / base;
			keys[i].h = r == 1 ? wh - y - 1 : h;
			x += keys[i].w;
		}
		if(base != 0)
			keys[i - 1].w = ww - 1 - keys[i - 1].x;
		y += h;
	}
}

void
usage(char *argv0) {
	fprintf(stderr, "usage: %s [-hdv] [-g geometry]\n", argv0);
	exit(1);
}

void
togglelayer() {
	memcpy(&keys, baselayer ? &keys_symbols : &keys_en, sizeof(keys_en));
	updatekeys();
	drawkeyboard();
	baselayer = !baselayer;
}

void
sigterm(int sig)
{
	running = False;
	sigtermd = True;
}

int
main(int argc, char *argv[]) {
	int i, xr, yr, bitm;
	unsigned int wr, hr;

	memcpy(&keys, &keys_en, sizeof(keys_en));
	signal(SIGTERM, sigterm);
	for (i = 1; argv[i]; i++) {
		if(!strcmp(argv[i], "-v")) {
			die("svkbd-"VERSION", © 2006-2020 svkbd engineers,"
				       " see LICENSE for details\n");
		} else if(!strcmp(argv[i], "-d")) {
			isdock = True;
			continue;
		} else if(!strncmp(argv[i], "-g", 2)) {
			if(i >= argc - 1)
				continue;

			bitm = XParseGeometry(argv[i+1], &xr, &yr, &wr, &hr);
			if(bitm & XValue)
				wx = xr;
			if(bitm & YValue)
				wy = yr;
			if(bitm & WidthValue)
				ww = (int)wr;
			if(bitm & HeightValue)
				wh = (int)hr;
			if(bitm & XNegative && wx == 0)
				wx = -1;
			if(bitm & YNegative && wy == 0)
				wy = -1;
			i++;
		} else if(!strcmp(argv[i], "-h")) {
			usage(argv[0]);
		}
	}

	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fprintf(stderr, "warning: no locale support\n");
	if(!(dpy = XOpenDisplay(0)))
		die("svkbd: cannot open display\n");
	setup();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return 0;
}

