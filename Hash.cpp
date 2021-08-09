#include <iostream>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

void printUint(uint64_t in, int bits) {
	for(int i = bits-1; i >= 0; i--) {
		std::cout << (in >> i & 1);
	}
	std::cout << std::endl;
}

uint32_t SHR(uint32_t in, uint32_t amount) {
	return in >> amount;
}

uint32_t ROTR(uint32_t in, uint32_t amount) {
	return (in >> amount)|(in << (32 - amount));
}

uint32_t CH(uint32_t x, uint32_t y, uint32_t z) {
	return (x & y) ^ ((~x) & z);
}

uint32_t MAJ(uint32_t x, uint32_t y, uint32_t z) {
	return (x & y) ^ (x & z) ^ (y & z);
}

uint32_t s0(uint32_t in) {
	return ROTR(in, 7) ^ ROTR(in, 18) ^ SHR(in, 3);
}

uint32_t s1(uint32_t in) {
	return ROTR(in, 17) ^ ROTR(in, 19) ^ SHR(in, 10);
}

uint32_t S0(uint32_t in) {
	return ROTR(in, 2) ^ ROTR(in, 13) ^ ROTR(in, 22);
}

uint32_t S1(uint32_t in) {
	return ROTR(in, 6) ^ ROTR(in, 11) ^ ROTR(in, 25);
}

// Padding needs to take some message and badd it out to 448 bits then add the length of the message to the end in a uint64
// Padding also needs to add a one after the message 

// Message block 512 bits

// Message Schedule 32 bit blocks
	// Expand message Schedule to 64 words
	// Wx = s1(x-2) + x-7 + s0(x-15) + x-16

// Compression
	// K[0..63] :=
   // 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
   // 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
   // 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   // 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
   // 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   // 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   // 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
   // 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	// a := 0x6a09e667
	// b := 0xbb67ae85
	// c := 0x3c6ef372
	// d := 0xa54ff53a
	// e := 0x510e527f
	// f := 0x9b05688c
	// g := 0x1f83d9ab
	// h := 0x5be0cd19
	
	// T1 := S1(e) + CH(e, f, g,) + h + K0 + Wo
	// T2 := S0(a) + MAJ(a, b, c)

	// a = T1 + T2
	// e += T1
	// For each word, shift everythin down one, a->b, b->c...
	// Add the initial constants to the end
	// If there are more message blocks, use these as the initial constants
	// Final hash is the concatenation of all these outputs.
struct Block {
	uint32_t part[16];
};

struct MessageBlockList {
	int blockCount;
	Block * blocks; 
};

void printMessageBlockList(MessageBlockList in) {
	for(int j = 0; j < 16; j++) {
		printUint(in.blocks[0].part[j], 32);
	}
}

// Ideally the input should be passed in as a uint64_t to prevent weird memory storages
// from messing up the hash. It technically doesn't matter though because as long as you
// use this as the hasher, it will never change.
MessageBlockList convertToMessageBlocks(void * input, int size, uint64_t items) {
	uint64_t bits = size*items*8;
	int messageBlocks = ((bits + 64) + 511 + 8)/512;

	Block * blocks = (Block *) calloc(messageBlocks, sizeof(Block));
	MessageBlockList output = {messageBlocks, blocks};

	memcpy(&output.blocks[0].part[0], input, size * items);
	for(int i = 0; i < messageBlocks; i++) {
		for(int j = 0; j < 16; j++) {
			output.blocks[i].part[j] = __builtin_bswap32(output.blocks[i].part[j]);
		}
	}

	int blockNumber = size * items / 512;
	int partNumber = (bits - 512 * blockNumber)/32;
	int index = bits - 512 * blockNumber - partNumber * 32;
	output.blocks[blockNumber].part[partNumber] |= (((uint64_t) 1) << (31 - index));

	output.blocks[messageBlocks-1].part[15] = bits;
	output.blocks[messageBlocks-1].part[14] = bits >> 32;

	return output;
}

struct MessageSchedule {
	uint32_t part[64];
};

void printMessageSchedule(MessageSchedule schedule) {
	for(int i = 0 ; i < 16; i++) {
		std::cout << i << ": ";
		printUint(schedule.part[i], 32);
	}
}

MessageSchedule fillMessageSchedule(Block input) {
	MessageSchedule output;
	memcpy(output.part, input.part, sizeof(uint32_t) * 16);
	// Wx = s1(x-2) + x-7 + s0(x-15) + x-16
	for(int i = 16; i < 64; i++) {
		output.part[i] = s1(output.part[i-2]) + output.part[i-7] + s0(output.part[i-15]) + output.part[i-16];
	}
	return output;
}



uint32_t K[64] = {
   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
   0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
   0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
   0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
   0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

// T1 := S1(e) + CH(e, f, g,) + h + KX + WX
// T2 := S0(a) + MAJ(a, b, c)

// a = T1 + T2
// e += T1
// For each word, shift everythin down one, a->b, b->c...
// Add the initial constants to the end
// If there are more message blocks, use these as the initial constants
// Final hash is the concatenation of all these outputs.
void printConstants(uint32_t * constants) {
	for(int i = 0; i < 8; i++) {
		std::cout << i << ": ";
		printUint(constants[i], 32);
	}
}
void doCompression(uint32_t * constants, MessageSchedule schedule) {
	// a = 0
	// b = 1
	// c = 2
	// d = 3
	// e = 4
	// f = 5
	// g = 6
	// h = 7
	uint32_t T1;
	uint32_t T2;
	uint32_t initials[8];
	for(int i = 0; i < 8; i++) {
		initials[i] = constants[i];
	}
	for(int i = 0; i < 64; i++) {
		T1 = S1(constants[4]) + CH(constants[4], constants[5], constants[6]) + constants[7] + schedule.part[i] + K[i];
		T2 = S0(constants[0]) + MAJ(constants[0], constants[1], constants[2]);
		for(int j = 7; j >= 1; j--) {
			constants[j] = constants[j-1];
		}
		constants[0] = T1 + T2;
		constants[4] += T1;
	}
	for(int i = 0; i < 8; i++) {
		constants[i] += initials[i];
	}
}

uint32_t * doHash(uint32_t compressions[8], void * input, int size, int items) {
	MessageBlockList messageBlocks = convertToMessageBlocks(input, size, items);
	MessageSchedule schedule;
	for(int i = 0; i < messageBlocks.blockCount; i++) {
		schedule = fillMessageSchedule(messageBlocks.blocks[i]);
		doCompression(compressions, schedule);
	}
	return compressions;
}

int main() {
	uint32_t output[8] = {
		0x6a09e667,
		0xbb67ae85,
		0x3c6ef372,
		0xa54ff53a,
		0x510e527f,
		0x9b05688c,
		0x1f83d9ab,
		0x5be0cd19
	};
	std::string input;
	std::cout << "Input your string: ";
	std::cin >> input;
	char * test = (char *) input.c_str();
	int chars = input.length();
	doHash(output, test, sizeof(char), chars);
	for(int i = 0; i < 8; i++) {
		std::cout << std::hex << output[i];
	}
	std::cout << std::endl;
}