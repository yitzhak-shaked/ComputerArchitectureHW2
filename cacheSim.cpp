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
unsigned int gMemAccessTime = -1;  // Memory access time
bool gWriteAllocate = false;  	  // Write-Allocate mode

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

int power(int base, int exp) {
	int result = 1;
	for (int i = 0; i < exp; ++i) {
		result *= base;
	}
	return result;
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
	mask = (1u << (end - start + 1u)) - 1u;
	mask = mask << start;

	return (value & mask) >> start;
}

int parseAddress(uint32_t address, unsigned int blockSize, unsigned int numSets,
				 uint32_t &tag, uint32_t &setIndex, uint32_t &blockOffset) {
	/** Address Structure:
	 * [1 .. 0]														: 00
	 * [<2+log2(blockSize)-1> .. 2]									: Block offset
	 * [<2+log2(blockSize)-1+log2(numSets)-1 .. <2+log2(blockSize)>]: Set index
	 * [31 .. <2+log2(blockSize)+log2(numSets)-1>]					: Tag
	 */
	
	unsigned leftIdxOffset = 1 + log2(blockSize);
	unsigned leftIdxSets = leftIdxOffset + log2(numSets);
	unsigned leftIdxTag = 31;
	
	blockOffset = get_bits(address, 2, leftIdxOffset);
	setIndex = get_bits(address, leftIdxOffset + 1, leftIdxSets);
	tag = get_bits(address, leftIdxSets + 1, leftIdxTag);

	return 0;
}

/******************************************************************************
 * Classes
 *****************************************************************************/
//  ~ ~ ~ Block class ~ ~ ~
class Block {
private:
	// Block parameters
	uint32_t tag;
	uint32_t blockAddress;
	unsigned long int timeStamp;  // For LRU replacement policy
	bool dirtyBit;
	bool validBit;
	
public:
	Block(uint32_t tag = 0, uint32_t blockAddress = 0, unsigned long int timeStamp = 0,
			bool dirtyBit = false, bool validBit = false) :
			tag(tag), blockAddress(blockAddress), timeStamp(timeStamp),
			dirtyBit(dirtyBit), validBit(validBit) {}

	~Block() {}

	// = Operator
	Block& operator=(const Block& other) {
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
	uint32_t getTag() { return tag; }
	uint32_t getBlockAddress() { return blockAddress; }
	unsigned long int getTimeStamp() { return timeStamp; }
	bool isDirty() { return dirtyBit; }
	bool isValid() { return validBit; }
	void setTag(uint32_t tag) { this->tag = tag; }
	void setBlockAddress(uint32_t blockAddress) { this->blockAddress = blockAddress; }
	void setTimeStamp(unsigned long int timeStamp) { this->timeStamp = timeStamp; }
	void setDirty(bool dirty) { this->dirtyBit = dirty; }
	void setValid(bool valid) { this->validBit = valid; }

	// Functions

};

// ~ ~ ~ Cache Way ~ ~ ~
class CacheWay {
private:
	unsigned int numBlocks;
	Block *blocks;

public:
	CacheWay(unsigned int numBlocks) {
		this->numBlocks = numBlocks;
		blocks = new Block[numBlocks];
	}

	~CacheWay() {
		delete[] blocks;
	}

	Block& getBlock(int index) { return blocks[index]; }
	unsigned int getNumBlocks() { return numBlocks; }
	
};

// ~ ~ ~ Cache class ~ ~ ~
class Cache {
private:
	// Cache parameters
	unsigned int size;
	unsigned int blockSize;
	unsigned int associativity;
	unsigned int numBlocks;
	unsigned int numWays;
	unsigned int numSets;
	unsigned int missCount;
	unsigned int accCount;
	int accessTime;
	CacheWay **pWayArr;

public:
	Cache(unsigned long int size, unsigned long int blockSize, unsigned long int associativity) : 
			size(size), blockSize(blockSize), associativity(associativity), missCount(0), accCount(0) {
		// Initialize parameters
		numBlocks = size / blockSize;
		numWays = power(2, associativity);
		numSets = numBlocks / numWays;

		pWayArr = new CacheWay*[numSets];
		for (unsigned int i = 0; i < numSets; ++i) {
			pWayArr[i] = new CacheWay(numWays);
		}
	}

