# mapp

This is a collection of tools used to build my [C64 CRPG project](https://github.com/animasodo/c64-crpg). It contains the following tools:

- _mapp_, to compress and pack a map file, attributes, and scripts into a single binary. Uses [json](https://github.com/nlohmann/json).
- _pack-assets_, to insert data into a `.d64` disk file without altering the BAM. Uses a manifest text file.
- _manifest-gen_, to create a `manifest.inc` file for the assembler out of a text file.
- _bytecode-comp_, to compile scripts into bytecode that will be interpreted by the game during runtime.
