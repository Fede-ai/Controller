#pragma once

//uint8_t
enum Cmd {
	UNDEFINED = 0,
	REGISTER_ADMIN = 1,
	REGISTER_ATTACKER = 2,
	REGISTER_VICTIM = 3,

	CLIENTS_UPDATE = 4,
	CHANGE_NAME = 5,	//admin
	BAN_HID = 6,		//admin
	UNBAN_HID = 7,		//admin
	KILL = 8,			//admin
	SAVE_DATASET = 9,	//admin

	START_SSH = 100,
	END_SSH = 101,
	SSH_DATA = 102
};