include_directories(
    lib/FreeRTOS/FreeRTOS/Source/include
    lib/FreeRTOS/FreeRTOS/Source/portable/GCC/ARM_CM3
    lib/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
    lib/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/CMSIS
)

add_compile_options(
    -ffunction-sections
    -fdata-sections
    -nostartfiles
    -ffreestanding
    -mthumb
    -mcpu=cortex-m3
    "-DVIVID_TIME_TYPE=unsigned"
    "-DVIVID_CONVERT_TIME(x)=(unsigned)((x)*1000.0)"
)

link_libraries(
    -Wl,--gc-sections
    -nostartfiles
    -ffreestanding
    -mthumb
    -mcpu=cortex-m3
)
