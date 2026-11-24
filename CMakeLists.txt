cmake_minimum_required(VERSION 3.15)

project(deco VERSION 3.10 DESCRIPTION "Demos Commander")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
add_compile_options(-Wall -Werror -Wshadow)

# DECO_NEWT: the screen and the keys on newt (on S-Lang) instead of
# termcap, for systems with newt but no terminfo (GonWare's CMDR.EXE)
option(DECO_NEWT "Screen and keys on newt" OFF)

add_executable(${PROJECT_NAME}
    choice.c
    cmd.c
    com.c
    dir.c
    draw.c
    edit.c
    env.c
    ex.c
    help.c
    hexview.c
    main.c
    menu.c
    mh.c
    pw.c
    rec.c
    run.c
    scr.c
    ut.c
    view.c
)
if(DECO_NEWT)
    target_sources(${PROJECT_NAME} PRIVATE newtio.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DECO_NEWT)
    target_link_libraries(${PROJECT_NAME} newt slang)
else()
    target_sources(${PROJECT_NAME} PRIVATE cap.c key.c tty.c)
    # termcap's tgetent: in ncurses, or in a separate tinfo (Gentoo, some
    # other distributions split it off)
    find_library(TINFO_LIBRARY tinfo)
    target_link_libraries(${PROJECT_NAME} ncurses $<$<BOOL:${TINFO_LIBRARY}>:${TINFO_LIBRARY}>)
endif()

install(TARGETS ${PROJECT_NAME}
    DESTINATION bin
)
install(FILES deco.1 deco-ru.1
    DESTINATION share/man/man1
)
install(FILES menu profile
    DESTINATION lib/deco
)
install(FILES help/e.general help/e.edit help/e.view help/e.hexview
    DESTINATION lib/deco/help
)
