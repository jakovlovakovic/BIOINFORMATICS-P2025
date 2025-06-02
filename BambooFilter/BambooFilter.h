#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct Segment {
	std::vector<std::vector<uint16_t>> buckets; // buckets of a single segment
	std::vector<uint32_t> overflow; // overflow part of the segment

	Segment(size_t num_of_buckets, size_t bucket_size);
};

// class for Bamboo Filter
class BambooFilter {
private:
	size_t i, p, n0, lf, lb, ls0, Me, expansionTrigger; // expantion round, next segment to expand, initial segment, fingerprint size, bucket index size, starting segment index size, variable Me from insert, expansion trigger
	std::vector<Segment> segments; // segment
	size_t getBitLength(uint32_t n); // helper function that returns the bit length of n
	uint32_t reconstructHash(uint16_t f, uint32_t Is, uint16_t Ib); // helper function that reconstructs hash

public:
	BambooFilter();

	uint16_t getFingerprint(std::string entry); // function that returns fingerprint for string entry
	uint16_t getBucketIndex(std::string entry); // function that returns bucket index for string entry
	uint16_t getAlternateBucketIndex(std::string entry); // function that returns alternate bucket index for string entry
	uint32_t getSegmentIndex(std::string entry); // function that returns segment index for string entry
	uint32_t getSegmentIndexWithCorrection(std::string entry); // function that returns segment index for string entry with correction

	bool insert(std::string entry); // function that inserts string entry into the filter
	bool expand(); // function that expands the filter
	bool lookup(std::string entry); //function that checks if string entry is in the filter
};