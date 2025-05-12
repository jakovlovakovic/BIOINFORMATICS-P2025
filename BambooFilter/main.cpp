#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include "BambooFilter.h"
extern "C" {
#include "hash/xxhash.h"  
}

int main(void) {
	BambooFilter bf; // inicijalizacija filtera

	// citanje filea
	std::ifstream file("GCA_000005845.2_ASM584v2_genomic.fna");
	if (!file.is_open()) {
		return 1;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	std::string fileContent = buffer.str();
	file.close();

	fileContent.erase(std::remove(fileContent.begin(), fileContent.end(), '\n'), fileContent.end());

	// dodavanje elemenata u filter
	for (size_t i = 0; i + 15 <= fileContent.size(); i++) {
		bf.insert(fileContent.substr(i, 15));
	}


	// DEBUG STATEMENTS:
	/*
	std::cout << "BOk";
	uint32_t tralelrotralala = reconstructHash1(1, 1048575, 3);
	std::cout << tralelrotralala;
	bf.segments[0].buckets[0][0] = 1;

	std::cout << "TEST" << std::endl;

	bf.getFingertip("test");
	bf.getBucketIndex("test");
	bf.getAlternateBucketIndex("test");
	bf.getSegmentIndex("test");
	bf.getSegmentIndexWithCorrection("test");
	*/


	return 0;
}