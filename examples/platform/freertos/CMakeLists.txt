add_library(main
    main.c
    main_vsm.c
    startup_gcc.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Source/tasks.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Source/list.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Source/queue.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Source/timers.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Source/event_groups.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Source/stream_buffer.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Source/portable/MemMang/heap_4.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/IntQueueTimer.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/build/gcc/printf-stdarg.c
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Demo/Common/Minimal/IntQueue.c
)

target_include_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Demo/Common/include
)

target_link_libraries(main
    -Wl,-T${CMAKE_SOURCE_DIR}/lib/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/build/gcc/mps2_m3.ld
    -Wl,-Map=linker.map
)
