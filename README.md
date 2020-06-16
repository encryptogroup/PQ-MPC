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

## Acknowledgements

The following directories contain code from external repositories:

* `emp-tool`: This is a modified stripped-down version of [emp-tool](https://github.com/emp-toolkit/emp-tool/tree/master/emp-tool) with changes to support 256-bit labels and substitute `AES-128` with `AES-256`.
* `pq-yao`: This is a modified version of [emp-sh2pc](https://github.com/emp-toolkit/emp-sh2pc/tree/master/emp-sh2pc) with changes for integration with PQ-OT and addition of support for PQ-Yao garbling.
* `test`: This directory contains test files from [emp-sh2pc/test](https://github.com/emp-toolkit/emp-sh2pc/tree/master/test).
