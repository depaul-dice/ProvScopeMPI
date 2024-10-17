
# HOW TO BUILD #

1. `mkdir build & cd build`
2. `cmake ..`
3. `make`

I made the makefile here to so that I can remake it without going into a different directory

# DEPENDENCIES #

You need to install LLVM-11 for this via 

`sudo apt-get install clang-11`

This might only install `clang-11` and not as `clang`, so you might want to make a symbolic link for `clang` if necessary.
Don't make LLVM from the llvm-project as it is huge and takes up so much space. 

Also, install `cmake` if necessary

`sudo apt-get install cmake`

Hopefully, this will work, but it might complain that you need some header files for this. In that case let's resolve this together. 
