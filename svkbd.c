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
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>
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
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

#include "drw.h"
#include "util.h"


/* macros */
#define LENGTH(x)       (sizeof x / sizeof x[0])
#define TEXTW(X)        (drw_fontset_getwidth(drw, (X)))
#define STRINGTOKEYSYM(X)			(XStringToKeySym(X))

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
static int iscyclemod(KeySym keysym);
static void leavenotify(XEvent *e);
static void press(Key *k, KeySym mod);
static double get_press_duration();
static void run(void);
static void setup(void);
static void simulate_keypress(KeySym keysym);
static void simulate_keyrelease(KeySym keysym);
static void showoverlay(int idx);
static void cyclemod();
static void hideoverlay();
static void cyclelayer();
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
static struct timeval pressbegin;
static int currentlayer = 0;
static int currentoverlay = -1; // -1 = no overlay
static int currentcyclemod = 0;
static KeySym overlaykeysym = 0; //keysym for which the overlay is presented
static int releaseprotect = 0; //set to 1 after overlay is shown, protecting against immediate release
static int tmp_keycode = 1;
static int rows = 0, ww = 0, wh = 0, wx = 0, wy = 0;
static char *name = "svkbd";
static int debug = 0;

static KeySym ispressingkeysym;

Bool ispressing = False;
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
	drw_sync(drw);
	XSync(dpy, False);
	drw_free(drw);
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


int
hasoverlay(KeySym keysym) {
	int begin, i;
	begin = 0;
	for(i = 0; i < OVERLAYS; i++) {
		if(overlay[i].keysym == XK_Cancel) {
			begin = i+1;
		} else if(overlay[i].keysym == keysym) {
			return begin+1;
		}
	}
	return -1;
}

int
iscyclemod(KeySym keysym) {
	int i;
	for(i = 0; i < CYCLEMODS; i++) {
		if(cyclemods[i].keysym == keysym) {
			return i;
		}
	}
	return -1;
}

void
leavenotify(XEvent *e) {
	if (currentoverlay != -1) {
		hideoverlay();
	}
	unpress(NULL, 0);
}

void record_press_begin(KeySym ks) {
	//record the begin of the press, don't simulate the actual keypress yet
	gettimeofday(&pressbegin, NULL);
	ispressingkeysym = ks;
}

void
press(Key *k, KeySym mod) {
	int i;
	int overlayidx = -1;
	k->pressed = !k->pressed;

	if (debug) { printf("Begin press: %ld\n", k->keysym); fflush(stdout); }
	pressbegin.tv_sec = 0;
	pressbegin.tv_usec = 0;
	ispressingkeysym = 0;

	int cm = iscyclemod(k->keysym);
	if (cm != -1) {
		if (!pressbegin.tv_sec && !pressbegin.tv_usec) {
			//record the begin of the press, don't simulate the actual keypress yet
			record_press_begin(k->keysym);
		}
	} else if(!IsModifierKey(k->keysym)) {
		if (currentoverlay == -1)
			overlayidx = hasoverlay(k->keysym);
		if (overlayidx != -1) {
			if (!pressbegin.tv_sec && !pressbegin.tv_usec) {
				//record the begin of the press, don't simulate the actual keypress yet
				record_press_begin(k->keysym);
			}
		} else {
			if (debug) { printf("Simulating press: %ld\n", k->keysym); fflush(stdout); }
			for(i = 0; i < LENGTH(keys); i++) {
				if(keys[i].pressed && IsModifierKey(keys[i].keysym)) {
					simulate_keypress(keys[i].keysym);
				}
			}
			pressedmod = mod;
			if(pressedmod) {
				simulate_keypress(mod);
			}
			simulate_keypress(k->keysym);

			for(i = 0; i < LENGTH(keys); i++) {
				if(keys[i].pressed && IsModifierKey(keys[i].keysym)) {
					simulate_keyrelease(keys[i].keysym);
				}
			}
		}
	}
	drawkey(k);
}





