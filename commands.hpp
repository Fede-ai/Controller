#pragma once

//uint8_t
enum Cmd {
	UNDEFINED = 0,
	REGISTER_ADMIN = 1,
	REGISTER_ATTACKER = 2,
	REGISTER_VICTIM = 3,
	PING = 4,

	CLIENTS_UPDATE = 50,
	CHANGE_NAME = 51,	//admin
	BAN_HID = 52,		//admin
	UNBAN_HID = 53,		//admin
	KILL = 54,			//admin
	SAVE_DATASET = 55,	//admin

	START_SSH = 100,
	END_SSH = 101,
	//every cmd from SSH_DATA to 150 should just be forwarded
	SSH_DATA = 120,

	SSH_MOUSE_POS = 121,
	SSH_MOUSE_PRESS = 122,
	SSH_MOUSE_RELEASE = 123,
	SSH_MOUSE_SCROLL = 124,

	SSH_KEYBOARD_PRESS = 130,
	SSH_KEYBOARD_RELEASE = 131,
};