# Level9

Level9 is an interpreter for the games by the English text adventure company Level 9. These games received considerable critical acclaim at the time and are probably the best text adventures written for the small cassette based 8-bit computers common in Britain during the early to mid 1980s.

The Level9 interpreter was originally written by Glen Summers, and later extended by myself, along with Alan Staniforth, Simon Baldwin, Dieter Baron and Andreas Scherrer. It has the very useful ability to search for Level 9 game data in any file loaded into it, allowing it to find games in uncompressed memory dumps from emulators (such as the commonly used Spectrum emulator SNA file format). Both the line-drawn graphics used in a number of Level 9's earlier games and the bitmap graphics from later games are also supported.

![Level9 playing Red Moon](Red%20Moon.png)

## Building

Download and install Visual Studio 2019 Community edition from https://visualstudio.microsoft.com/. In the installer, under "Workloads", make sure that "Desktop development with C++" is selected.

Install git. I use the version of git that is part of Cygwin, a Linux-like environment for Windows, but Git for Windows can be used from a Windows command prompt.

Open the environment that you are using git from (e.g. Cygwin), and switch to the root directory that the build environment will be created under. Clone this repository with git:
```
git clone https://github.com/DavidKinder/Level9.git
```
Start Visual Studio, open the solution "Level9.sln", then build and run the "Level9" project.
