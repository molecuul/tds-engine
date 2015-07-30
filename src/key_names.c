#include "key_names.h"
#include "log.h"

#include <string.h>

static struct tds_key_name _tds_key_name_list[] = {
	{"0", GLFW_KEY_0},
	{"1", GLFW_KEY_1},
	{"2", GLFW_KEY_2},
	{"3", GLFW_KEY_3},
	{"4", GLFW_KEY_4},
	{"5", GLFW_KEY_5},
	{"6", GLFW_KEY_6},
	{"7", GLFW_KEY_7},
	{"8", GLFW_KEY_8},
	{"9", GLFW_KEY_9},
	{"space", GLFW_KEY_SPACE},
	{"comma", GLFW_KEY_COMMA},
	{"period", GLFW_KEY_PERIOD},
	{"slash", GLFW_KEY_SLASH},
	{"semicolon", GLFW_KEY_SEMICOLON},
	{"equal", GLFW_KEY_EQUAL},
	{"A", GLFW_KEY_A},
	{"B", GLFW_KEY_B},
	{"C", GLFW_KEY_C},
	{"D", GLFW_KEY_D},
	{"E", GLFW_KEY_E},
	{"F", GLFW_KEY_F},
	{"G", GLFW_KEY_G},
	{"H", GLFW_KEY_H},
	{"I", GLFW_KEY_I},
	{"J", GLFW_KEY_J},
	{"K", GLFW_KEY_K},
	{"L", GLFW_KEY_L},
	{"M", GLFW_KEY_M},
	{"N", GLFW_KEY_N},
	{"O", GLFW_KEY_O},
	{"P", GLFW_KEY_P},
	{"Q", GLFW_KEY_Q},
	{"R", GLFW_KEY_R},
	{"S", GLFW_KEY_S},
	{"T", GLFW_KEY_T},
	{"U", GLFW_KEY_U},
	{"V", GLFW_KEY_V},
	{"W", GLFW_KEY_W},
	{"X", GLFW_KEY_X},
	{"Y", GLFW_KEY_Y},
	{"Z", GLFW_KEY_Z},
	{"leftbracket", GLFW_KEY_LEFT_BRACKET},
	{"rightbracket", GLFW_KEY_RIGHT_BRACKET},
	{"backslash", GLFW_KEY_BACKSLASH},
	{"grave", GLFW_KEY_GRAVE_ACCENT},
	{"escape", GLFW_KEY_ESCAPE},
	{"enter", GLFW_KEY_ENTER},
	{"tab", GLFW_KEY_TAB},
	{"backspace", GLFW_KEY_BACKSPACE},
	{"insert", GLFW_KEY_INSERT},
	{"delete", GLFW_KEY_DELETE},
	{"leftarrow", GLFW_KEY_LEFT},
	{"rightarrow", GLFW_KEY_RIGHT},
	{"uparrow", GLFW_KEY_UP},
	{"downarrow", GLFW_KEY_DOWN},
	{"pageup", GLFW_KEY_PAGE_UP},
	{"pagedown", GLFW_KEY_PAGE_DOWN},
	{"home", GLFW_KEY_HOME},
	{"end", GLFW_KEY_END},
	{"capslock", GLFW_KEY_CAPS_LOCK},
	{"scrolllock", GLFW_KEY_SCROLL_LOCK},
	{"numlock", GLFW_KEY_NUM_LOCK},
	{"printscreen", GLFW_KEY_PRINT_SCREEN},
	{"pause", GLFW_KEY_PAUSE},
	{"F1", GLFW_KEY_F1},
	{"F2", GLFW_KEY_F2},
	{"F3", GLFW_KEY_F3},
	{"F4", GLFW_KEY_F4},
	{"F5", GLFW_KEY_F5},
	{"F6", GLFW_KEY_F6},
	{"F7", GLFW_KEY_F7},
	{"F8", GLFW_KEY_F8},
	{"F9", GLFW_KEY_F9},
	{"F10", GLFW_KEY_F10},
	{"F11", GLFW_KEY_F11},
	{"F12", GLFW_KEY_F12},
	{"KP0", GLFW_KEY_KP_0},
	{"KP1", GLFW_KEY_KP_1},
	{"KP2", GLFW_KEY_KP_2},
	{"KP3", GLFW_KEY_KP_3},
	{"KP4", GLFW_KEY_KP_4},
	{"KP5", GLFW_KEY_KP_5},
	{"KP6", GLFW_KEY_KP_6},
	{"KP7", GLFW_KEY_KP_7},
	{"KP8", GLFW_KEY_KP_8},
	{"KP9", GLFW_KEY_KP_9},
	{"KPdecimal", GLFW_KEY_KP_DECIMAL},
	{"KPdivide", GLFW_KEY_KP_DIVIDE},
	{"KPmultiply", GLFW_KEY_KP_MULTIPLY},
	{"KPsubtract", GLFW_KEY_KP_SUBTRACT},
	{"KPadd", GLFW_KEY_KP_ADD},
	{"KPenter", GLFW_KEY_KP_ENTER},
	{"leftshift", GLFW_KEY_LEFT_SHIFT},
	{"rightshift", GLFW_KEY_RIGHT_SHIFT},
	{"leftctrl", GLFW_KEY_LEFT_CONTROL},
	{"rightctrl", GLFW_KEY_RIGHT_CONTROL},
	{"leftalt", GLFW_KEY_LEFT_ALT},
	{"rightalt", GLFW_KEY_RIGHT_ALT},
	{"leftsuper", GLFW_KEY_LEFT_SUPER},
	{"rightsuper", GLFW_KEY_RIGHT_SUPER},
	{"menu", GLFW_KEY_MENU},
};

struct tds_key_name* tds_key_name_get_list(void) {
	return _tds_key_name_list;
}

int tds_key_name_get_list_size(void) {
	return sizeof(_tds_key_name_list) / sizeof(struct tds_key_name);
}

int tds_key_name_get_key(char* name) {
	for (int i = 0; i < tds_key_name_get_list_size(); ++i) {
		if (!strcmp(name, _tds_key_name_list[i].name)) {
			return _tds_key_name_list[i].id;
		}
	}

	tds_logf(TDS_LOG_WARNING, "Key name [%s] did not match any keys in the list. See src/key_names.c for a complete key list.");
	return GLFW_KEY_UNKNOWN;
}