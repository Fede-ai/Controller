set(SOURCE
	main.cpp
	attacker.hpp 
	attacker.cpp
	tui.hpp 
	tui.cpp
	../commands.hpp
)

add_executable(Attacker ${SOURCE})

target_link_libraries(Attacker PRIVATE 
	SFML::System SFML::Window SFML::Graphics SFML::Network
	ftxui::screen ftxui::dom ftxui::component
)

if(WIN32)
  add_custom_command(TARGET Attacker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:Attacker>
    $<TARGET_FILE_DIR:Attacker>
    COMMAND_EXPAND_LISTS
  )
endif()
