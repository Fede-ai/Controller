set(SOURCE
	main.cpp
	server.hpp
	server.cpp
	../commands.hpp
)

add_executable(Server ${SOURCE})

target_link_libraries(Server PRIVATE SFML::Network SQLite::SQLite3)

if(WIN32)
  add_custom_command(TARGET Server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:Server>
    $<TARGET_FILE_DIR:Server>
    COMMAND_EXPAND_LISTS
  )
endif()
