/* 046267 Computer Architecture - HW #2                                      */
/* Athors: Shaked Yitzhak - 322704776, Idan Simon - 207784653		         */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Cashe Simulator ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/******************************************************************************
 * Includes
 *****************************************************************************/
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <bitset>
#include <cstdint>

/******************************************************************************
 * usings
 *****************************************************************************/
using std::FILE;
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::ifstream;
using std::stringstream;
using std::bitset;

/******************************************************************************
 * Constants
 *****************************************************************************/

 /******************************************************************************
 * Global Variables
 *****************************************************************************/
unsigned int memAccessTime = -1;  // Memory access time

/******************************************************************************
 * Declarations
 *****************************************************************************/

/******************************************************************************
 * Helper Functions
 *****************************************************************************/
 int log2(int x) {
	int log = 0;
	while (x > 1) {
		x >>= 1;
		log++;
	}
	return log;
}

uint32_t dec_to_bin(uint32_t dec) {
	uint32_t bin = 0;
	int i = 1;
	while (dec > 0) {
		bin += (dec % 2) * i;
		dec /= 2;
		i *= 10;
	}
	return bin;
}

uint32_t get_bits(uint32_t value, unsigned start, unsigned end) {
	// Get bits from start to end (inclusive)
	if (start > end || start < 0 || end >= 32) {
		cerr << "Error: get_bits recieved an invalid bit range" << endl;
		return 0;
	}
	uint32_t mask;
	mask = ((1u << (end - start + 1u)) - 1u) << start;

	return (value & mask) >> start;
}

/******************************************************************************
 * Classes
 *****************************************************************************/
//  ~ ~ ~ Block class ~ ~ ~
class Block {
private:
	// Block parameters
	unsigned int tag;
	unsigned int blockAddress;
	unsigned long int timeStamp;  // For LRU replacement policy
	bool dirtyBit;
	bool validBit;
	
public:
	Block() {  // TODO: Rewrite function
		tag = 0;
		blockAddress = 0;
		timeStamp = 0;
		dirtyBit = false;
		validBit = false;
	}

	// = Operator
	Block& operator=(const Block& other) {  // TODO: Rewrite function
		if (this != &other) {
			tag = other.tag;
			blockAddress = other.blockAddress;
			timeStamp = other.timeStamp;
			dirtyBit = other.dirtyBit;
			validBit = other.validBit;
		}
		return *this;
	}

	// Getters & Setters
	unsigned long int getTag() { return tag; }
	void setTag(unsigned long int tag) { this->tag = tag; }

	unsigned long int getBlockAddress() { return blockAddress; }
	void setBlockAddress(unsigned long int blockAddress) { this->blockAddress = blockAddress; }

	unsigned long int getTimeStamp() { return timeStamp; }
	void setTimeStamp(unsigned long int timeStamp) { this->timeStamp = timeStamp; }

	bool isDirty() { return dirtyBit; }
	void setDirty(bool dirtyBit) { this->dirtyBit = dirtyBit; }

	bool isValid() { return validBit; }
	void setValid(bool validBit) { this->validBit = validBit; }

	// Functions

};

// ~ ~ ~ Cache class ~ ~ ~
class Cache {
private:
	// Cache parameters
	unsigned int size;
	unsigned int blockSize;
	unsigned int associativity;
	unsigned int numWays;
	unsigned int numSets;
	unsigned int numBlocks;
	unsigned int missCount;
	unsigned int accCount;
	int accessTime;
	Block **cache;

public:
	Cache(unsigned long int size, unsigned long int blockSize,
		unsigned long int associativity) {  // TODO: Rewrite function
		this->size = size;
		this->blockSize = blockSize;
		this->associativity = associativity;
		this->numSets = size / (blockSize * associativity);
		this->numBlocks = size / blockSize;

		cache = new Block*[numSets];
		for (unsigned long int i = 0; i < numSets; i++) {
			cache[i] = new Block[associativity];
		}
	}

	~Cache() {  // TODO: Rewrite function
		for (unsigned long int i = 0; i < numSets; i++) {
			delete[] cache[i];
		}
		delete[] cache;
	}

	// Getters & Setters
	unsigned int getAccessTime() { return accessTime; }
	unsigned int getMissCount() { return missCount; }
	unsigned int getAccCount() { return accCount; }
	
	// Target functions
	bool accessCache(uint32_t address, char operation) {  // TODO: Rewrite function
		unsigned long int blockAddress = address / blockSize;
		unsigned long int setIndex = blockAddress % numSets;
		unsigned long int tag = blockAddress / numSets;

		for (unsigned long int i = 0; i < associativity; i++) {
			if (cache[setIndex][i].isValid() && cache[setIndex][i].getTag() == tag) {
				// Cache hit
				accCount++;
				cache[setIndex][i].setTimeStamp(accCount);
				return true;
			}
		}

		// Cache miss
		missCount++;
		accCount++;

		for (unsigned long int i = 0; i < associativity; i++) {
			if (!cache[setIndex][i].isValid()) {
				cache[setIndex][i].setTag(tag);
				cache[setIndex][i].setBlockAddress(blockAddress);
				cache[setIndex][i].setValid(true);
				cache[setIndex][i].setTimeStamp(accCount);
				return false;
			}
		}

		// Evict a block using LRU policy
		unsigned long int lruIndex = 0;
		for (unsigned long int i = 1; i < associativity; i++) {
			if (cache[setIndex][i].getTimeStamp() < cache[setIndex][lruIndex].getTimeStamp()) {
				lruIndex = i;
			}
		}

		cache[setIndex][lruIndex].setTag(tag);
		cache[setIndex][lruIndex].setBlockAddress(blockAddress);
		cache[setIndex][lruIndex].setDirty(false);
		cache[setIndex][lruIndex].setTimeStamp(accCount);

		return false;
	}
};