int tmp_remap(KeySym keysym) {
	XChangeKeyboardMapping(dpy, tmp_keycode, 1, &keysym, 1);
	XSync(dpy, False);
	return tmp_keycode;
}

void
simulate_keypress(KeySym keysym) {
	KeyCode code = XKeysymToKeycode(dpy, keysym);
	if (code == 0)
		code = tmp_remap(keysym);
	XTestFakeKeyEvent(dpy, code, True, 0);
}

void
simulate_keyrelease(KeySym keysym) {
	KeyCode code = XKeysymToKeycode(dpy, keysym);
	if (code == 0)
		code = tmp_remap(keysym);
	XTestFakeKeyEvent(dpy, code, False, 0);
}


double get_press_duration() {
	struct timeval now;
	gettimeofday(&now, NULL);
	return (double) ((now.tv_sec * 1000000L + now.tv_usec) - (pressbegin.tv_sec * 1000000L + pressbegin.tv_usec)) / (double) 1000000L;
}

void
unpress(Key *k, KeySym mod) {
	int i;

	if(k != NULL) {
		switch(k->keysym) {
		case XK_Cancel:
			cyclelayer();
			break;
		case XK_Break:
		  running = False;
		default:
			break;
		}
	}


	if ((pressbegin.tv_sec || pressbegin.tv_usec) && k && k->keysym == ispressingkeysym) {
		if (currentoverlay == -1) {
			if (get_press_duration() < overlay_delay) {
				if (debug) { printf("Delayed simulation of press after release: %ld\n", k->keysym); fflush(stdout); }
				//simulate the press event, as we postponed it earlier in press()
				for(i = 0; i < LENGTH(keys); i++) {
					if(keys[i].pressed && IsModifierKey(keys[i].keysym)) {
						simulate_keypress(keys[i].keysym);
					}
				}
				pressedmod = mod;
				if(pressedmod) {
					simulate_keypress(mod);
				}
				simulate_keypress(k->keysym);
				pressbegin.tv_sec = 0;
				pressbegin.tv_usec = 0;
			} else {
				return;
			}
		}
	}

	if (debug) {
		if (k) {
			printf("Simulation of release: %ld\n", k->keysym); fflush(stdout);
		} else {
			printf("Simulation of release (all keys)"); fflush(stdout);
		}
	}


	for(i = 0; i < LENGTH(keys); i++) {
		if(keys[i].pressed && !IsModifierKey(keys[i].keysym)) {
			simulate_keyrelease(keys[i].keysym);
			keys[i].pressed = 0;
			drawkey(&keys[i]);
			break;
		}
	}
	if(i != LENGTH(keys)) {
		if(pressedmod) {
			simulate_keyrelease(mod);
		}
		pressedmod = 0;

		for(i = 0; i < LENGTH(keys); i++) {
			if(keys[i].pressed) {
				simulate_keyrelease(keys[i].keysym);
				keys[i].pressed = 0;
				drawkey(&keys[i]);
			}
		}
	}

	if (currentoverlay != -1) {
		if (releaseprotect) {
			releaseprotect = 0;
		} else {
			hideoverlay();
		}
	}
}

