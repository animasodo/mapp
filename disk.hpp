#ifndef DISK_H
#define DISK_H

#include <cstdint>
#include <utility>
#include <vector>

extern const int SECTORS_PER_TRACK[];
extern const int NUM_TRACKS;
extern const long DISK_BYTES;

long track_offset(int track);
long block_offset(int track, int sector);

uint8_t* bam_ptr(std::vector<uint8_t>& disk);
bool bam_is_free(std::vector<uint8_t>& disk, int track, int sector);
void bam_set_used(std::vector<uint8_t>& disk, int track, int sector);

std::vector<std::pair<int,int>> all_blocks_in_order();

#endif