// ~ ~ ~ TwoLevelCache class ~ ~ ~
class TwoLevelCache {
private:
	unsigned long int acumulatedTime;
	unsigned long int accCount;
	Cache *L1;
	Cache *L2;

public:
	TwoLevelCache(unsigned long int L1Size, unsigned long int L1BlockSize,
			unsigned long int L1Associativity, unsigned long int L2Size,
			unsigned long int L2BlockSize, unsigned long int L2Associativity) {
		// Initialize parameters
		acumulatedTime = 0;
		accCount = 0;

		// Create L1 and L2 caches
		L1 = new Cache(L1Size, L1BlockSize, L1Associativity);
		L2 = new Cache(L2Size, L2BlockSize, L2Associativity);
	}

	~TwoLevelCache() {
		delete L1;
		delete L2;
	}

	// Getters
	unsigned long int getAccCount() { return accCount; }
	unsigned long int getAccTime() { return acumulatedTime; }
	Cache* getL1() { return L1; }
	Cache* getL2() { return L2; }

	// Target functions
	void accessCache(uint32_t address, char operation) {
		accCount++;
		// Access L1 cache
		this->acumulatedTime += L1->getAccessTime();
		if (L1->accessCache(address, operation)) {
			// If hit, return
			return;
		}

		// If miss, access L2 cache
		this->acumulatedTime += L2->getAccessTime();
		if (L2->accessCache(address, operation)) {
			// If hit, return
			return;
		}

		// If miss, access memory
		this->acumulatedTime += memAccessTime;
	}
};

/******************************************************************************
 * Main Function
 *****************************************************************************/
int main(int argc, char **argv) {

	if (argc < 19) {
		cerr << "Not enough arguments" << endl;
		return 0;
	}

	// Get input arguments

	// File
	// Assuming it is the first argument
	char* fileString = argv[1];
	ifstream file(fileString); //input file stream
	string line;
	if (!file || !file.good()) {
		// File doesn't exist or some other error
		cerr << "File not found" << endl;
		return 0;
	}

	// Parsing the arguments
	unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
			L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

	for (int i = 2; i < 19; i += 2) {
		string s(argv[i]);
		if (s == "--mem-cyc") {
			MemCyc = atoi(argv[i + 1]);
		} else if (s == "--bsize") {
			BSize = atoi(argv[i + 1]);
		} else if (s == "--l1-size") {
			L1Size = atoi(argv[i + 1]);
		} else if (s == "--l2-size") {
			L2Size = atoi(argv[i + 1]);
		} else if (s == "--l1-cyc") {
			L1Cyc = atoi(argv[i + 1]);
		} else if (s == "--l2-cyc") {
			L2Cyc = atoi(argv[i + 1]);
		} else if (s == "--l1-assoc") {
			L1Assoc = atoi(argv[i + 1]);
		} else if (s == "--l2-assoc") {
			L2Assoc = atoi(argv[i + 1]);
		} else if (s == "--wr-alloc") {
			WrAlloc = atoi(argv[i + 1]);
		} else {
			cerr << "Error in arguments" << endl;
			return 0;
		}
	}

	// Updating the global variables
	memAccessTime = MemCyc;

	// Creating the caches
	TwoLevelCache cache(L1Size, BSize, L1Assoc, L2Size, BSize, L2Assoc);

	// Executing input lines
	while (getline(file, line)) {
		stringstream ss(line);
		string address;
		char operation = 0; // read (R) or write (W)
		if (!(ss >> operation >> address)) {
			// Operation appears in an Invalid format
			cout << "Command Format error" << endl;
			return 0;
		}

		// DEBUG - remove this line
		cout << "operation: " << operation;

		string cutAddress = address.substr(2); // Removing the "0x" part of the address

		// DEBUG - remove this line
		cout << ", address (hex)" << cutAddress;

		unsigned long int num = 0;
		num = strtoul(cutAddress.c_str(), NULL, 16);

		// DEBUG - remove this line
		cout << " (dec) " << num << endl;
		
		uint32_t binAddress = dec_to_bin(num);
		// DEBUG - remove this line
		cout << "Address in binary: " << binAddress << endl;

		// Accessing the cache
		cache.accessCache(binAddress, operation);
	}

	double L1MissRate = (double)cache.getL1()->getMissCount() / (double)cache.getL1()->getAccCount();
	double L2MissRate = (double)cache.getL2()->getMissCount() / (double)cache.getL2()->getAccCount();
	double avgAccTime = (double)cache.getAccTime() / (double)cache.getAccCount();

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);

	return 0;
}

/******************************************************************************
 * Test Functions
 *****************************************************************************/

/******************************************************************************
 * Temporary Main Function for Testing
 *****************************************************************************/
int _main() {
	// test_history();
	// test_fsm();
	// test_power();
	// test_log2();
	// test_get_bits();
	// test_calculate_tag();

	return 0;
}

/* Output:

*/