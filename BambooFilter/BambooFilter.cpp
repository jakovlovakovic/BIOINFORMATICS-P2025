#include "BambooFilter.h"
#include <iostream>
#include <cmath>
#include <random>
#include <utility>
extern "C" {
	#include "hash/xxhash.h"  
}

#define EMPTY_BUCKET_ELEMENT 32768 // represents an empty bucket element
#define BUCKET_SIZE 4
#define SEGMENT_SIZE 16

// random number generator
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(0, BUCKET_SIZE - 1);

// konstruktor for a segment
Segment::Segment(size_t num_of_buckets, size_t bucket_size) {
	for (size_t j = 0; j < num_of_buckets; j++) {
		std::vector<uint16_t> bucket;
		bucket.resize(bucket_size);

		for (size_t k = 0; k < bucket_size; k++) {
			bucket[k] = EMPTY_BUCKET_ELEMENT; // initialise every element as en empty bucket element
		}

		buckets.push_back(bucket);
	}
	
	this->overflow = std::vector<uint32_t>();
}

// konstruktor for the BambooFilter
BambooFilter::BambooFilter() {
	// using 32-bit hash
	this->i = 0; // expention round
	this->p = 0; // next segment index to expand
	this->n0 = 524288; // initial number of segments (2^19)
	this->lf = 11; // fingerprint size (11 bits)
	this->lb = 2; // bucket index size (2 bits)
	this->ls0 = 19; // segment index size (19 bits)
	this->Me = SEGMENT_SIZE; // variable Me from insert function
	this->expansionTrigger = 0; // expansion trigger

	unsigned int num_of_buckets = 1 << lb;

    for (int j = 0; j < n0; j++) {
		Segment segment(num_of_buckets, BUCKET_SIZE); // inicialise segment (1 << lb is 2^lb), TODO: dodati velicinu bucketa
        this->segments.push_back(segment);
    }
}

// calculate fingerprint
uint16_t BambooFilter::getFingerprint(std::string entry) {
	uint32_t hash = XXH32(entry.c_str(), entry.length(), 0);
	uint16_t fingerprint = (hash >> (this->lb + this->ls0))
		% static_cast<uint16_t>((std::pow(2.0, this->lf))); // fingerprint
	return fingerprint;
}

// calculate bucket index
uint16_t BambooFilter::getBucketIndex(std::string entry) {
	uint32_t hash = XXH32(entry.c_str(), entry.length(), 0);
	uint16_t bucketIndex = hash % static_cast<uint16_t>(std::pow(2.0, this->lb));
	return bucketIndex;
}

// calculate alternate bucket index
uint16_t BambooFilter::getAlternateBucketIndex(std::string entry) {
	uint16_t fingerprintXORBucketIndex = this->getBucketIndex(entry) ^ this->getFingerprint(entry);
	uint16_t alternateBucketIndex =
		fingerprintXORBucketIndex % static_cast<uint16_t>(std::pow(2.0, this->lb));
	return alternateBucketIndex;
}

// calculate segment indexa
uint32_t BambooFilter::getSegmentIndex(std::string entry) {
	uint32_t hash = XXH32(entry.c_str(), entry.length(), 0);
	uint32_t segmentIndex = (hash >> this->lb) % 
		(static_cast<uint32_t>(std::pow(2.0, this->i + 1)) * this->n0);
	return segmentIndex;
}

// calculate segment indexa with correction
uint32_t BambooFilter::getSegmentIndexWithCorrection(std::string entry) {
	uint32_t hash = XXH32(entry.c_str(), entry.length(), 0);
	uint32_t segmentIndex = (hash >> this->lb) %
		(static_cast<uint32_t>(std::pow(2.0, this->i + 1)) * this->n0) -
		(static_cast<uint32_t>(std::pow(2.0, this->i)) * this->n0);
	return segmentIndex;
}

