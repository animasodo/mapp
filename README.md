# MapProcessor
...or *mapp* for short. Simple CLI tool I put together for compressing maps exported as binaries from CharPad or some other graphics editor and converting them to C-style data.

The compression is pretty simple: since I'm only really gonna be using about 16 tiles for maps that I can swap around by writing on the character set, I'm using the upper nibble for storing how many of these tiles there are in a straight horizontal line.
So if I have a map that has {water, water water, terrain, terrain, water}, the compressed version will be {3 water, 2 terrain, 1 water}.