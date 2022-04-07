
add_subdirectory(toaster)
add_subdirectory(countdown)
if(NOT VIVID_BINDING_FREERTOS)
    add_subdirectory(countdown_cpp)
endif()
add_subdirectory(dispatch)
add_subdirectory(platform)