	~Cache() {
		for (unsigned int i = 0; i < numSets; ++i) {
			delete pWayArr[i];
		}
		delete[] pWayArr;
	}
	
	// Getters & Setters
	unsigned int getAccessTime() { return accessTime; }
	unsigned int getMissCount() { return missCount; }
	unsigned int getAccCount() { return accCount; }

	// Target functions
	Block* access_cache(uint32_t address, char operation, unsigned long int accumulatedTime) {
		accCount++;

		// Parsing the address
		uint32_t blockOffset, setIndex, tag;
		parseAddress(address, blockSize, numSets, tag, setIndex, blockOffset);

		// Searching for the block in the cache
		for (unsigned int i = 0; i < numWays; i++) {
			Block &matchingBlock = pWayArr[i]->getBlock(setIndex);
			if (matchingBlock.isValid() && matchingBlock.getTag() == tag) {
				// Cache Hit
				matchingBlock.setTimeStamp(accumulatedTime);  // "Touch" block for LRU policy
				if (operation == 'W') {
					// If write operation, set dirty bit
					matchingBlock.setDirty(true);
				}
				return &matchingBlock;
			}
		}

		// Cache Miss
		missCount++;

		return nullptr;
	}

	void fetch_block_into_cache(uint32_t address, unsigned long int accumulatedTime, uint32_t *evictedAddress, bool* dirty = nullptr) {
		// Parsing the address
		uint32_t blockOffset, setIndex, tag;
		parseAddress(address, blockSize, numSets, tag, setIndex, blockOffset);
		
		// Looking for a vacant block in ways
		for (unsigned int i = 0; i < numWays; i++) {
			Block &matchingBlock = pWayArr[i]->getBlock(setIndex);
			if (!matchingBlock.isValid()) {
				// If block is not valid, set it as valid and set the tag
				matchingBlock.setValid(true);
				matchingBlock.setTag(tag);
				matchingBlock.setBlockAddress(address);
				matchingBlock.setDirty(false);  // Reset dirty bit
				matchingBlock.setTimeStamp(accumulatedTime);  // "Touch" block for LRU policy
				return;
			}
		}
		
		// Conflict miss - need to evict a block
		// Finding the oldest block in the set as per LRU policy
		Block &oldestBlock = pWayArr[0]->getBlock(setIndex);
		for (unsigned int i = 1; i < numWays; i++) {
			Block &matchingBlock = pWayArr[i]->getBlock(setIndex);
			if (matchingBlock.getTimeStamp() < oldestBlock.getTimeStamp()) {
				oldestBlock = matchingBlock;
			}
		}

		// Return the evicted address
		*evictedAddress = oldestBlock.getBlockAddress();

		// Evicting the oldest block
		if (oldestBlock.isDirty()) {
			// If the block is dirty, write it back to memory
			// TODO: Should remain empty?
			if (dirty != nullptr) {
				*dirty = true;
			}
		}

		// Updating the block with the new data
		oldestBlock.setTag(tag);
		oldestBlock.setBlockAddress(address);
		oldestBlock.setTimeStamp(accumulatedTime);  // "Touch" block for LRU policy
		oldestBlock.setValid(true);
		oldestBlock.setDirty(false);  // Reset dirty bit
	}

