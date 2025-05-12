#include "BambooFilter.h"
#include <iostream>
#include <cmath>
#include <random>
#include <utility>
extern "C" {
	#include "hash/xxhash.h"  
}

#define EMPTY_BUCKET_ELEMENT 32768 // oznaka praznog elementa u bucketu
#define BUCKET_SIZE 4

// random number generator
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(0, BUCKET_SIZE - 1);

// konstruktor za segment
Segment::Segment(size_t num_of_buckets, size_t bucket_size) {
	for (size_t j = 0; j < num_of_buckets; j++) {
		std::vector<uint16_t> bucket;
		bucket.resize(bucket_size);

		for (size_t k = 0; k < bucket_size; k++) {
			bucket[k] = EMPTY_BUCKET_ELEMENT; // inicijaliziramo sve elemente u bucketu na oznaku praznog elementa
		}

		buckets.push_back(bucket);
	}

	overflow = std::vector<uint32_t>();
}

// konstruktor za BambooFilter
BambooFilter::BambooFilter() {
	// koristimo 32-bitni hash
	i = 0; // runda ekspanzije
	p = 0; // index sljedeceg za expandanje
	n0 = 524288; // pocetan broj segmenata (2^19)
	lf = 11; // velicina fingertipa
	lb = 2; // velicina indexa bucketa (2 bita)
	ls0 = 19; // velicina indexa segmenta (19 bitova)
	Me = 16; // varijable Me iz insert funkcije

	unsigned int num_of_buckets = 1 << lb;

    for (int j = 0; j < n0; j++) {
		Segment segment(num_of_buckets, BUCKET_SIZE); // inicijaliziramo segment (1 << lb je 2^lb), TODO: dodati velicinu bucketa
        this->segments.push_back(segment);
    }
}

// funkcija za racunanje fingertipa
uint16_t BambooFilter::getFingertip(std::string entry) {
	uint32_t hash = XXH32(entry.c_str(), entry.length(), 0);
	uint16_t fingertip = (hash >> (this->lb + this->ls0))
		% static_cast<uint16_t>((std::pow(2.0, this->lf))); // fingertip
	return fingertip;
}

// funkcija za racunanje bucket indexa
uint16_t BambooFilter::getBucketIndex(std::string entry) {
	uint32_t hash = XXH32(entry.c_str(), entry.length(), 0);
	uint16_t bucketIndex = hash % static_cast<uint16_t>(std::pow(2.0, this->lb));
	return bucketIndex;
}

// funkcija za racunanje alternativnog bucket indexa
uint16_t BambooFilter::getAlternateBucketIndex(std::string entry) {
	uint16_t fingertipXORBucketIndex = this->getBucketIndex(entry) ^ this->getFingertip(entry);
	uint16_t alternateBucketIndex =
		fingertipXORBucketIndex % static_cast<uint16_t>(std::pow(2.0, this->lb));
	return alternateBucketIndex;
}

// funkcija za racunanje segment indexa
uint32_t BambooFilter::getSegmentIndex(std::string entry) {
	uint32_t hash = XXH32(entry.c_str(), entry.length(), 0);
	uint32_t segmentIndex = (hash >> this->lb) % 
		(static_cast<uint32_t>(std::pow(2.0, this->i + 1)) * this->n0);
	return segmentIndex;
}

// funkcija za racunanje segment indexa s korekcijom
uint32_t BambooFilter::getSegmentIndexWithCorrection(std::string entry) {
	uint32_t hash = XXH32(entry.c_str(), entry.length(), 0);
	uint32_t segmentIndex = (hash >> this->lb) %
		(static_cast<uint32_t>(std::pow(2.0, this->i + 1)) * this->n0) -
		(static_cast<uint32_t>(std::pow(2.0, this->i)) * this->n0);
	return segmentIndex;
}

// helper funkcija za dobivanje velicine broja u bitovima
size_t BambooFilter::getBitLength(uint32_t n) {
	if (n == 0) {
		return 0;
	}
	return static_cast<int>(std::log2(n)) + 1;
}

// funkcija za rekonstrukciju hasha
uint32_t BambooFilter::reconstructHash(uint16_t f, uint32_t Is, uint16_t Ib) {
	size_t segmentIndexSize = this->getBitLength(Is); // dobij velicinu segment indexa
	
	// izracunaj koliko "krademo" bitova fingertipa
	size_t difference;
	if (segmentIndexSize <= this->ls0) difference = 0;
	else difference = segmentIndexSize - this->ls0;

	uint32_t mask = UINT32_MAX << difference; // izracun maske koja ce se koristiti za operaciju i nad fingertipom

	uint32_t newFingertip = (f & mask) << (this->ls0 + this->lb); // izracunaj novi fingertip

	uint32_t segementIndexBucketIndex = (Is << this->lb) | Ib; // izracunaj spojeni segment index i bucket index

	uint32_t reconstructedHash = newFingertip | segementIndexBucketIndex; // izracunaj rekonstruirani hash

	return reconstructedHash;
}

// funkcija za umetanje elementa u BambooFilter
bool BambooFilter::insert(std::string entry) {
	// fingertip
	uint16_t f = this->getFingertip(entry);
	// bucket index
	uint16_t Ib = this->getBucketIndex(entry);
	// alternate bucket index
	uint16_t IbAlternate = this->getAlternateBucketIndex(entry);
	// segment index
	uint32_t Is = this->getSegmentIndex(entry);

	// segment index correction
	if (Is >= this->segments.size())
		Is = this->getSegmentIndexWithCorrection(entry);

	// provjerimo ima li bucket s indexom Ib prazan entry
	for (size_t j = 0; j < BUCKET_SIZE; j++) {
		if (this->segments[Is].buckets[Ib][j] == EMPTY_BUCKET_ELEMENT) {
			this->segments[Is].buckets[Ib][j] = f;
			return true;
		}
	}

	// provjerimo ima li bucket s indexom IbAlternate prazan entry
	for (size_t j = 0; j < BUCKET_SIZE; j++) {
		if (this->segments[Is].buckets[IbAlternate][j] == EMPTY_BUCKET_ELEMENT) {
			this->segments[Is].buckets[IbAlternate][j] = f;
			return true;
		}
	}

	// randomly selectaj Ib ili IbAlternate
	uint16_t ib = (dis(gen) >= BUCKET_SIZE / 2) ? Ib : IbAlternate;
	for (size_t loop = 0; loop < this->Me; loop++) {
		// randomly selectaj element iz bucketa
		uint16_t randomIndex = dis(gen);
		std::swap(f, this->segments[Is].buckets[ib][randomIndex]);

		// dobimo alternativni bucket za "novi" f
		uint16_t fingertipXORBucketIndex = ib ^ f;
		uint16_t ibAlternate =
			fingertipXORBucketIndex % static_cast<uint16_t>(std::pow(2.0, this->lb));

		// provjerimo ima li bucket s indexom IbAlternate prazan entry
		for (size_t j = 0; j < BUCKET_SIZE; j++) {
			if (this->segments[Is].buckets[ibAlternate][j] == EMPTY_BUCKET_ELEMENT) {
				this->segments[Is].buckets[ibAlternate][j] = f;
				return true;
			}
		}

		ib = ibAlternate; // priprema za sljedecu iteraciju
	}

	this->segments[Is].overflow.push_back(f); // TODO: Ovo obavezno izmijeni
	return true;
}