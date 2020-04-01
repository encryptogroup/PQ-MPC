# PQ-MPC

This repository contains the code for the paper
*"Secure Two-Party Computation in a Quantum World"*
by Niklas Büscher, Daniel Demmler, Nikolaos P. Karvelas, Stefan Katzenbeisser, Juliane Krämer, Deevashwer Rathee, Thomas Schneider, and Patrick Struck, which will appear at [ACNS'20](https://sites.google.com/di.uniroma1.it/ACNS2020).

## Required packages:
 - libgmp-dev 
 - SEAL (version 3.1.0)

## Compilation

To compile the library:
```
mkdir build && cd build
cmake ..
make
// or make -j 4 for faster compilation
```

## Tests

To compile the tests, run `cmake -DBUILD_TESTS=ON .. && make` in `build/`.
Then run the test binaries in `build/bin/` as follows to make sure everything works as intended:

```
./[test] 1 [port] & ./[test] 2 [port]
```
