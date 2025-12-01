project(shell)

find_package(libfuse REQUIRED)

add_executable(${PROJECT_NAME} vfs.c vfs.h kubsh.c)

target_link_libraries(${PROJECT_NAME} PUBLIC libfuse readline)
