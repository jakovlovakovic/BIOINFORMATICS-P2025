#pragma once
#include <string>
#include <vector>

struct Segment {
	std::vector<std::vector<uint16_t>> buckets; // bucketi jednog segmenta
	std::vector<uint16_t> overflow; // overflow dio segmenta

	Segment(size_t num_of_buckets, size_t bucket_size);
};

// TODO: trebamo dodati i kriterij za koliko se insertiona radi ekspanzija
class BambooFilter {
private:
	int i, p, n0, lf, lb, ls0; // runda ekspanzije, index sljedeceg za expand, pocetni broj segmenata, velicina fingertipa, velicina bucketa indexa, pocetna velicina segment indexa
	std::vector<Segment> segments; // segmenti

public:
	BambooFilter();
	//~BambooFilter();

	uint16_t getFingertip(std::string entry); // funkcija koja vraca fingertip za string entry
	uint16_t getBucketIndex(std::string entry); // funkcija koja vraca bucket index za string entry
	uint16_t getAlternateBucketIndex(std::string entry); // funkcija koja vraca alternativni bucket index za string entry
	uint32_t getSegmentIndex(std::string entry); // funkcija koja vraca segment index za string entry
	uint32_t getSegmentIndexWithCorrection(std::string entry); // funkcija koja vraca segment index za string entry ako prethodni ne pristaje

	//bool insert(std::string);
	//bool expand();
	//bool lookup(std::string);
};