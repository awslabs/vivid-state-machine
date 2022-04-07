add_executable(toaster
    application.c
    toaster.c
)

target_link_libraries(toaster
    main
    vivid-sm
)