	Block* get_block(uint32_t address) {
		// Parsing the address
		uint32_t blockOffset, setIndex, tag;
		parseAddress(address, blockSize, numSets, tag, setIndex, blockOffset);

		// Searching for the block in the cache
		for (unsigned int i = 0; i < numWays; i++) {
			Block &matchingBlock = pWayArr[i]->getBlock(setIndex);
			if (matchingBlock.isValid() && matchingBlock.getTag() == tag) {
				// Cache Hit
				return &matchingBlock;
			}
		}

		return nullptr;
	}
};

// ~ ~ ~ TwoLevelCache class ~ ~ ~
class TwoLevelCache {
private:
	unsigned long int accumulatedTime;
	unsigned long int accCount;
	unsigned int L1AccessTime;
	unsigned int L2AccessTime;
	Cache *L1;
	Cache *L2;

public:
	TwoLevelCache(unsigned long int L1Size, unsigned long int L1BlockSize, unsigned long int L1Associativity, unsigned int L1AccessTime,
				  unsigned long int L2Size, unsigned long int L2BlockSize, unsigned long int L2Associativity, unsigned int L2AccessTime) :
				  accumulatedTime(0), accCount(0), L1AccessTime(L1AccessTime), L2AccessTime(L2AccessTime) {
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
	unsigned long int getAccTime() { return accumulatedTime; }
	Cache* getL1() { return L1; }
	Cache* getL2() { return L2; }

	// Target functions
	void access_cache(uint32_t address, char operation) {
		this->accCount++;

		// Access L1 cache
		this->accumulatedTime += L1->getAccessTime();
		if (L1->access_cache(address, operation, this->accumulatedTime)) {
			// If hit, return
			return;
		}

		// If miss, access L2 cache
		this->accumulatedTime += L2->getAccessTime();
		if (L2->access_cache(address, operation, this->accumulatedTime)) {
			// If hit, return
			return;
		}

		// If miss, access memory
		this->accumulatedTime += gMemAccessTime;

		/** TODO: Validate
		 * How to sync the caches and force inclusivity?					By implementing the process from here -> code below
		 * Should the block be feched to all cache levels?					YES! -> Same as above
		 * While accessing L2 and L1, should we increment the access time?	Seems like NO -> Creating a new function
		 * After fetching block from memory should we reaccess L2?			unavoidable, but without incrementing counters -> Same as above.
		 * Remind me the benefit of inclusivity?
		 */
		if (operation == 'R' || gWriteAllocate) {
			Block *block;
			
			// Fetch the block into LLC
			uint32_t evictedAddress = (uint32_t)-1;
			L2->fetch_block_into_cache(address, this->accumulatedTime, &evictedAddress);

			// If a block was evicted from L2, invalidate it in L1
			if (evictedAddress != (uint32_t)-1) {
				block = L1->get_block(evictedAddress);
				if (block != nullptr) {
					block->setValid(false);
				}
			}

			// Fetch the block into L1
			bool dirty = false;
			evictedAddress = (uint32_t)-1;
			L1->fetch_block_into_cache(address, this->accumulatedTime, &evictedAddress, &dirty);
			
			// If a block was evicted from L1 and it was dirty, write it back to L2
			if (evictedAddress != (uint32_t)-1 && dirty) {
				// Updating the block in L2
				L2->get_block(evictedAddress)->setDirty(true);
			}
		}
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
	gMemAccessTime = MemCyc;
	gWriteAllocate = WrAlloc;

	// Creating the caches
	TwoLevelCache cache(L1Size, BSize, L1Assoc, L1Cyc,
						L2Size, BSize, L2Assoc, L2Cyc);

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
		cache.access_cache(binAddress, operation);
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
void test_parseAddress() {
	cout << "Testing parseAddress function..." << endl;
	uint32_t address = 0x12345678;
	unsigned int blockSize = 16;
	unsigned int numSets = 4;
	uint32_t tag, setIndex, blockOffset;

	parseAddress(address, blockSize, numSets, tag, setIndex, blockOffset);
	cout << "\tAddress: " << std::bitset<32>(address) << endl;
	cout << "\tBlock Size: " << blockSize << endl;
	cout << "\tNumber of Sets: " << numSets << endl;
	cout << "Parsed Address:" << endl;
	cout << "\tTag: " << std::bitset<32>(tag) << endl;
	cout << "\tSet Index: " << std::bitset<32>(setIndex) << endl;
	cout << "\tBlock Offset: " << std::bitset<32>(blockOffset) << endl;
}

/******************************************************************************
 * Temporary Main Function for Testing
 *****************************************************************************/
int _main() {
	test_parseAddress();

	return 0;
}

/* Output:
Testing parseAddress function...
        Address: 00010010001101000101011001111000
        Block Size: 16
        Number of Sets: 4
Parsed Address:
        Tag: 00000000000100100011010001010110
        Set Index: 00000000000000000000000000000001
        Block Offset: 00000000000000000000000000001110
*/
