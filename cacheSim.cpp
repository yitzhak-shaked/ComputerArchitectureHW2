/* 046267 Computer Architecture - HW #2                                      */
/* Athors: Shaked Yitzhak - 322704776, Idan Simon - 207784653				 */
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
unsigned int gMemAccessTime = -1;		// Memory access time
bool gWriteAllocate 		= false;  	// Write-Allocate mode

/******************************************************************************
 * Declarations
 *****************************************************************************/

/******************************************************************************
 * Helper Functions
 *****************************************************************************/
inline unsigned int log2_u(unsigned int x) {
	unsigned int log = 0;
	while (x > 1) {
		x >>= 1;
		log++;
	}
	return log;
}

uint32_t dec_to_bin(uint32_t dec) {
    return dec; // Just return the value, as bitwise operations work on the integer itself.
}

inline uint32_t get_bits(uint32_t v, unsigned lo, unsigned hi) {
	if (lo > hi || lo < 0 || hi >= 32) {
		// cerr << "Error: get_bits received an invalid bit range" << endl;
		return 0;
	}
    uint32_t mask = ((uint64_t)1 << (hi - lo + 1)) - 1;
    return (v >> lo) & mask;
}

int parse_address(uint32_t address, unsigned int blockSize, unsigned int numSets,
				 uint32_t &tag, uint32_t &setIndex, uint32_t &blockOffset) {
	/** Address Structure:
	 * [<log2(blockSize)-1> .. 0]									: Block offset
	 * [<log2(blockSize)+log2(numSets)-1 .. <log2(blockSize)>]		: Set index
	 * [31 .. <log2(blockSize)+log2(numSets)-1>]					: Tag
	 */
	unsigned leftIdxOffset = log2_u(blockSize) - 1;
	unsigned leftIdxSets = leftIdxOffset + log2_u(numSets);
	unsigned leftIdxTag = 31;
	
	// Handling edge cases
	if (numSets == 0) {
		// No sets
		cerr << "Error: Number of sets cannot be 0" << endl;
		return 1;
	} else if (numSets == 1) {
		// Only one set, so all bits after the block offset are the tag
		blockOffset = get_bits(address, 0, leftIdxOffset);
		setIndex = 0;
		tag = get_bits(address, leftIdxOffset + 1, leftIdxTag);
		return 0;
	}
	
	blockOffset = get_bits(address, 0, leftIdxOffset);
	setIndex = get_bits(address, leftIdxOffset + 1, leftIdxSets);
	tag = get_bits(address, leftIdxSets + 1, leftIdxTag);

	return 0;
}

/******************************************************************************
 * Classes & Structs
 *****************************************************************************/
// ~ ~ ~ Block struct ~ ~ ~
struct Block {
    uint32_t tag = 0;
	uint32_t address = 0;
    bool valid = false;
    bool dirty = false;
    unsigned long timeStamp = 0;
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
	unsigned int getNumBlocks() const { return numBlocks; }
	
};

// ~ ~ ~ Cache class ~ ~ ~
class Cache {
private:
	// Cache parameters
	unsigned int size;
	unsigned int blockSize;
	unsigned int numBlocks;
	unsigned int numWays;
	unsigned int numSets;
	unsigned int accCount;
	unsigned int missCount;
	CacheWay **pWayArr;

public:
	Cache(unsigned int size, unsigned int blockSize, unsigned int numWays) : 
			size(size), blockSize(blockSize), numWays(numWays), accCount(0), missCount(0) {
		// Initialize parameters
		numBlocks = size / blockSize;
		numSets   = numBlocks / numWays;

		pWayArr = new CacheWay*[numWays];
		for (unsigned int i = 0; i < numWays; ++i) {
			pWayArr[i] = new CacheWay(numSets);
		}
	}

	~Cache() {
		for (unsigned int i = 0; i < numWays; ++i) {
			delete pWayArr[i];
		}
		delete[] pWayArr;
	}
	
	// Getters & Setters
	unsigned int getMissCount() const { return missCount; }
	unsigned int getAccCount() const { return accCount; }

	unsigned int getNumSets() const { return numSets; }
	unsigned int getNumWays() const { return numWays; }
	CacheWay* getWay(int index) const { return pWayArr[index]; }

