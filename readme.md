 Teeworlds zCatch [![CircleCI](https://circleci.com/gh/jxsl13/zcatch/tree/zCatch-0.7.x.svg?style=svg)](https://circleci.com/gh/jxsl13/zcatch/tree/zCatch-0.7.x) [![Build status](https://ci.appveyor.com/api/projects/status/103160wvp3gs1tj8/branch/zCatch-0.7.x?svg=true)](https://ci.appveyor.com/project/jxsl13/zcatch/branch/zCatch-0.7.x) [![Build Status](https://travis-ci.com/jxsl13/zcatch.svg?branch=zCatch-0.7.x)](https://travis-ci.com/jxsl13/zcatch)
=========

Description
===========

zCatch is a game server modification for the retro-multiplayergame Teeworlds. It is written for Teeworlds 0.7. If you hit someone, the player is caught and will be spectating you, until you die or win the round. The last player standing will win the game and might earn some score points in the rankings.

Introduced new server commands
------------------------------
| Command                        | Description                                                                                     |
|--------------------------------|-------------------------------------------------------------------------------------------------|
| sv_weapon_mode <0..6>          |  0: Hammer 1: Gun 2: Shotgun 3: Grenade Launcher(default) 4: Laser Rifle 5: Ninja 6: Everything |
| sv_db_type ""                  | ""(no ranking), "redis" or "sqlite"(default)                                                    |
| sv_db_sqlite_file "ranking.db" | Relative path to the sqlite3 database file.                                                     |
| sv_warmup_autostart <0/1>      | Whether warmup should automatically start if there are not enough players to end a round.       |
  
More commands can be found in the example configuration file in the **[wiki](https://github.com/jxsl13/zcatch/wiki/Server-configuration-example)**.

Teeworlds
=========

Teeworlds is a free online multiplayer game, available for all major
operating systems. Battle with up to 16 players in a variety of game
modes, including Team Deathmatch and Capture The Flag. You can even
design your own maps!

License
=======

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software. See license.txt for full license
text including copyright information.

Please visit https://www.teeworlds.com/ for up-to-date information about
the game, including new versions, custom maps and much more.

Originally written by Magnus Auvinen. 


This modification has been created by jxsl13.  
Please provide the apropriate credits if you want to modify this source code.


Building on Linux or macOS
==========================

Requirements:
-------------

    gcc >= 8.10 (supporting C++17) / macOS usually ships with the latest clang version.  
It might also be necessary to add these two lines to your `.bashrc/.zshrc` etc.

    export PATH=/usr/local/gcc-8.10/bin:$PATH
    export LD_LIBRARY_PATH=/usr/local/gcc-8.10/lib64:$LD_LIBRARY_PATH

Installing dependencies
-----------------------

    # Debian/Ubuntu
    sudo apt install build-essential cmake git libfreetype6-dev libsdl2-dev libpnglite-dev libwavpack-dev python3

    # Fedora
    sudo dnf install @development-tools cmake gcc-c++ git freetype-devel mesa-libGLU-devel pnglite-devel python3 SDL2-devel wavpack-devel

    # Arch Linux (doesn't have pnglite in its repositories)
    sudo pacman -S --needed base-devel cmake freetype2 git glu python sdl2 wavpack

    # macOS
    brew install cmake freetype sdl2


Downloading repository
----------------------

    git clone https://github.com/jxsl13/zcatch.git
    cd zcatch

    # Checkout the 0.7.x branch with:
    git checkout zCatch-0.7.x

    # In order to download all external libraries, execute:
    git submodule update --init --recursive
    

Building
--------

    mkdir -p build
    cd build
    cmake ..
    make -j4 zcatch_srv

    # depending on the number of CPU cores your computer has
    # you can change the -j4 to -j<cpu cores> 

    If your latest GCC version has been installed in a non-default path, 
    you can set take that into account by using instead of "cmake .."
    something like "cmake .. -DCMAKE_CXX_COMPILER=g++-8.10 -DCMAKE_CC_COMPILER=gcc-8.10"

On subsequent builds, you only have to repeat the `make` step.

You can then run the client with `./teeworlds` and the server with
`./zcatch_srv`.


Build options
-------------

The following options can be passed to the `cmake ..` command line (between the
`cmake` and `..`) in the "Building" step above.

`-GNinja`: Use the Ninja build system instead of Make. This automatically
parallizes the build and is generally **faster**. (Needs `sudo apt install
ninja-build` on Debian, `sudo dnf install ninja-build` on Fedora, and `sudo
pacman -S --needed ninja` on Arch Linux.)

`-DDEV=ON`: Enable debug mode and disable some release mechanics. This leads to
**faster** builds.

`-DCLIENT=OFF`: Disable generation of the client target. Can be useful on
headless servers which don't have graphics libraries like SDL2 installed.


Building on Windows with Visual Studio
======================================

Download and install some version of [Microsoft Visual
Studio](https://www.visualstudio.com/) (as of writing, MSVS Community 2017)
with the following components:

* Desktop development with C++ (on the main page)
* Python development (on the main page)
* Git for Windows (in Individual Components → Code tools)

Run Visual Studio. Open the Team Explorer (View → Team Explorer, Ctrl+^,
Ctrl+M). Click Clone (in the Team Explorer, Connect → Local Git Repositories).
Enter `https://github.com/teeworlds/teeworlds` into the first input box. Wait
for the download to complete (terminals might pop up).

Wait until the CMake configuration is done (watch the Output windows at the
bottom).

Select `teeworlds.exe` in the Select Startup Item… combobox next to the green
arrow. Wait for the compilation to finish.

For subsequent builds you only have to click the button with the green arrow
again.


Building on Windows with MinGW
==============================

Download and install MinGW with at least the following components:

- mingw-developer-toolkit-bin
- mingw32-base-bin
- mingw32-gcc-g++-bin
- msys-base-bin

Also install [Git](https://git-scm.com/downloads) (for downloading the source
code), [Python](https://www.python.org/downloads/) and
[CMake](https://cmake.org/download/).

Open CMake ("CMake (cmake-gui)" in the start menu). Click "Browse Source"
(first line) and select the directory with the Teeworlds source code. Next,
click "Browse Build" and create a subdirectory for the build (e.g. called
"build"). Then click "Configure". Select "MinGW Makefiles" as the generator and
click "Finish". Wait a bit (until the progress bar is full). Then click
"Generate".

You can now build Teeworlds by executing `mingw32-make` in the build directory.


