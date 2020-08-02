static Key keys[] = {


  { "1!", XK_1, 1 },
  { "2@", XK_2, 1 },
  { "3#", XK_3, 1 },
  { "4$", XK_4, 1 },
  { "5%", XK_5, 1 },
  { "6^", XK_6, 1 },
  { "7&", XK_7, 1 },
  { "8*", XK_8, 1 },
  { "9(", XK_9, 1 },
  { "0)", XK_0, 1 },

  { 0 }, /* New row */

  { "'\"", XK_apostrophe, 1 },
  { "`~", XK_grave, 1 },
  { "-_", XK_minus, 1 },
  { "=+", XK_plus, 1 },
  { "[{", XK_bracketleft, 1 },
  { "]}", XK_bracketright, 1 },
  { ",<", XK_comma, 1 },
  { ".>", XK_period, 1 },
  { "/?", XK_slash, 1 },
  { "\\", XK_backslash, 1 },

  { 0 }, /* New row */

  { "|", XK_Shift_L|XK_bar, 1 },
  { "L", XK_Left, 1 },
  { "R", XK_Right, 1 },
  { "Ctrl-C", XK_Shift_L|XK_bar, 1 },
  { "Ctrl-L", XK_Shift_L|XK_bar, 1 },
  { "Ctrl-V", XK_Shift_L|XK_bar, 1 },
  { "Ctrl-D", XK_Shift_L|XK_bar, 1 },
  { "TAB", XK_Tab, 1 },
  { "<-", XK_BackSpace, 2 },

  { 0 }, /* New row */
  { "Layer 1", XK_Cancel, 1},
  { "Shift", XK_Shift_L, 1 },
  /*{ "L", XK_Left, 1 },*/
  { "D", XK_Down, 1 },
  { "U", XK_Up, 1 },
  /*{ "R", XK_Right, 1 },*/
  { "", XK_space, 2 },
  { "Esc", XK_Escape, 1 },
  { "Ctrl", XK_Control_L, 1 },
  /*{ "Alt", XK_Alt_L, 1 },*/
  { "Enter", XK_Return, 2 },
};

Buttonmod buttonmods[] = {
	{ XK_Shift_L, Button2 },
	{ XK_Alt_L, Button3 },
};

