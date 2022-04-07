add_library(main
    main.c
)

find_package(Threads REQUIRED)

target_link_libraries(main
    ${LIBEV_LIBRARIES}
    Threads::Threads
)
