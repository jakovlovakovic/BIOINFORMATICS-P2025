#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include "BambooFilter.h"
extern "C" {
#include "hash/xxhash.h"  
}

#define K 15

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

	// zapocni brojanje vremena
	auto start = std::chrono::high_resolution_clock::now();
	// dodavanje elemenata u filter
	for (size_t i = 0; i + K <= fileContent.size(); i++) {
		bf.insert(fileContent.substr(i, K));
	}

	// provjera da li je element u filteru
	//bool wrong = true;
	//int j = 0;
	for (size_t i = 0; i + K <= fileContent.size(); i++) {
		//j++;
		auto val = bf.lookup(fileContent.substr(i, K));
		if (val == false) {
			//wrong = false;
			break;
		}
	}

	//if (wrong == true) {
		//std::cout << "TOCNO " << j << std::endl;
	//}

	// zavrsi brojanje vremena
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = end - start;
	std::cout << "Elapsed time: " << elapsed.count() << " seconds" << std::endl;

	// zapocni brojanje vremena
	start = std::chrono::high_resolution_clock::now();
	std::cout << bf.lookup("TGGAAAACATACTGC") << std::endl;
	// zavrsi brojanje vremena
	end = std::chrono::high_resolution_clock::now();
	elapsed = end - start;
	std::cout << "Elapsed time: " << elapsed.count() << " seconds" << std::endl;

	// zapocni brojanje vremena
	start = std::chrono::high_resolution_clock::now();
	std::cout << bf.lookup("TGGAAAACAMMMTGC") << std::endl;
	// zavrsi brojanje vremena
	end = std::chrono::high_resolution_clock::now();
	elapsed = end - start;
	std::cout << "Elapsed time: " << elapsed.count() << " seconds" << std::endl;

	// DEBUG STATEMENTS:
	/*
	std::cout << bf.lookup("TGGAAAACATACTGC") << std::endl;
	std::cout << bf.lookup("TGGAAAACAMMMTGC") << std::endl;
	std::cout << "BOk";
	uint32_t tralelrotralala = reconstructHash1(1, 1548575, 3);
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