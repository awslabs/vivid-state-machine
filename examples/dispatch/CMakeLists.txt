add_executable(dispatch
    application.c
    caller.c
    callee.c
)

target_link_libraries(dispatch
    main
    vivid-sm
)