	// Target functions
	bool access(uint32_t address, char operation, unsigned long int curTime) {
		accCount++;

		// Parsing the address
		uint32_t blockOffset, setIndex, tag;
		parse_address(address, blockSize, numSets, tag, setIndex, blockOffset);
		// cout << "Access: Tag: " << std::bitset<32>(tag) << ", Set Index: " << std::bitset<32>(setIndex) << endl; // WHENDONE: Remove.

		// Searching for the block in the cache
		for (unsigned int i = 0; i < numWays; i++) {
			Block &matchingBlock = pWayArr[i]->getBlock(setIndex);
			if (matchingBlock.valid && matchingBlock.tag == tag) {
				// Cache Hit
				// cout << "Access: Cache hit in way " << i << endl; // WHENDONE: Remove.
				matchingBlock.timeStamp = curTime;  // "Touch" block for LRU policy
				if (operation == 'w') {
					// If write operation, set dirty bit
					matchingBlock.dirty = true;
				}
				return true;
			}
		}

		// Cache Miss
		missCount++;

		return false;
	}

	uint32_t fetch_block_into_cache(uint32_t address, unsigned long int curTime, bool dirty, bool* wasDirty = nullptr) {
		// Parsing the address
		uint32_t blockOffset, setIndex, tag;
		parse_address(address, blockSize, numSets, tag, setIndex, blockOffset);
		
		// Looking for a vacant block in ways
		for (unsigned int i = 0; i < numWays; i++) {
			Block &matchingBlock = pWayArr[i]->getBlock(setIndex);
			if (!matchingBlock.valid) {
				// cout << "Fetch: Placing block in way " << i << endl; // WHENDONE: Remove.
				// If block is not valid, set it as valid and set the tag
				matchingBlock.valid = true;
				matchingBlock.tag = tag;
				matchingBlock.address = address;
				matchingBlock.dirty = dirty;
				matchingBlock.timeStamp = curTime;  // "Touch" block for LRU policy
				return (uint32_t)-1;  // No eviction
			}
		}
		
		// Conflict miss - need to evict a block
		// Finding the oldest block in the set as per LRU policy
		Block *LRUBlock = &pWayArr[0]->getBlock(setIndex);
		// unsigned int temp = 0;
		for (unsigned int i = 1; i < numWays; i++) {
			Block &matchingBlock = pWayArr[i]->getBlock(setIndex);
			if (matchingBlock.timeStamp < LRUBlock->timeStamp) {
				LRUBlock = &matchingBlock;
				// temp = i;
			}
		}
		// cout << "Fetch: Evicting block according to LRU policy from way " << temp << ", and placing new block" << endl; // WHENDONE: Remove.
		
		// Remember the evicted address
		uint32_t evictedAddress = LRUBlock->address;

		// Evicting the oldest block
		if ((*wasDirty = LRUBlock->dirty)) {
			// If the block is dirty, write it back to memory
			// TODO: Should remain empty?
		}

		// Updating the block with the new data
		LRUBlock->tag = tag;
		LRUBlock->address = address;
		LRUBlock->timeStamp = curTime;  // "Touch" block for LRU policy
		LRUBlock->valid = true;
		LRUBlock->dirty = dirty;
		return evictedAddress;  // Return the evicted address
	}

	Block* get_block(uint32_t address, unsigned long int curTime) const {
		// Parsing the address
		uint32_t blockOffset, setIndex, tag;
		parse_address(address, blockSize, numSets, tag, setIndex, blockOffset);

		// Searching for the block in the cache
		for (unsigned int i = 0; i < numWays; i++) {
			Block &matchingBlock = pWayArr[i]->getBlock(setIndex);
			if (matchingBlock.valid && matchingBlock.tag == tag) {
				// Cache Hit
				matchingBlock.timeStamp = curTime;  // "Touch" block for LRU policy
				return &matchingBlock;
			}
		}

		return nullptr;
	}
};

