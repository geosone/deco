The GonWare Commander is the file manager of GonWare (CMDR.EXE), grown
from the Demos Commander (deco) by Serge Vakulenko: panels that follow
the screen size, a VC-like colour scheme, a newt/S-Lang screen backend
and GDOS support. It still builds for Unix systems as `deco`.

It speaks the language of the messages (LANGUAGE, LC_ALL, LC_MESSAGES,
LANG) through gettext, text domain `cmdr`: German, French, Spanish and
Italian so far (po/*.po; `po/update.sh` extracts new texts and merges
them). The screen cells hold Latin-1 for now, so languages beyond it
(Polish, Czech, Russian ...) come with Unicode cells.

Demos Commander (deco) is an text-mode file manager for Unix systems.

The main interface consists of two panels, which display the file system.
It is used in a similar way to many other programs run in the Unix shell.
Arrow keys control file selection, the insert key is used to select files
(but not directories), and the function keys perform operations such as
renaming, editing and copying files.

Sources are distributed freely under the terms of MIT license.

=== Build and Install ===

    git clone https://github.com/sergev/deco
    cd deco
    cmake -B build
    make -C build
    make -C build install
