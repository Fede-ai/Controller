#include "tui.hpp"
#include "scroller.hpp"

ftxui::Tui::Tui()
	:
	screen(ScreenInteractive::Fullscreen())
{
    //setup title
    title = Renderer([] {
        return hbox({
            text("") | bold | color(Color::Blue),
            }) | border;
        });

    //setup server shell panel
    server_shell_output = " Server Shell, type \"help\" for help with commands.\n \n";
    server_input = Input(&server_shell_input, "Type here...");
    server_scroller = Scroller(Renderer([&] {
        return paragraph(server_shell_output);
        }));
    server_shell = Renderer(Container::Vertical({ server_input, server_scroller }), [&] {
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
    ssh_shell = Renderer(Container::Vertical({ ssh_input, ssh_scroller }), [&] {
        return vbox({
            ssh_scroller->Render(),
            separator(),
            hbox({text(" > "), ssh_input->Render()}) | xflex,
            });
        });

    //setup info panels
    clients_overview_output = "Server connection required";
    clients_scroller = Scroller(Renderer([&] {
        return paragraph(clients_overview_output);
        }));
    clients_overview = Renderer(clients_scroller, [&] {
        return vbox({
            separator(),
            hbox({
                filler() | size(WIDTH, EQUAL, 1),
                clients_scroller->Render()
                })
            });
        });
   
    server_database_output = "Server connection and admin privileges required";
    database_scroller = Scroller(Renderer([&] {
        return paragraph(server_database_output);
        }));
    server_database = Renderer(database_scroller, [&] {
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
            clients_overview,
            server_database,
            Renderer([] { return filler(); })
        }, &info_tab_selected);

    //setup tab for shell panels
    shell_tab_values = { "Server", "SSH" };
    shell_tab_menu = Menu(&shell_tab_values, &shell_tab_selected);
    shell_tab_container = Container::Tab({
            server_shell,
            ssh_shell,
        }, &shell_tab_selected);

    //layout for event handling
    layout = Container::Vertical({
        Container::Horizontal({
            info_tab_menu,
            info_tab_container,
        }),
        Container::Horizontal({
            shell_tab_menu,
            shell_tab_container,
        }),
        });

    //main layout renderer
    ssh_title = text("SSH (0)") | color(Color::Red);
    info_title = text("Not connected") | color(Color::Red);
    main_renderer = Renderer(layout, [&] {
        return vbox({
            title->Render(),
            vbox({
                hbox({
                    info_title | bold | xflex,
                    separator(),
                    info_tab_menu->Render(),
                }) | xflex | align_right,
                info_tab_container->Render(),
            }) | border,
            hbox({
                vbox({
                    ssh_title | bold | center,
                    separator(),
                    //non interactive checkbox
                    hbox({
                        vbox({
                            Checkbox("", &is_sending_mouse)->Render(),
                            Checkbox("", &is_sending_keyboard)->Render()
                        }),
                        vbox({
                            text("Mouse"),
                            text("Keyboard")
                        }),
                    }),
                    separator(),
                    shell_tab_menu->Render()
                }),
                separator(),
                shell_tab_container->Render() | flex,
            }) | flex | border,
            });
        });
}

void ftxui::Tui::run()
{
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
                    //server_shell_output += " > " + server_shell_input + "\n";
					server_commands.push(server_shell_input);
                    server_shell_input.clear();

                    return true;
                }
                else if (shell_tab_selected == 1) {
                    //ssh_shell_output += " > " + ssh_shell_input + "\n";
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
                    
                    std::string hId = server_database_output;
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
    title = Renderer([s] {
        return hbox({
            text(s) | bold | color(Color::Blue),
            }) | border;
        });
    triggerRedraw();
}

void ftxui::Tui::printServerShell(std::string s)
{
    server_shell_output += s;

    server_scroller->Render();

    dynamic_cast <ScrollerBase*>(server_scroller.get())->ScrollToBottom();
    triggerRedraw();
}

void ftxui::Tui::printSshShell(std::string s)
{
    ssh_shell_output += s;

    ssh_scroller->Render();

    dynamic_cast <ScrollerBase*>(ssh_scroller.get())->ScrollToBottom();
    triggerRedraw();
}

void ftxui::Tui::triggerRedraw()
{
	screen.PostEvent(Event::Custom);
}
