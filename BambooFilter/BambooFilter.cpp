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
#define SEGMENT_SIZE 16

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
	
	this->overflow = std::vector<uint32_t>();
}

// konstruktor za BambooFilter
BambooFilter::BambooFilter() {
	// koristimo 32-bitni hash
	this->i = 0; // runda ekspanzije
	this->p = 0; // index sljedeceg za expandanje
	this->n0 = 524288; // pocetan broj segmenata (2^19)
	this->lf = 11; // velicina fingertipa
	this->lb = 2; // velicina indexa bucketa (2 bita)
	this->ls0 = 19; // velicina indexa segmenta (19 bitova)
	this->Me = SEGMENT_SIZE; // varijable Me iz insert funkcije
	this->expansionTrigger = 0; // trigger za ekspanziju

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

			// provjeri treba li expendat, i expandaj ako treba
			if (this->expansionTrigger >= SEGMENT_SIZE) {
				this->expansionTrigger = 0;
				this->expand();
				return true;
			}
			expansionTrigger++;

			return true;
		}
	}

	// provjerimo ima li bucket s indexom IbAlternate prazan entry
	for (size_t j = 0; j < BUCKET_SIZE; j++) {
		if (this->segments[Is].buckets[IbAlternate][j] == EMPTY_BUCKET_ELEMENT) {
			this->segments[Is].buckets[IbAlternate][j] = f;

			// provjeri treba li expendat, i expandaj ako treba
			if (this->expansionTrigger >= SEGMENT_SIZE) {
				this->expansionTrigger = 0;
				this->expand();
				return true;
			}
			expansionTrigger++;

			return true;
		}
	}

	// randomly selectaj Ib ili IbAlternate
	uint16_t ib = (dis(gen) >= BUCKET_SIZE / 2) ? Ib : IbAlternate;
	uint16_t ibOld = Ib;
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
				if (this->expansionTrigger >= SEGMENT_SIZE) {
					expansionTrigger = 0;
					this->expand();
					return true;
				}
				expansionTrigger++;
				return true;
			}
		}

		ibOld = ib; // spremi stari ib
		ib = ibAlternate; // priprema za sljedecu iteraciju
	}

	uint32_t reconstructedHash = this->reconstructHash(f, Is, ibOld); // rekonstruacija hasha
	this->segments[Is].overflow.push_back(reconstructedHash); // dodaj u overflow

	// pozivanje expansiona
	if (this->expansionTrigger >= SEGMENT_SIZE) {
		this->expansionTrigger = 0;
		this->expand();
		return true;
	}
	expansionTrigger++;

	return true;
}

