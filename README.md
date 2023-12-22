# AAPP-Challenges

This repository contains the two challenges for the Advanced Algorithms and Parallel Programming course at the Politecnico di Milano A.Y. 2023/2024.

## Sequence Alignment (Advanced Algorithms)

The `sequence_alignment` folder contains the code for the first challenge. The code is written in C and it is based on the Needleman-Wunsch algorithm.

Also, a constrained version of the algorithm is implemented, where the maximum number of possible gaps is fixed (minimum length alignment).

Finally, there is a function to generate sequences of maximum distance given their length.

[More details](Sequence_Alignment/README.md)

## SMILES Coverage (Parallel Programming)

The `smiles_coverage` folder contains the code for the second challenge. The code is written in C++ and MPI is used to parallelize the computation.

The parallelized version is accompanied by a sequential version, which is used to compare the results.

[More details](SMILES_coverage/README.md)
