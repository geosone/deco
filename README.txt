The GonWare Commander is the file manager of GonWare (CMDR.EXE), grown
from the Demos Commander (deco) by Serge Vakulenko: panels that follow
the screen size, a VC-like colour scheme, a newt/S-Lang screen backend
and GDOS support. It still builds for Unix systems as `deco`.

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