// funkcija koja prosiruje fitler
bool BambooFilter::expand() {
	// provjerimo da li mozemo expandat
	if (this->i >= this->lf) return true;

	// kreiramo novi segment
	unsigned int num_of_buckets = 1 << lb;
	Segment segment(num_of_buckets, BUCKET_SIZE);
	this->segments.push_back(segment);
	uint32_t newSegmentIndex = this->segments.size() - 1; // index novog segmenta

	// iteriramo se po segmentu kojeg trenutno expandom
	uint16_t bucketIndex = 0;
	for (auto& bucket : this->segments[this->p].buckets) {
		for (size_t j = 0; j < BUCKET_SIZE; j++) {
			if (bucket[j] != EMPTY_BUCKET_ELEMENT) {
				uint16_t f = bucket[j]; // fingertip

				// izracunaj treba li fingertip u novi segment
				uint16_t temp = (f >> this->i) << 15;
				// ako je ovaj if true, onda mora u novi segment
				if (temp > 0) {
					// makni element iz starog segmenta
					bucket[j] = EMPTY_BUCKET_ELEMENT;

					// izracunaj alternativni bucket index
					uint16_t fingertipXORBucketIndex = bucketIndex ^ f;
					uint16_t alternateBucketIndex =
						fingertipXORBucketIndex % static_cast<uint16_t>(std::pow(2.0, this->lb));

					// insertaj element u novi segment
					bool insertSuccesful = false;
					for (size_t k = 0; k < BUCKET_SIZE; k++) {
						if (this->segments[newSegmentIndex].buckets[bucketIndex][k] == EMPTY_BUCKET_ELEMENT) {
							this->segments[newSegmentIndex].buckets[bucketIndex][k] = f;
							insertSuccesful = true;
							break;
						}
					}
					// insertaj element u novi segment (u alternativni bucket ako je orginalni zauzet)
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
					// insertaj element u novi segment (u overflow, ako su bucketi zauzeti)
					if (!insertSuccesful) {
						uint32_t reconstructedHash = this->reconstructHash(f, newSegmentIndex, bucketIndex); // rekonstruacija hasha
						this->segments[newSegmentIndex].overflow.push_back(reconstructedHash);
					}
				}
			}
		}
		bucketIndex++;
	}
	
	// iteriramo se po overflowu
	std::vector<uint32_t> newOverflowForOldSegment; // zamjenski overflow za stari segment
	newOverflowForOldSegment = std::vector<uint32_t>();

	for (auto element : this->segments[this->p].overflow) {
		uint16_t f = (element >> (this->lb + this->ls0))
			% static_cast<uint16_t>((std::pow(2.0, this->lf))); // fingertip

		uint16_t overflowBucketIndex = element % static_cast<uint16_t>(std::pow(2.0, this->lb)); // bucket index

		// alternativni bucket index
		uint16_t fingertipXORBucketIndex = overflowBucketIndex ^ f;
		uint16_t alternateOverflowBucketIndex =
			fingertipXORBucketIndex % static_cast<uint16_t>(std::pow(2.0, this->lb));
		
		// dobij segment index
		uint32_t segmentIndex = (element >> this->lb) %
			(static_cast<uint32_t>(std::pow(2.0, this->i + 1)) * this->n0);

		// ako treba u novi segment
		if (segmentIndex == newSegmentIndex) {
			// insertaj element u novi segment
			bool insertSuccesful = false;
			for (size_t k = 0; k < BUCKET_SIZE; k++) {
				if (this->segments[newSegmentIndex].buckets[overflowBucketIndex][k] == EMPTY_BUCKET_ELEMENT) {
					this->segments[newSegmentIndex].buckets[overflowBucketIndex][k] = f;
					insertSuccesful = true;
					break;
				}
			}
			// insertaj element u novi segment (u alternativni bucket ako je orginalni zauzet)
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
			// insertaj element u novi segment (u overflow, ako su bucketi zauzeti)
			if (!insertSuccesful) {
				this->segments[newSegmentIndex].overflow.push_back(element); // dodaj u overflow novog segmenta
			}
		}
		// ako je segment index jednak starom segmentu provjeri mozes li rasporedit neke elemente u njega
		else if (segmentIndex == this->p) {
			// insertaj element u bucket u TRENUTNOM segmentu
			bool insertSuccesful = false;
			for (size_t k = 0; k < BUCKET_SIZE; k++) {
				if (this->segments[this->p].buckets[overflowBucketIndex][k] == EMPTY_BUCKET_ELEMENT) {
					this->segments[this->p].buckets[overflowBucketIndex][k] = f;
					insertSuccesful = true;
					break;
				}
			}
			// insertaj element u bucket u TRENUTNOM segmentu (ako je orginalni bucket pun)
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
			// ostavi element u overflowu
			if (!insertSuccesful) {
				newOverflowForOldSegment.push_back(element); // dodaj u overflow novog segmenta ako nema slobodnog mjesta
			}
		}

	}
	this->segments[this->p].overflow = newOverflowForOldSegment; // zamjeni overflow starog segmenta

	this->p++; // povecaj index na index sljedeceg segmenta

	if (this->p == static_cast<uint16_t>(std::pow(2.0, this->i)) * this->n0) {
		this->p = 0; // resetiraj index
		this->i++; // povecaj rundu ekspanzije
	}

	return true;
}

// funkcija za provjeru da li je element u BambooFilteru
bool BambooFilter::lookup(std::string entry) {
	// dobij sve potrebne elemente
	uint16_t f = this->getFingertip(entry);
	uint16_t Ib = this->getBucketIndex(entry);
	uint16_t IbAlternate = this->getAlternateBucketIndex(entry);
	uint32_t Is = this->getSegmentIndex(entry);
	if (Is >= this->segments.size())
		Is = this->getSegmentIndexWithCorrection(entry);

	// pogledaj je li element u bucketima
	for (size_t j = 0; j < BUCKET_SIZE; j++) {
		if (this->segments[Is].buckets[Ib][j] == f) {
			return true;
		}
		if (this->segments[Is].buckets[IbAlternate][j] == f) {
			return true;
		}
	}

	// pogledaj je li element u overflowu
	uint32_t reconstructedHash = this->reconstructHash(f, Is, Ib); // rekonstruacija hasha
	uint32_t reconstructedHashAlternate = this->reconstructHash(f, Is, IbAlternate); // rekonstruacija hasha s alternativnim bucketom
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