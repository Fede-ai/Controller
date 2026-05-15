set(SOURCE
	main.cpp
	server.hpp
	server.cpp
	../commands.hpp
)

add_executable(Server ${SOURCE})

#target_include_directories(Server PRIVATE ${PATH_SFML}/include)
#target_link_directories(Server PRIVATE ${PATH_SFML}/lib)
#target_link_libraries(Server PRIVATE
#	$<$<CONFIG:Release>:sfml-system.lib sfml-network.lib>
#	$<$<CONFIG:Debug>:sfml-system-d.lib sfml-network-d.lib>
#)

target_link_libraries(Server PRIVATE SFML::Network SQLite3::SQLite3)

if(WIN32)
  add_custom_command(TARGET Server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:Server>
    $<TARGET_FILE_DIR:Server>
    COMMAND_EXPAND_LISTS
  )
endif()

#add_custom_command(TARGET Server POST_BUILD
#	COMMAND ${CMAKE_COMMAND} -E copy_if_different        	
#	$<$<CONFIG:Release>:${PATH_SFML}/bin/sfml-system-3.dll>
#	$<$<CONFIG:Debug>:${PATH_SFML}/bin/sfml-system-d-3.dll>
#	$<$<CONFIG:Release>:${PATH_SFML}/bin/sfml-network-3.dll>
#	$<$<CONFIG:Debug>:${PATH_SFML}/bin/sfml-network-d-3.dll>
#	$<TARGET_FILE_DIR:Server>
#)