void print_cache_content(Cache *cache) {
	/**
	 * Divide each cache to ways presented as numbered columns, and each way will hold all the set blocks as numbered rows.
	 * Inside each cell representing block write its tag and in parentheses the step in which it was last "touched" for LRU policy.
	 */
	cout << "Cache content:" << endl;
	for (unsigned int i = 0; i < cache->getNumSets(); i++) {
		cout << "Set " << i << ": ";
		for (unsigned int j = 0; j < cache->getNumWays(); j++) {
			Block &block = cache->getWay(j)->getBlock(i);
			if (block.valid) {
				cout << "[" << block.tag << " (" << block.timeStamp << ")] ";
			} else {
				cout << "[Empty] ";
			}
		}
		cout << endl;
	}
}

// ~ ~ ~ TwoLevelCache class ~ ~ ~
class TwoLevelCache {
private:
	unsigned long int accumulatedTime;
	unsigned long int totalAccesses;
	unsigned int L1AccessTime;
	unsigned int L2AccessTime;
	Cache *L1;
	Cache *L2;

public:
	TwoLevelCache(unsigned int blockSize,
				  unsigned int L1Size, unsigned int L1NumWays, unsigned int L1AccessTime,
				  unsigned int L2Size, unsigned int L2NumWays, unsigned int L2AccessTime) :
				  accumulatedTime(0), totalAccesses(0), L1AccessTime(L1AccessTime), L2AccessTime(L2AccessTime) {
		// Create L1 and L2 caches
		L1 = new Cache(L1Size, blockSize, L1NumWays);
		L2 = new Cache(L2Size, blockSize, L2NumWays);
	}

	~TwoLevelCache() {
		delete L1;
		delete L2;
	}

	// Getters
	unsigned long int getAccCount() { return totalAccesses; }
	unsigned long int getAccTime() { return accumulatedTime; }
	Cache* getL1() { return L1; }
	Cache* getL2() { return L2; }

	// Target functions
	void access(uint32_t address, char operation) {
		this->totalAccesses++;
		// cout << " L1 content: " << endl; // WHENDONE: Remove.
		// print_cache_content(L1); // WHENDONE: Remove.
		// cout << " L2 content: " << endl; // WHENDONE: Remove.
		// print_cache_content(L2); // WHENDONE: Remove.
		// cout << "\n>>> " << totalAccesses << " <<<" << endl; // WHENDONE: Remove.
		// cout << "Accessing address: " << std::bitset<32>(address) << ", Operation: " << operation << ", Current time: " << this->accumulatedTime << endl; // WHENDONE: Remove.

		// Access L1 cache
		this->accumulatedTime += L1AccessTime;
		if (L1->access(address, operation, this->accumulatedTime)) {
			// cout << "L1 Hit" << endl; // WHENDONE: Remove.
			return; // If hit, return
		}
		// cout << "L1 Miss" << endl; // WHENDONE: Remove.

		// If miss, access L2 cache
		this->accumulatedTime += L2AccessTime;
		if (L2->access(address, operation, this->accumulatedTime)) {
			// cout << "L2 Hit , fetching into L1" << endl; // WHENDONE: Remove.
			// If hit, check if the block should be fetched into L1
			if (operation == 'r' || gWriteAllocate) {
				// Fetch the block into L1
				bool wasDirty = false;
                uint32_t evictedAddress = L1->fetch_block_into_cache(address, accumulatedTime, operation=='w', &wasDirty);
				// If a block was evicted from L1 and it was dirty, write it back to L2
				if (evictedAddress != (uint32_t)-1 && wasDirty) {
					L2->get_block(evictedAddress, this->accumulatedTime)->dirty = true;  // Updating the block in L2
				}
			}
			return;
		}

		// If miss, access memory
		this->accumulatedTime += gMemAccessTime;
		// cout << "L2 Miss, performing memory access..." << endl; // WHENDONE: Remove.
		if (operation == 'r' || gWriteAllocate) {
			Block *block;
			
			// Fetch the block into LLC
			bool wasDirty = false;
			// cout << "Fetching block into L2" << endl; // WHENDONE: Remove.
			uint32_t evictedAddress = L2->fetch_block_into_cache(address, this->accumulatedTime, operation=='w', &wasDirty);

			// If a block was evicted from L2, invalidate it in L1
			if (evictedAddress != (uint32_t)-1) {
				block = L1->get_block(evictedAddress, this->accumulatedTime);
				if (block != nullptr) {
					// cout << "Invalidating block in L1 due to eviction from L2" << endl; // WHENDONE: Remove.
					block->valid = false;
				}
			}

			// Fetch the block into L1
			wasDirty = false;
			// cout << "Fetching block into L1" << endl; // WHENDONE: Remove.
			evictedAddress = L1->fetch_block_into_cache(address, this->accumulatedTime, operation=='w', &wasDirty);
			
			// If a block was evicted from L1 and it was dirty, write it back to L2
			if (evictedAddress != (uint32_t)-1 && wasDirty) {
				L2->get_block(evictedAddress, this->accumulatedTime)->dirty = true;  // Updating the block in L2
			}
		}

	}
};

