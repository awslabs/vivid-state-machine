add_library(${PROJECT_NAME}
    $<$<BOOL:${VIVID_BINDING_LIBEV}>:   binding/vivid_binding_libev.c>
    $<$<BOOL:${VIVID_BINDING_WIN32}>:   binding/vivid_binding_win32.c>
    $<$<BOOL:${VIVID_BINDING_LINUX}>:   binding/vivid_binding_linux.c>
    $<$<BOOL:${VIVID_BINDING_FREERTOS}>:binding/vivid_binding_freertos.c>
    vivid_log.c
    vivid_map.c
    vivid_periodic_timer.c
    vivid_queue.c
    vivid_uml.c
    vivid_sm.c
)

if(VIVID_BINDING_LIBEV)
    find_library(LIBEV_LIBRARIES NAMES ev)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ../lib/jsmn
)

target_link_libraries(${PROJECT_NAME}
    $<$<BOOL:${VIVID_BINDING_LIBEV}>:${LIBEV_LIBRARIES}>
    $<$<BOOL:${VIVID_BINDING_LINUX}>:m>
)

install(TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY
    ../inc/vivid
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/vivid-sm.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
