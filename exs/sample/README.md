# EXPLANATION OF THIS DIRECTORY #
This directory is of an example program that shows both non-determinism in execution paths because of communication order and inputs. 
Communication order non-determinism occurs because of `MPI_ANY_SOURCE` in `MPI_Recv` in main.c, with this in the execution, we could have either rank 1 or 2 send the message in non-determinate order. 
Input non-determinism occurs because of two input files: origInput.txt and reproduceInput.txt. This makes rank 1 send the data either 0 or 1, and because of it, rank 0 takes different execution paths. 

Our paper aims to create a tool that controls the communication order non-determinism, which allows users to analyze the effect of non-determinism because of inputs, and find points of divergence/convergence. 

# HOW TO BUILD #

## PRELIMINARIES ##
You first need MPI to build this program. 

`sudo apt install -y openmpi-bin openmpi-common libopenmpi-dev`

Then you also need to install [wllvm](https://github.com/travitch/whole-program-llvm).
If you need other llvm tools, install via llvm-11 for consistency, do not install LLVM as is, or else it would cause A LOT of versioning issues. 

`sudo apt-get install llvm-11 llvm-11-dev`

If you seem to have the llvm toolset that's available with the name (toolname)-11 but not as (toolname), simply make a symbolic link for it in the path that's available for you. 

Also, you need to use `wllvm` and `wllvm++` under the hood of MPICC and MPICXX.
To do that, in the command line, go
```
export MPICC=wllvm
export MPICXX=wllvm++
```
or write these in .bashrc to save time.

## BUILD ##

simply go:
``` make ```

Then, if you want to extract the bitcode out of the executable, go
```extract-bc main``` 

This would create a file called main.bc. To insert the print statement, 
you need to first go into skeleton directory at the top of this repo and follow the instructions there 
(which you would probably get stuck with, and that's why we want the meeting tomorrow).

You should go, after you make the shared object file in skeleton directory,

```
opt -load ../../skeleton/build/libSkeletonPass.so --bbprinter -o main.mod.bc < main.bc
```

If you can make main.mod.bc, compile it further as an executable by 

```
mpicc -o main.mod main.mod.bc printbb.c
```

run it with the command

```
mpirun -n 3 ./main.mod origInput.txt
```