// helper funkction gets the bit length of a number
size_t BambooFilter::getBitLength(uint32_t n) {
	if (n == 0) {
		return 0;
	}
	return static_cast<int>(std::log2(n)) + 1;
}

// reconstruct hash from fingerprint, segment index and bucket index
uint32_t BambooFilter::reconstructHash(uint16_t f, uint32_t Is, uint16_t Ib) {
	size_t segmentIndexSize = this->getBitLength(Is); // get segment index size
	
	// calculate the difference between segment index size and ls0
	size_t difference;
	if (segmentIndexSize <= this->ls0) difference = 0;
	else difference = segmentIndexSize - this->ls0;

	uint32_t mask = UINT32_MAX << difference; // calculate operation mask

	uint32_t newFingerprint = (f & mask) << (this->ls0 + this->lb); // calculate new fingerprint

	uint32_t segementIndexBucketIndex = (Is << this->lb) | Ib; // calculate segment index and bucket index joined

	uint32_t reconstructedHash = newFingerprint | segementIndexBucketIndex; // the whole reconstructed hash

	return reconstructedHash;
}

// insert function of the BambooFilter
bool BambooFilter::insert(std::string entry) {
	// fingerprint
	uint16_t f = this->getFingerprint(entry);
	// bucket index
	uint16_t Ib = this->getBucketIndex(entry);
	// alternate bucket index
	uint16_t IbAlternate = this->getAlternateBucketIndex(entry);
	// segment index
	uint32_t Is = this->getSegmentIndex(entry);

	// segment index correction
	if (Is >= this->segments.size())
		Is = this->getSegmentIndexWithCorrection(entry);

	// check if the bucket with index Ib has an empty entry
	for (size_t j = 0; j < BUCKET_SIZE; j++) {
		if (this->segments[Is].buckets[Ib][j] == EMPTY_BUCKET_ELEMENT) {
			this->segments[Is].buckets[Ib][j] = f;

			// check the expention trigger expand if necessary
			if (this->expansionTrigger >= SEGMENT_SIZE) {
				this->expansionTrigger = 0;
				this->expand();
				return true;
			}
			expansionTrigger++;

			return true;
		}
	}

	// check if the alternate bucket with index IbAlternate has an empty entry
	for (size_t j = 0; j < BUCKET_SIZE; j++) {
		if (this->segments[Is].buckets[IbAlternate][j] == EMPTY_BUCKET_ELEMENT) {
			this->segments[Is].buckets[IbAlternate][j] = f;

			// check the expention trigger expand if necessary
			if (this->expansionTrigger >= SEGMENT_SIZE) {
				this->expansionTrigger = 0;
				this->expand();
				return true;
			}
			expansionTrigger++;

			return true;
		}
	}

	// randomly select Ib or IbAlternate
	uint16_t ib = (dis(gen) >= BUCKET_SIZE / 2) ? Ib : IbAlternate;
	uint16_t ibOld = Ib;
	for (size_t loop = 0; loop < this->Me; loop++) {
		// randomly select element from bucketa
		uint16_t randomIndex = dis(gen);
		std::swap(f, this->segments[Is].buckets[ib][randomIndex]);

		// get alternate bucket for "new" f
		uint16_t fingerprintXORBucketIndex = ib ^ f;
		uint16_t ibAlternate =
			fingerprintXORBucketIndex % static_cast<uint16_t>(std::pow(2.0, this->lb));

		// check if the bucket with index IbAlternate has en empty entry
		for (size_t j = 0; j < BUCKET_SIZE; j++) {
			if (this->segments[Is].buckets[ibAlternate][j] == EMPTY_BUCKET_ELEMENT) {
				this->segments[Is].buckets[ibAlternate][j] = f;
				if (this->expansionTrigger >= SEGMENT_SIZE) {
					expansionTrigger = 0;
					this->expand();
					return true;
				}
				expansionTrigger++;
				return true;
			}
		}

		ibOld = ib; // spremi old ib
		ib = ibAlternate; // preparation for next iteration
	}

	uint32_t reconstructedHash = this->reconstructHash(f, Is, ibOld); // rekonstruact the hash
	this->segments[Is].overflow.push_back(reconstructedHash); // add to overflow

	// call expansion
	if (this->expansionTrigger >= SEGMENT_SIZE) {
		this->expansionTrigger = 0;
		this->expand();
		return true;
	}
	expansionTrigger++;

	return true;
}

