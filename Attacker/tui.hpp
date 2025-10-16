#include <ftxui/component/screen_interactive.hpp>
#include "ftxui/component/component.hpp"

namespace ftxui {
	
	class Tui {
	public:
		Tui();
		void run();
		void stop();

		void setTitle(std::string s);
		void setSshTitle(Element e);
		void setInfoTitle(Element e);
		void setIsSendingMouse(bool b);
		void setIsSendingKeyboard(bool b);
		
		void setClientsOutput(std::string s);
		void setDatabaseOutput(std::string s);
		void printServerShell(std::string s);
		void clearServerShell();
		void printSshShell(std::string s);
		void clearSshShell();

		//will have to require mutex
		std::queue<std::string> server_commands;
		std::queue<std::string> ssh_commands;

	private:
		void triggerRedraw();

		std::mutex mutex;
		//variables that require mutex
		std::string title_string;
		Element new_info_title, new_ssh_title;
		std::string new_clients_output, new_database_output;
		std::queue<std::string> new_server_shell_output, new_ssh_shell_output;
		bool new_is_sending_mouse = false, new_is_sending_keyboard = false;
		
		Element info_title, ssh_title;
		std::string clients_output, database_output;
		std::string server_shell_output, ssh_shell_output;

		std::vector<std::string> info_tab_values, shell_tab_values;
		Component info_tab_menu, info_tab_container;
		Component shell_tab_menu, shell_tab_container;
		
		Component clients, database;
		Component clients_scroller, database_scroller;
		
		std::string server_shell_input = "", ssh_shell_input = "";
		Component server_input, server_scroller, server_shell;
		Component ssh_input, ssh_scroller, ssh_shell;
		int info_tab_selected = 0, shell_tab_selected = 0;

		bool is_sending_mouse = false, is_sending_keyboard = false;
		Component mouse_checkbox, keyboard_checkbox;
		
		ScreenInteractive screen;
		Component layout, main_renderer, title;
	};
}