/******************************************************************************
 * Main Function
 *****************************************************************************/
int main(int argc, char **argv) {
	// Check if enough arguments are provided
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
	unsigned blockSize = 1u << BSize;
    unsigned L1S = 1u << L1Size;
	unsigned L1numWays = 1u << L1Assoc;
    unsigned L2S = 1u << L2Size;
	unsigned L2numWays = 1u << L2Assoc;

    TwoLevelCache cache(blockSize, L1S, L1numWays, L1Cyc, L2S, L2numWays, L2Cyc);

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

		// Parsing the operation
		string cutAddress = address.substr(2); // Removing the "0x" part of the address
		unsigned long int num = 0;
		num = strtoul(cutAddress.c_str(), NULL, 16);
		uint32_t binAddress = dec_to_bin(num);

		// Accessing the cache
		cache.access(binAddress, operation);
	}
	// Round to 3 decimal places with custom rounding logic
	double L1MissRate = (double)cache.getL1()->getMissCount() / (double)cache.getL1()->getAccCount();
	double L2MissRate = (double)cache.getL2()->getMissCount() / (double)cache.getL2()->getAccCount();
	double avgAccTime = (double)cache.getAccTime() / (double)cache.getAccCount();

	// // Custom rounding function: round up if 4th digit > 5, down if <= 5
	// auto customRound = [](double value) -> double {
	// 	double shifted = value * 1000.0;  // Shift 3 decimal places
	// 	double fractional = shifted - (int)shifted;
	// 	if (fractional > 0.5) {
	// 		return (int)(shifted + 1) / 1000.0;
	// 	} else {
	// 		return (int)shifted / 1000.0;
	// 	}
	// };

	// L1MissRate = customRound(L1MissRate);
	// L2MissRate = customRound(L2MissRate);
	// avgAccTime = customRound(avgAccTime);

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);

	return 0;
}

/******************************************************************************
 * Test Functions
 *****************************************************************************/
void test_parseAddress() {
	uint32_t address = 0x12345678;
	unsigned int blockSize = 16;
	unsigned int numSets = 4;
	uint32_t tag, setIndex, blockOffset;
	
	cout << "Testing parse_address function..." << endl;
	cout << "\tAddress: " << std::bitset<32>(address) << endl;
	cout << "\tBlock Size: " << blockSize << endl;
	cout << "\tNumber of Sets: " << numSets << endl;
	parse_address(address, blockSize, numSets, tag, setIndex, blockOffset);
	cout << "Parsed Address:" << endl;
	cout << "\tTag: " << std::bitset<32>(tag) << endl;
	cout << "\tSet Index: " << std::bitset<32>(setIndex) << endl;
	cout << "\tBlock Offset: " << std::bitset<32>(blockOffset) << endl;
}

void test_dec_to_bin() {
	unsigned int dec = 100;
	cout << "Decimal: " << dec << " to Binary: " << std::bitset<32>(dec) << endl;
	dec = 255;
	cout << "Decimal: " << dec << " to Binary: " << std::bitset<32>(dec) << endl;
	dec = 1024;
	cout << "Decimal: " << dec << " to Binary: " << std::bitset<32>(dec) << endl;	
}

/******************************************************************************
 * Temporary Main Function for Testing
 *****************************************************************************/
int _main() {
	// test_parseAddress();
	// test_dec_to_bin();
	return 0;
}

/* Output:

*/
