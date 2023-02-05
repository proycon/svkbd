static const Bool wmborder = True;
static int fontsize = 22;
/* overlay delay in seconds */
static double overlay_delay = 1.0;
/* repeat delay in seconds, will not work on keys with overlays */
static double repeat_delay = 0.75;
/* scan rate in microseconds, affects key repetition rate */
static int scan_rate = 50;
/* one row of keys takes up 1/x of the screen height */
static int heightfactor = 14;
static int xspacing = 5;
static int yspacing = 5;
static const char *defaultfonts[] = {
	"DejaVu Sans:bold:size=22"
};
static const char *defaultcolors[SchemeLast][2] = {
	/*     fg         bg       */
	[SchemeNorm] = { "#bbbbbb", "#132a33" },
	[SchemeNormShift] = { "#008ac0", "#132a33" },
	[SchemeNormABC] = { "#ffffff", "#14313d" },
	[SchemeNormABCShift] = { "#008ac0", "#14313d" },
	[SchemePress] = { "#ffffff", "#259937" },
	[SchemePressShift] = { "#00c001", "#259937" },
	[SchemeHighlight] = { "#58a7c6", "#005577" },
	[SchemeHighlightShift] = { "#008ac0", "#005577" },
	[SchemeOverlay] = { "#ffffff", "#2b3313" },
	[SchemeOverlayShift] = { "#008ac0", "#2b3313" },
	[SchemeWindow] = { "#bbbbbb", "#132a33" },
};

