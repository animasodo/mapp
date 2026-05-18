# MapProcessor

...or *mapp* for short. Simple CLI tool I put together for compressing maps exported as binaries from CharPad or some other graphics editor and converting them to the file format used in my [C64 CRPG project](https://github.com/animasodo/c64-crpg).

Each map exported uses the following structure: the first byte defines the type of data (0 for maps). If the data is a map, the next two bytes define the size (width and height). The rest of the binary is the compressed map data.