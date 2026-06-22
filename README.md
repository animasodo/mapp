# MapProcessor

...or *mapp* for short. Command line tool I put together for compressing maps exported as binaries from CharPad or some other graphics editor and converting them to the file format used in my [C64 CRPG project](https://github.com/animasodo/c64-crpg).

Uses [json](https://github.com/nlohmann/json).

## Imported JSON structure

You can attach a json to insert metadata and entity data using the `-j` option. Here's an example of the expected JSON structure.

```json
{
    "width": 32,
    "height": 32,
    "warps": [
        {
            "map": "overworld",
            "src_x": 4,
            "src_y": 7,
            "dst_x": 8,
            "dst_y": 10
        },
        {
            "map": "volcano",
            "src_x": 8,
            "src_y": 3,
            "dst_x": 13,
            "dst_y": 21
        }
    ],
    "doors": [
        {
            "x": 5,
            "y": 6
        }
    ]
}
```

## Exported binary structure

```
[2 bytes] header (0x4d, 0x50 for maps)
[1 byte] width
[1 byte] height
[2 bytes] compressed map length in little endian
[x bytes] compressed map data, uses a hybrid RLE method:
    - if the horizontal length is less than 8, it fits inside the three most significant bits of the byte. the game adds one to the length, so if the three bits are 000, it's actually one character
    - if it's 8 or more, the three bits are set to 111 and the actual length is in the next byte

*** optional data ***

** warps **
[1 byte] warp header (0x57)
- if a CSV file is used: [1 byte] map id
- if no CSV is used: [x bytes] map name, null terminated
[1 byte] source x location
[1 byte] source y location
[1 byte] destiny x location
[1 byte] destiny y location

** doors **
[1 byte] door header (0x44)
[1 byte] x location
[1 byte] y location

[1 byte] EOF marker (0x45)
```

The structure may be subject to changes in the future, this is still a WIP.