#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct Segment {
	std::vector<std::vector<uint16_t>> buckets; // bucketi jednog segmenta
	std::vector<uint32_t> overflow; // overflow dio segmenta

	Segment(size_t num_of_buckets, size_t bucket_size);
};

// class for Bamboo Filter
class BambooFilter {
private:
	size_t i, p, n0, lf, lb, ls0, Me, expansionTrigger; // runda ekspanzije, index sljedeceg za expand, pocetni broj segmenata, velicina fingertipa, velicina bucketa indexa, pocetna velicina segment indexa, varijable Me iz insert funkcije, expansion trigger
	std::vector<Segment> segments; // segmenti
	size_t getBitLength(uint32_t n); // helper funckija koja vraca bit length broja n
	uint32_t reconstructHash(uint16_t f, uint32_t Is, uint16_t Ib); // helper funkcija koja rekonstruira hash

public:
	BambooFilter();

	uint16_t getFingerprint(std::string entry); // funkcija koja vraca fingertip za string entry
	uint16_t getBucketIndex(std::string entry); // funkcija koja vraca bucket index za string entry
	uint16_t getAlternateBucketIndex(std::string entry); // funkcija koja vraca alternativni bucket index za string entry
	uint32_t getSegmentIndex(std::string entry); // funkcija koja vraca segment index za string entry
	uint32_t getSegmentIndexWithCorrection(std::string entry); // funkcija koja vraca segment index za string entry ako prethodni ne pristaje

	bool insert(std::string entry); // funkcija koja ubacuje string entry u filter
	bool expand(); // funkcija koja prosiruje fitler
	bool lookup(std::string entry); // funkcija koja provjera da li string entry postoji u filteru
};