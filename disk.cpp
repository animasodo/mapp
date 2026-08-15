#include <cstdint>
#include <utility>
#include <vector>
#include "disk.hpp"

// disk geometry (number of sectors in each track)
// in theory we could go up to 41 but that's scary and might not be very reliable?

const int SECTORS_PER_TRACK[] = {21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21, // 1-17
                                 19,19,19,19,19,19,19,                               // 18-24
                                 18,18,18,18,18,18,                                  // 25-30
                                 17,17,17,17,17};                                    // 31-35
const int NUM_TRACKS = 35;
const long DISK_BYTES = 683 * 256;

// returns the offset of a track in bytes
long track_offset(int track) {
    long offset = 0;
    for (int t = 1; t < track; ++t){
        offset += SECTORS_PER_TRACK[t - 1] * 256;
    }
    return offset;
}

// returns the offset of a sector in bytes
long block_offset(int track, int sector) {
    return track_offset(track) + sector * 256;
}

// BAM helpers (18:0)
//
// the layout of the BAM is as follows:
//
// the first four-byte group uses this structure:
// byte 0-1 = track and sector of first directory sector
// byte 2 = DOS version
// byte 3 = unused
//
// after all that, it's just this:
// byte 4 = number of free sectors on the track
// byte 5-7 = bitmap of sectors, 1 is free and 0 is occupied. 4 is lowest and 6 is highest

// return the BAM pointer
uint8_t* bam_ptr(std::vector<uint8_t>& disk) {
    return &disk[block_offset(18, 0)];
}

// go through bitmap, check if a specific sector is free (1)
bool bam_is_free(std::vector<uint8_t>& disk, int track, int sector) {
    uint8_t* bam = bam_ptr(disk);
    int base = 4 + (track - 1) * 4; // four byte initial offset + (track - 1) * four byte sector data
    int byte_idx = base + 1 + (sector / 8); // base + one byte offset + (sector / 8 bits per byte)
    int bit_idx = sector % 8;
    return (bam[byte_idx] >> bit_idx) & 1;
}

void bam_set_used(std::vector<uint8_t>& disk, int track, int sector) {
    uint8_t* bam = bam_ptr(disk);
    int base = 4 + (track - 1) * 4;
    int byte_idx = base + 1 + (sector / 8);
    int bit_idx = sector % 8;
    if ((bam[byte_idx] >> bit_idx) & 1) {
        bam[byte_idx] &= ~(1 << bit_idx);
        if (bam[base] > 0) bam[base]--;
    }
}

// get ordered list of sectors as a pair (t,s) minus track 18
std::vector<std::pair<int,int>> all_blocks_in_order() {
    std::vector<std::pair<int,int>> blocks;
    for (int t = 1; t <= NUM_TRACKS; ++t) {
        if (t == 18) continue; // first rule of the 1541: NEVER use track 18 (if it's a booter disk)
        for (int s = 0; s < SECTORS_PER_TRACK[t - 1]; ++s){
            blocks.push_back({t, s});
        }
    }
    return blocks;
}