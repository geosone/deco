cmake_minimum_required(VERSION 3.15)

project(deco VERSION 4.0 DESCRIPTION "GonWare Commander, based on the Demos Commander")
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
    mouse.c
    mh.c
    pw.c
    rec.c
    run.c
    scr.c
    ut.c
    view.c
    intl.c
)
# the message catalogues (po/*.po, text domain cmdr) and where they go
include(GNUInstallDirs)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    CMDR_LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
find_program(MSGFMT msgfmt)
if(MSGFMT)
    file(GLOB _po "${CMAKE_CURRENT_SOURCE_DIR}/po/*.po")
    foreach(_f ${_po})
        get_filename_component(_lang ${_f} NAME_WE)
        set(_mo "${CMAKE_CURRENT_BINARY_DIR}/locale/${_lang}/LC_MESSAGES/cmdr.mo")
        add_custom_command(OUTPUT ${_mo}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/locale/${_lang}/LC_MESSAGES"
            COMMAND ${MSGFMT} -o ${_mo} ${_f}
            DEPENDS ${_f})
        list(APPEND _mos ${_mo})
        install(FILES ${_mo} DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/${_lang}/LC_MESSAGES)
    endforeach()
    add_custom_target(cmdr-mo ALL DEPENDS ${_mos})
endif()
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
# the help of the languages without a catalogue yet (Russian, Ukrainian);
# the others come through gettext
install(FILES help/r.general help/r.edit help/r.view help/r.hexview
              help/u.general help/u.edit help/u.view help/u.hexview
    DESTINATION lib/deco/help
)
