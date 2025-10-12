set(SOURCE
	main.cpp
	attacker.hpp 
	attacker.cpp
	scroller.hpp
	tui.hpp 
	tui.cpp
	../commands.hpp
)

add_executable(Attacker ${SOURCE})

target_include_directories(Attacker PRIVATE ${PATH_SFML}/include)
target_include_directories(Attacker PRIVATE ${PATH_FTXUI}/include)

target_link_directories(Attacker PRIVATE ${PATH_SFML}/lib)
target_link_directories(Attacker PRIVATE ${PATH_FTXUI}/lib)

target_link_libraries(Attacker PRIVATE
	$<$<CONFIG:Release>:sfml-system.lib sfml-window.lib sfml-graphics.lib sfml-network.lib>
	$<$<CONFIG:Debug>:sfml-system-d.lib sfml-window-d.lib sfml-graphics-d.lib sfml-network-d.lib>
	ftxui-component.lib
	ftxui-dom.lib
	ftxui-screen.lib
)

# copy necessary dll files
add_custom_command(TARGET Attacker POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different        	
	$<$<CONFIG:Release>:${PATH_SFML}/bin/sfml-system-3.dll>
	$<$<CONFIG:Debug>:${PATH_SFML}/bin/sfml-system-d-3.dll>
	$<$<CONFIG:Release>:${PATH_SFML}/bin/sfml-window-3.dll>
	$<$<CONFIG:Debug>:${PATH_SFML}/bin/sfml-window-d-3.dll>
	$<$<CONFIG:Release>:${PATH_SFML}/bin/sfml-graphics-3.dll>
	$<$<CONFIG:Debug>:${PATH_SFML}/bin/sfml-graphics-d-3.dll>
	$<$<CONFIG:Release>:${PATH_SFML}/bin/sfml-network-3.dll>
	$<$<CONFIG:Debug>:${PATH_SFML}/bin/sfml-network-d-3.dll>
	$<TARGET_FILE_DIR:Attacker>
)