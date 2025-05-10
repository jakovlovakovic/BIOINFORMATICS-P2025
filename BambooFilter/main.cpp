#include <iostream>
#include <vector>
#include "BambooFilter.h"
extern "C" {  
	#include "hash/xxhash.h"  
}

int main(void) {
	BambooFilter bf;

	//bf.segments[0].buckets[0][0] = 1;

	//std::cout << "TEST" << std::endl;

	bf.getFingertip("test");
	bf.getBucketIndex("test");
	bf.getAlternateBucketIndex("test");
	bf.getSegmentIndex("test");
	bf.getSegmentIndexWithCorrection("test");
	
	return 0;
}