
# HOW TO BUILD #

1. `mkdir build & cd build`
2. `cmake ..`
3. `make`

I made the makefile here to so that I can remake it without going into a different directory

# DEPENDENCY #

We have lots of dependencies but obviously the most notable one is LLVM-11.
To install LLVM-11, with Ubuntu 20.04, 
`sudo apt-get install clang-11`
You might also need to make the bin/clang into symbolic link of bin/clang-11
To install cmake, 
`sudo apt-get install cmake`

This might still result in compiler error for not including some header files that are for LLVM, if so let me know
