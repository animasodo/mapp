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
    ]
}
```

## Exported binary structure

```
[2 bytes] header (0x4d, 0x50 for maps)
[1 byte] width
[1 byte] height
[2 bytes] compressed map length in little endian
[x bytes] compressed map data, uses RLE (upper nibble is length + 1, lower nibble is tile)
*** optional data ***
** warps **
[1 byte] warp header (0x57)
[x bytes] map name, null terminated
[1 byte] source x location
[1 byte] source y location
[1 byte] destiny x location
[1 byte] destiny y location
```

The structure may be subject to changes in the future, this is still a WIP.