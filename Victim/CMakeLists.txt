set(SOURCE
	main.cpp
	victim.hpp
	victim.cpp
	../commands.hpp
)

#add_executable(Victim WIN32 ${SOURCE})
add_executable(Victim ${SOURCE})

set_target_properties(Victim PROPERTIES
  WIN32_EXECUTABLE $<CONFIG:Release>
)

target_link_libraries(Victim PRIVATE SFML::System SFML::Network SFML::Window SFML::Main)

if(WIN32)
  add_custom_command(TARGET Victim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:Victim>
    $<TARGET_FILE_DIR:Victim>
    COMMAND_EXPAND_LISTS
  )
endif()
