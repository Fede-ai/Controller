#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <mutex>

namespace ftxui {

	class ScrollerBase : public ComponentBase {
	public:
		ScrollerBase(Component child) { Add(child); }

		void ScrollToBottom() { selected_ = size_ - 1; }
		int GetSelected() const { return selected_; }

	private:
		Element OnRender() final {
			auto focused = Focused() ? focus : ftxui::select;
			auto style = Focused() ? inverted : nothing;

			Element background = ComponentBase::Render();
			background->ComputeRequirement();
			size_ = background->requirement().min_y;
			return dbox({
					   std::move(background),
					   vbox({
						   text(L"") | size(HEIGHT, EQUAL, selected_),
						   text(L"") | style | focused,
					   }),
				}) |
				vscroll_indicator | yframe | yflex | reflect(box_);
		}

		bool OnEvent(Event event) final {
			if (event.is_mouse() && box_.Contain(event.mouse().x, event.mouse().y))
				TakeFocus();

			int selected_old = selected_;
			if (event == Event::ArrowUp || event == Event::Character('k') ||
				(event.is_mouse() && event.mouse().button == Mouse::WheelUp)) {
				selected_--;
			}
			if ((event == Event::ArrowDown || event == Event::Character('j') ||
				(event.is_mouse() && event.mouse().button == Mouse::WheelDown))) {
				selected_++;
			}
			if (event == Event::PageDown)
				selected_ += box_.y_max - box_.y_min;
			if (event == Event::PageUp)
				selected_ -= box_.y_max - box_.y_min;
			if (event == Event::Home)
				selected_ = 0;
			if (event == Event::End)
				selected_ = size_;

			selected_ = std::max(0, std::min(size_ - 1, selected_));
			return selected_old != selected_;
		}

		bool Focusable() const final { return true; }

		int selected_ = 0;
		int size_ = 0;
		Box box_;
	};

	Component Scroller(Component child);

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
		// -1 = error, 0-99 = progress, 100 = done, 101 = idle
		void setSendingFileProgress(short p);
		// -1 = error, 0-99 = progress, 100 = done, 101 = idle
		void setGettingFileProgress(short p);

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
		void triggerRedraw() {
			screen.PostEvent(Event::Custom);
		}

		std::mutex mutex;
		//variables that require mutex
		std::string title_string;
		Element new_info_title, new_ssh_title;
		std::string new_clients_output, new_database_output;
		std::queue<std::string> new_server_shell_output, new_ssh_shell_output;
		bool new_is_sending_mouse = false, new_is_sending_keyboard = false;
		Element new_sending_file_status, new_getting_file_status;

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
		Element sending_file_status, getting_file_status;

		ScreenInteractive screen;
		Component layout, main_renderer, title;
	};
}
