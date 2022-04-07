add_executable(countdown
    application.c
    countdown.c
)

target_link_libraries(countdown
    main
    vivid-sm
)
