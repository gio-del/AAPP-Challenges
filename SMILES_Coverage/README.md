# SMILES Coverage

This folder contains the code for the second challenge. The code is written in C++ and MPI is used to parallelize the computation.

## Repository structure

Instructions for building and running the application are provided in this [README](parallel/README.md) file.

## Challenge objectives

The goal of the program is:
> Given a set of molecules in SMILES format, we want to compute the occurrences (coverage) of substrings in the molecules. The length of the substrings is from 1 to `max_length`, which is a parameter of the application.

### How to compute the coverage

The serial program simply iterates over all the lengths from 1 to `max_length` and for each length, it computes the coverage of all the possible substrings of that length.

### How to parallelize the computation

The main idea is that there is a master process that reads the input file and sends the molecules to the other processes. Then, the master process sends to the other processes the starting and ending index of the molecules that they have to process, splitting the work as evenly as possible. Note that this part is polynomial in the number of processes as the master do not need to actually iterate over the molecules.

Then, each process computes the coverage of the molecules in the range that it has been assigned. This is the expensive part of the computation, but since all the processors are working on different molecules, there is no need for synchronization and the program scales almost linearly.

Finally, the results are gathered by the master process and printed to the output file.
