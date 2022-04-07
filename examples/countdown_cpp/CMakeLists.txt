add_executable(countdown_cpp
    application.cpp
    countdown_cpp.cxx
)

target_link_libraries(countdown_cpp
    main
    vivid-sm
)
