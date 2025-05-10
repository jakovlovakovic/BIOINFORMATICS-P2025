#include "BambooFilter.h"
#include <iostream>
#include <cmath>
extern "C" {
	#include "hash/xxhash.h"  
}

// konstruktor za segment
Segment::Segment(size_t num_of_buckets, size_t bucket_size) {
	for (int j = 0; j < num_of_buckets; j++) {
		std::vector<uint16_t> bucket;
		bucket.resize(bucket_size);
		buckets.push_back(bucket);
	}

	overflow = std::vector<uint16_t>();
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

	unsigned int num_of_buckets = 1 << lb;

    for (int j = 0; j < n0; j++) {
		Segment segment(num_of_buckets, 4); // inicijaliziramo segment (1 << lb je 2^lb)
        segments.push_back(segment);
    }
}

// funkcija za racunanje fingertipa
uint16_t BambooFilter::getFingertip(std::string entry) {
	uint32_t hash = XXH32(entry.c_str(), entry.length(), 0);
	uint16_t fingertip = (hash >> (this->lb + this->ls0)) % static_cast<uint16_t>((std::pow(2.0, this->lf))); // fingertip
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
	uint16_t alternateBucketIndex = fingertipXORBucketIndex % static_cast<uint16_t>(std::pow(2.0, this->lb));
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