// expand function of the BambooFilter
bool BambooFilter::expand() {
	// check if we can expand
	if (this->i >= this->lf) return true;

	// create a new segment
	unsigned int num_of_buckets = 1 << lb;
	Segment segment(num_of_buckets, BUCKET_SIZE);
	this->segments.push_back(segment);
	uint32_t newSegmentIndex = this->segments.size() - 1; // index for new segmenta

	// iterate over the current segment that is being expanded
	uint16_t bucketIndex = 0;
	for (auto& bucket : this->segments[this->p].buckets) {
		for (size_t j = 0; j < BUCKET_SIZE; j++) {
			if (bucket[j] != EMPTY_BUCKET_ELEMENT) {
				uint16_t f = bucket[j]; // fingeprint

				// calculate if the fingerprint needs to be moved into the new segment
				uint16_t temp = (f >> this->i) << 15;
				// if true, then move it to the new segment
				if (temp > 0) {
					// remove the element from the old segment
					bucket[j] = EMPTY_BUCKET_ELEMENT;

					// calculate alternate bucket index
					uint16_t fingerprintXORBucketIndex = bucketIndex ^ f;
					uint16_t alternateBucketIndex =
						fingerprintXORBucketIndex % static_cast<uint16_t>(std::pow(2.0, this->lb));

					// insert element into new segment
					bool insertSuccesful = false;
					for (size_t k = 0; k < BUCKET_SIZE; k++) {
						if (this->segments[newSegmentIndex].buckets[bucketIndex][k] == EMPTY_BUCKET_ELEMENT) {
							this->segments[newSegmentIndex].buckets[bucketIndex][k] = f;
							insertSuccesful = true;
							break;
						}
					}
					// insert element into new segment (alternate bucket)
					if (!insertSuccesful) {
						insertSuccesful = false;
						for (size_t k = 0; k < BUCKET_SIZE; k++) {
							if (this->segments[newSegmentIndex].buckets[alternateBucketIndex][k] == EMPTY_BUCKET_ELEMENT) {
								this->segments[newSegmentIndex].buckets[alternateBucketIndex][k] = f;
								insertSuccesful = true;
								break;
							}
						}
					}
					// insert element into new segment (overflow)
					if (!insertSuccesful) {
						uint32_t reconstructedHash = this->reconstructHash(f, newSegmentIndex, bucketIndex); // reconstruct hash
						this->segments[newSegmentIndex].overflow.push_back(reconstructedHash);
					}
				}
			}
		}
		bucketIndex++;
	}
	
	// iterate over the overflow
	std::vector<uint32_t> newOverflowForOldSegment; // new overflow for old segment
	newOverflowForOldSegment = std::vector<uint32_t>();

	for (auto element : this->segments[this->p].overflow) {
		uint16_t f = (element >> (this->lb + this->ls0))
			% static_cast<uint16_t>((std::pow(2.0, this->lf))); // fingerprint

		uint16_t overflowBucketIndex = element % static_cast<uint16_t>(std::pow(2.0, this->lb)); // bucket index

		// alternate bucket index
		uint16_t fingerprintXORBucketIndex = overflowBucketIndex ^ f;
		uint16_t alternateOverflowBucketIndex =
			fingerprintXORBucketIndex % static_cast<uint16_t>(std::pow(2.0, this->lb));
		
		// get segment index
		uint32_t segmentIndex = (element >> this->lb) %
			(static_cast<uint32_t>(std::pow(2.0, this->i + 1)) * this->n0);

		// insert element into the new segment if necessary
		if (segmentIndex == newSegmentIndex) {
			// insert element into new segment
			bool insertSuccesful = false;
			for (size_t k = 0; k < BUCKET_SIZE; k++) {
				if (this->segments[newSegmentIndex].buckets[overflowBucketIndex][k] == EMPTY_BUCKET_ELEMENT) {
					this->segments[newSegmentIndex].buckets[overflowBucketIndex][k] = f;
					insertSuccesful = true;
					break;
				}
			}
			// insert element into new segment (alternate bucket)
			if (!insertSuccesful) {
				insertSuccesful = false;
				for (size_t k = 0; k < BUCKET_SIZE; k++) {
					if (this->segments[newSegmentIndex].buckets[alternateOverflowBucketIndex][k] == EMPTY_BUCKET_ELEMENT) {
						this->segments[newSegmentIndex].buckets[alternateOverflowBucketIndex][k] = f;
						insertSuccesful = true;
						break;
					}
				}
			}
			// insert element into new segment (overflow)
			if (!insertSuccesful) {
				this->segments[newSegmentIndex].overflow.push_back(element); // add element to the overflow of new segment
			}
		}
		// if the segment index is equal to the old segment index, check if you can distribute some elements into it
		else if (segmentIndex == this->p) {
			// insert element into bucket in the CURRENT segment
			bool insertSuccesful = false;
			for (size_t k = 0; k < BUCKET_SIZE; k++) {
				if (this->segments[this->p].buckets[overflowBucketIndex][k] == EMPTY_BUCKET_ELEMENT) {
					this->segments[this->p].buckets[overflowBucketIndex][k] = f;
					insertSuccesful = true;
					break;
				}
			}
			// insert element into alternate bucket in the CURRENT segment (if the bucket is full)
			if (!insertSuccesful) {
				insertSuccesful = false;
				for (size_t k = 0; k < BUCKET_SIZE; k++) {
					if (this->segments[this->p].buckets[alternateOverflowBucketIndex][k] == EMPTY_BUCKET_ELEMENT) {
						this->segments[this->p].buckets[alternateOverflowBucketIndex][k] = f;
						insertSuccesful = true;
						break;
					}
				}
			}
			// leave the element in the overflow
			if (!insertSuccesful) {
				newOverflowForOldSegment.push_back(element); // add to the overflow
			}
		}

	}
	this->segments[this->p].overflow = newOverflowForOldSegment; // replace the old overflow with the new one

	this->p++; // change the index to the next segment index

	if (this->p == static_cast<uint16_t>(std::pow(2.0, this->i)) * this->n0) {
		this->p = 0; // reset index
		this->i++; // increase expantion round
	}

	return true;
}

// lookup function of the BambooFilter
bool BambooFilter::lookup(std::string entry) {
	// get all the necessary elements
	uint16_t f = this->getFingerprint(entry);
	uint16_t Ib = this->getBucketIndex(entry);
	uint16_t IbAlternate = this->getAlternateBucketIndex(entry);
	uint32_t Is = this->getSegmentIndex(entry);
	if (Is >= this->segments.size())
		Is = this->getSegmentIndexWithCorrection(entry);

	// check if it is in one of the buckets
	for (size_t j = 0; j < BUCKET_SIZE; j++) {
		if (this->segments[Is].buckets[Ib][j] == f) {
			return true;
		}
		if (this->segments[Is].buckets[IbAlternate][j] == f) {
			return true;
		}
	}

	// check if it is in the overflow
	uint32_t reconstructedHash = this->reconstructHash(f, Is, Ib); // rekonstruct hash
	uint32_t reconstructedHashAlternate = this->reconstructHash(f, Is, IbAlternate); // rekonstruct hash with alternate bucketom
	for (auto element : this->segments[Is].overflow) {
		if (element == reconstructedHash) {
			return true;
		}
		if (element == reconstructedHashAlternate) {
			return true;
		}
	}

	return false;
}