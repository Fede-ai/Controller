#include <ftxui/component/screen_interactive.hpp>
#include "ftxui/component/component.hpp"

namespace ftxui {
	
	class Tui {
	public:
		Tui();
		void run();
		void stop();

		void setTitle(std::string s);

		void printServerShell(std::string s);
		void printSshShell(std::string s);
		void triggerRedraw();

		std::queue<std::string> server_commands;
		std::queue<std::string> ssh_commands;
		Element info_title, ssh_title;
		std::string server_shell_output, ssh_shell_output;
		std::string clients_overview_output, server_database_output;
		bool is_sending_mouse = false, is_sending_keyboard = false;

	private:
		ScreenInteractive screen;
		Component layout, main_renderer, title;

		std::vector<std::string> info_tab_values, shell_tab_values;
		Component info_tab_menu, info_tab_container;
		Component shell_tab_menu, shell_tab_container;
		
		Component clients_overview, server_database;
		Component clients_scroller, database_scroller;
		
		std::string server_shell_input = "", ssh_shell_input = "";
		Component server_input, server_scroller, server_shell;
		Component ssh_input, ssh_scroller, ssh_shell;
		int info_tab_selected = 0, shell_tab_selected = 0;
	};
}
