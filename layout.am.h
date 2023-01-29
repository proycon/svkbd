#define KEYS 63
static Key keys_am[] = {
	{ "՝","՜", XK_Armenian_exclam, 1 },
	{ "ֆ","Ֆ", XK_Armenian_fe, 1 },
	{ "ձ","Ձ", XK_Armenian_dza, 1 },
	{ "֊","—", XK_Armenian_hyphen, 1 },
	{ ",","՟", XK_comma, 1 },
	{ "։","…", XK_Armenian_full_stop, 1 },
	{ "՞","%", XK_Armenian_question, 1 },
	{ "․","և", XK_Armenian_ligature_ew, 1 },
	{ "՛","՚", XK_Armenian_accent, 1 },
	{ ")","(", XK_parenright, 1 },
	{ "օ","Օ", XK_Armenian_o, 1 },
	{ "է","Է", XK_Armenian_e, 1 },
	{ "ղ","Ղ", XK_Armenian_ghat, 1 },
	{ "<-", 0, XK_BackSpace, 2 },
	{ "Del", 0, XK_Delete, 1},
	{ 0 }, /* New row */
	{ "->|", 0, XK_Tab, 1 },
	{ "ճ", "Ճ", XK_Armenian_tche, 1 },
	{ "փ", "Փ", XK_Armenian_pyur, 1 },
	{ "բ", "Բ", XK_Armenian_ben, 1 },
	{ "ս", "Ս", XK_Armenian_se, 1 },
	{ "մ", "Մ", XK_Armenian_men, 1 },
	{ "ո", "Ո", XK_Armenian_vo, 1 },
	{ "ւ", "Ւ", XK_Armenian_vyun, 1 },
	{ "կ","Կ", XK_Armenian_ken, 1 },
	{ "ը", "Ը", XK_Armenian_at, 1 },
	{ "թ", "Թ", XK_Armenian_to, 1 },
	{ "ծ", "Ծ", XK_Armenian_tsa, 1 },
	{ "ց", "Ց", XK_Armenian_tso, 1 },
	{ "Return", 0, XK_Return, 3 },
	{ 0 }, /* New row */
	{ 0, 0, XK_Caps_Lock, 2 },
	{ "ջ", "Ջ", XK_Armenian_je, 1 },
	{ "վ", "Վ", XK_Armenian_vev, 1 },
	{ "գ", "Գ", XK_Armenian_gim, 1 },
	{ "ե", "Ե", XK_Armenian_yech, 1 },
	{ "ա", "Ա", XK_Armenian_ayb, 1 },
	{ "ն", "Ն", XK_Armenian_nu, 1 },
	{ "ի", "Ի", XK_Armenian_ini, 1 },
	{ "տ", "Տ", XK_Armenian_tyun, 1 },
	{ "հ", "Հ", XK_Armenian_ho, 1 },
	{ "պ", "Պ", XK_Armenian_pe, 1 },
	{ "ր", "Ր", XK_Armenian_re, 1 },
	{ "\\","/", XK_backslash, 1 },
	{ 0 }, /* New row */
	{ 0, 0, XK_Shift_L, 3 },
	{ "ժ", "Ժ", XK_Armenian_zhe, 1 },
	{ "դ", "Դ", XK_Armenian_da, 1 },
	{ "չ", "Չ", XK_Armenian_cha, 1 },
	{ "յ", "Յ", XK_Armenian_hi, 1 },
	{ "զ", "Զ", XK_Armenian_za, 1 },
	{ "լ", "Լ", XK_Armenian_lyun, 1 },
	{ "ք", "Ք", XK_Armenian_ke, 1 },
	{ "խ", "Խ", XK_Armenian_khe, 1 },
	{ "շ", "Շ", XK_Armenian_sha, 1 },
	{ "ռ", "Ռ", XK_Armenian_ra, 1 },
	{ 0, 0, XK_Shift_R, 2 },
	{ 0 }, /* New row */
	{ "Ctrl", 0, XK_Control_L, 2 },
	{ "Alt",  0,XK_Alt_L, 2 },
	{ "",  0,XK_space, 5 },
	{ "Alt",  0,XK_Alt_R, 2 },
	{ "Ctrl",  0,XK_Control_R, 2 },
};

Buttonmod buttonmods[] = {
	{ XK_Shift_L, Button2 },
	{ XK_Alt_L, Button3 },
};

#define OVERLAYS 1
static Key overlay[OVERLAYS] = {
	{ 0, 0, XK_Cancel },
};

#define LAYERS 1
static char* layer_names[LAYERS] = {
	"am",
};

static Key* available_layers[LAYERS] = {
	keys_am,
};