void
run(void) {
	XEvent ev;
	int xfd;
	fd_set fds;
	struct timeval tv;
	double duration = 0.0;
	int cyclemodidx;


	xfd = ConnectionNumber(dpy);
	tv.tv_usec = 0;
	tv.tv_sec = 1;


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
		} else {
			if (ispressing && ispressingkeysym) {
				duration = get_press_duration();
				if (debug == 2) { printf("%f\n", duration); fflush(stdout); }
				if (get_press_duration() >= overlay_delay) {
					if (debug) { printf("press duration %f\n", duration); fflush(stdout); }
					cyclemodidx = iscyclemod(ispressingkeysym);
					if (cyclemodidx != -1) {
						cyclemod();
					} else {
						showoverlay(hasoverlay(ispressingkeysym));
					}
					pressbegin.tv_sec = 0;
					pressbegin.tv_usec = 0;
					ispressingkeysym = 0;
				}
			}
		}
		usleep(100000L);
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

	//find an unused keycode to use as a temporary keycode (derived from source: https://stackoverflow.com/questions/44313966/c-xtest-emitting-key-presses-for-every-unicode-character)
	KeySym *keysyms = NULL;
	int keysyms_per_keycode = 0;
	int keycode_low, keycode_high;
	Bool key_is_empty;
	int symindex;
	XDisplayKeycodes(dpy, &keycode_low, &keycode_high);
	keysyms = XGetKeyboardMapping(dpy, keycode_low, keycode_high - keycode_low, &keysyms_per_keycode);
	for(i = keycode_low; i <= keycode_high; i++) {
		key_is_empty = True;
		for(j = 0; j < keysyms_per_keycode; j++) {
			symindex = (i - keycode_low) * keysyms_per_keycode + j;
			if(keysyms[symindex] != 0) {
				key_is_empty = False;
			} else {
				break;
			}
		}
		if (key_is_empty) {
			tmp_keycode = i;
			break;
		}
	}

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

	XFree(keysyms);
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
	fprintf(stderr, "usage: %s [-hdvD] [-g geometry] [-fn font]\n", argv0);
	exit(1);
}

void
cyclelayer() {
	currentlayer++;
	if (currentlayer >= LAYERS)
		currentlayer = 0;
	if (debug) { printf("Cycling to layer %d\n", currentlayer); fflush(stdout); }
	memcpy(&keys, layers[currentlayer], sizeof(keys_en));
	updatekeys();
	drawkeyboard();
}

void
cyclemod() {
	int i;
	//unpress all pressed keys
	for(i = 0; i < LENGTH(keys); i++) {
		if(keys[i].pressed) {
			keys[i].pressed = 0;
			drawkey(&keys[i]);
		}
	}
	pressedmod = 0;
	pressbegin.tv_sec = 0;
	pressbegin.tv_usec = 0;
	ispressingkeysym = 0;
	currentcyclemod++;
	if (currentcyclemod >= CYCLEMODS)
		currentcyclemod = 0;
	if (debug) { printf("Cycling modifier to %d\n", currentcyclemod); fflush(stdout); }
	keys[CYCLEMODKEY].label = cyclemods[currentcyclemod].label;
	keys[CYCLEMODKEY].keysym = cyclemods[currentcyclemod].keysym;
	drawkey(&keys[CYCLEMODKEY]);
	XSync(dpy, False);
}

void
showoverlay(int idx) {
	if (debug) { printf("Showing overlay %d\n", idx); fflush(stdout); }
	int i,j;
	//unpress existing key (visually only)
	for(i = 0; i < LENGTH(keys); i++) {
		if(keys[i].pressed && !IsModifierKey(keys[i].keysym)) {
			keys[i].pressed = 0;
			drawkey(&keys[i]);
			break;
		}
	}

	for (i = idx, j=0; i < OVERLAYS; i++, j++) {
		if (overlay[i].keysym == XK_Cancel) {
			break;
		}
		while (keys[j].keysym == 0) j++;
		keys[j].label = overlay[i].label;
		keys[j].keysym = overlay[i].keysym;
	}
	currentoverlay = idx;
	overlaykeysym = ispressingkeysym;
	releaseprotect = 1;
	updatekeys();
	drawkeyboard();
	XSync(dpy, False);
}

void
hideoverlay() {
	if (debug) { printf("Hiding overlay %d\n", currentoverlay); fflush(stdout); }
	currentoverlay = -1;
	overlaykeysym = 0;
	currentlayer = -1;
	cyclelayer();
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
		} else if (!strcmp(argv[i], "-fn")) { /* font or font set */
			fonts[0] = argv[++i];
		} else if(!strcmp(argv[i], "-D")) {
			debug = 1;
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
