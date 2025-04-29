#include <iostream>
#include <bitset>
#include <thread>
#include <chrono>
#include <iomanip> 
#include <fstream>
 
uint8_t reg_A, reg_B, reg_C, reg_D, reg_E, reg_H, reg_L; // ACCUMULATOR, GENERAL REGISTERS (8 bits)
uint16_t reg_SP, reg_PC; // STACK POINTER, PROGRAM COUNTER (16 bits)
uint16_t reg_RET; // INTERNAL
uint8_t reg_FLAGS; // INTERNAL

uint8_t memory[0xFFFF] = {0x00};

bool HALT = false;
 
// DDD = Destination, SSS = Source
enum RegisterRefs {
    A = 0b111,
    B = 0b000,
    C = 0b001,
    D = 0b010,
    E = 0b011,
    H = 0b100,
    L = 0b101,
	FLAGS = 0b1000
};
 
// RP = Register Pair
enum RegisterPairsRefs {
    BC = 0b00,
    DE = 0b01,
    HL = 0b10,
    SP = 0b11,
    PC = 0b100,
	PSW = 0b101
};
 
void setRegister(RegisterRefs reg, uint8_t d8) {
 
    switch(reg) {
        case RegisterRefs::A:
            reg_A = d8;
            break;
        case RegisterRefs::B:
            reg_B = d8;
            break;
        case RegisterRefs::C:
            reg_C = d8;
            break;
        case RegisterRefs::D:
            reg_D = d8;
            break;
        case RegisterRefs::E:
            reg_E = d8;
            break;
        case RegisterRefs::H:
            reg_H = d8;
            break;
        case RegisterRefs::L:
            reg_L = d8;
            break;
    }
}
void setRegisterPair(RegisterPairsRefs reg, uint16_t d16) {
    switch(reg) {
        case RegisterPairsRefs::BC:
            reg_B = (d16 >> 8) & 0xFF;
            reg_C = d16 & 0xFF;
            break;
        case RegisterPairsRefs::DE:
            reg_D = (d16 >> 8) & 0xFF;
            reg_E = d16 & 0xFF;
            break;
        case RegisterPairsRefs::HL:
            reg_H = (d16 >> 8) & 0xFF;
            reg_L = d16 & 0xFF;
            break;
        case RegisterPairsRefs::SP:
            reg_SP = d16;
            break;
		case RegisterPairsRefs::PSW:
			reg_A = (d16 >> 8) & 0xFF;
			reg_FLAGS = d16 & 0xFF;
        default:
            reg_PC = d16;
            break;
    }
}
void setRegisterPair(RegisterPairsRefs reg, uint8_t d8h, uint8_t d8l) {
    switch(reg) {
        case RegisterPairsRefs::BC:
            reg_B = d8h;
            reg_C = d8l;
            break;
        case RegisterPairsRefs::DE:
            reg_D = d8h;
            reg_E = d8l;
            break;
        case RegisterPairsRefs::HL:
            reg_H = d8h;
            reg_L = d8l;
            break;
        case RegisterPairsRefs::SP:
            reg_SP = (static_cast<uint16_t>(d8h) << 8) | d8l;
            break;
        case RegisterPairsRefs::PC:
            reg_PC = (static_cast<uint16_t>(d8h) << 8) | d8l;
            break;
        default:
            break;
    }
}
 
uint8_t getRegister(RegisterRefs reg) {
    switch(reg){
        case RegisterRefs::A:
            return reg_A;
            break;
        case RegisterRefs::B:
            return reg_B;
            break;
        case RegisterRefs::C:
            return reg_C;
            break;
        case RegisterRefs::D:
            return reg_D;
            break;
        case RegisterRefs::E:
            return reg_E;
            break;
        case RegisterRefs::H:
            return reg_H;
            break;
        case RegisterRefs::L:
            return reg_L;
            break;
		case RegisterRefs::FLAGS:
			return reg_FLAGS;
        default:
            return 0;
            break;
    } 
    return 0;
} 
uint16_t getRegister(RegisterPairsRefs reg) {
    switch(reg) {
        case RegisterPairsRefs::BC:
            return (static_cast<uint16_t>(reg_B) << 8) | reg_C;
            break;
        case RegisterPairsRefs::DE:
            return (static_cast<uint16_t>(reg_D) << 8) | reg_E;
            break;
        case RegisterPairsRefs::HL:
            return (static_cast<uint16_t>(reg_H) << 8) | reg_L;
            break;
        case RegisterPairsRefs::SP:
            return reg_SP;
            break;
        case RegisterPairsRefs::PC:
            return reg_PC;
            break;
		case RegisterPairsRefs::PSW:
			return (static_cast<uint16_t>(reg_A) << 8) | reg_FLAGS;
			break;
        default:
            return 0;
            break;
    } 
    return 0;
} 

bool flag_Z, flag_S, flag_P, flag_CY, flag_AC; // ZERO, SIGN, PARITY, CARRY, AUX-CARRY
 
void setBit(uint8_t& byte, uint8_t bit, bool value) {
    if (value) {
        byte |= (1 << bit);     // Met le bit à 1
    } else {
        byte &= ~(1 << bit);    // Met le bit à 0
    }
}
enum FlagType {
    FLAG_Z  = 1 << 0,  // Zero
    FLAG_S  = 1 << 1,  // Sign
    FLAG_P  = 1 << 2,  // Parity
    FLAG_AC = 1 << 3,  // Auxiliary Carry (calcul basé sur nibble inférieur)
    FLAG_CY = 1 << 4   // Carry (valeur > 255 pour 8 bits)
};

void checkFlags(uint8_t value, uint8_t previous, uint8_t flagsToCheck) {
	if (flagsToCheck & FLAG_Z)
		flag_Z = (value == 0);

	if (flagsToCheck & FLAG_S)
		flag_S = (value & 0x80) != 0;

	if (flagsToCheck & FLAG_P) {
		uint8_t count = 0;
		for (uint8_t i = 0; i < 8; ++i)
			if (value & (1 << i)) ++count;
				flag_P = (count % 2 == 0);  // even parity
	}

	if (flagsToCheck & FLAG_AC)
		flag_AC = ((previous & 0x0F) + (value & 0x0F)) > 0x0F;

	if (flagsToCheck & FLAG_CY){
		if(previous == 0xFF){
			if(value > 0xFF || value == 0){
				flag_CY = true;
			} else {
				flag_CY = false;
			} 
		} else {
			flag_CY = false;
		} 
	} 
		
}
void setFlagReg(){
	setBit(reg_FLAGS, 0, flag_CY);
	setBit(reg_FLAGS, 1, 0);
	setBit(reg_FLAGS, 2, flag_P);
	setBit(reg_FLAGS, 3, 0);
	setBit(reg_FLAGS, 4, flag_AC);
	setBit(reg_FLAGS, 5, 0);
	setBit(reg_FLAGS, 6, flag_Z);
	setBit(reg_FLAGS, 7, flag_S);
} 

enum OpCodes {
	// 0x
	NOP 		= 	0x00,		//	NOP									->	No operation
	LXI_B_D16	=	0x01,		//	LXI 	B, 0bXXXXXXXXXXXXXXXX		->	Load register pair BC with 16-bit immediate value
	STAX_B		=	0x02,		//	STAX 	BC							->	Store register A in memory address in register pair BC
	INX_B		=	0x03,		//	INX 	BC							->	Increment register pair BC
	INR_B		=	0x04,		//	INR		B							->	Increment register B
	DCR_B		=	0x05,		//	DCR 	B 							->	Decrement register B
	MVI_B_D8	=	0x06,		//	MVI		B, 0bXXXXXXXX				->	Load register B with 8-bit immediate value
	RLC			=	0x07,		//	RLC									-> 	Rotate register A left (Circular)
	//			=	0x08,		// --- Unused
	DAD_B		=	0x09,		//	DAD		BC							-> 	Add register pair BC to register pair HL and store in register pair HL
	LDAX_B		=	0x0A,		//	LDAX	BC							->	Load register A from memory address in register pair BC
	DCX_B		=	0x0B,		//	DCX		BC							->	Decrement register pair BC
	INR_C		=	0x0C,		//	INR		C							->	Increment register C
	DCR_C		=	0x0D,		//	DCR		C							->	Decrement register C
	MVI_C_D8	=	0x0E,		//	MVI		C, 0bXXXXXXXX				->	Load register C with 8-bit immediate value
	RRC			=	0x0F,		//	RRC									->	Rotate register A right (Circular)
 
	// 1x
	//			=	0x10,
	LXI_D_D16	=	0x11,		//	LXI		D, 0bXXXXXXXXXXXXXXXX		->	Load register pair DE with 16-bit immediate value
	STAX_D		=	0x12,		//	STAX	DE							->	Store register A in memory address in register pair DE
	INX_D		=	0x13,		//	INX		DE							->	Increment register pair DE
	INR_D		=	0x14,		//	INR		D							->	Increment register D
	DCR_D		=	0x15,		//	DCR		D							->	Decrement register D
	MVI_D_D8	=	0x16,		//	MVI		D, 0bXXXXXXXX				->	Load register D with 8-bit immediate value
	RAL			=	0x17,		//	RAL									->	Rotate register A left trought carry
	//			=	0x18,		// --- Unused
	DAD_D		=	0x19,		//	DAD		DE							->	Add register pair DE to register pair HL and store in register pair HL
	LDAX_D		=	0x1A,		//	LDAX	DE							->	Load register A from memory address in register pair DE
	DCX_D		=	0x1B,		//	DCX		DE							->	Decrement register pair DE
	INR_E		= 	0x1C,		// 	INR		E							-> 	Increment register E
	DCR_E		= 	0x1D,		// 	DCR		E							-> 	Decrement register E
	MVI_E_D8	= 	0x1E,		// 	MVI		E, 0bXXXXXXXX				-> 	Load register E with 8-bit immediate value
	RAR			= 	0x1F,		// 	RAR									-> 	Rotate register A right through carry
 
	// 2x
	RIM			= 	0x20,		// 	RIM									-> 	Read interrupt mask and serial data into register A
	LXI_H_D16	= 	0x21,		// 	LXI		H, 0bXXXXXXXXXXXXXXXX		-> 	Load register pair HL with 16-bit immediate value
	SHLD_A16	= 	0x22,		// 	SHLD	0bXXXXXXXXXXXXXXXX			-> 	Store register pair HL at immediate address
	INX_H		= 	0x23,		// 	INX		HL							-> 	Increment register pair HL
	INR_H		= 	0x24,		// 	INR		H							-> 	Increment register H
	DCR_H		= 	0x25,		// 	DCR		H							-> 	Decrement register H
	MVI_H_D8	= 	0x26,		// 	MVI		H, 0bXXXXXXXX				-> 	Load register H with 8-bit immediate value 
	DAA			= 	0x27,		// 	DAA									-> 	Decimal adjust accumulator
	//			=	0x28,		// --- Unused
	DAD_H		= 	0x29,		// 	DAD		HL							-> 	Add register pair HL to register pair HL and store in register pair HL
	LHLD_A16	= 	0x2A,		// 	LHLD	0bXXXXXXXX					-> 	Load register pair HL from immediate address
	DCX_H		= 	0x2B,		// 	DCX		HL							-> 	Decrement register pair HL
	INR_L		= 	0x2C,		// 	INR		L							-> 	Increment register L
	DCR_L		= 	0x2D,		// 	DCR		L							-> 	Decrement register L
	MVI_L_D8	= 	0x2E,		// 	MVI		L, 0bXXXXXXXXXXXXXXXX		-> 	Load register L with 8-bit immediate value
	CMA			= 	0x2F,		// 	CMA									-> 	Complement register A
 
	// 3x
	SIM			= 	0x30,		// 	SIM									->	Set interrupt mask and serial data from register A
	LXI_SP_D16	= 	0x31,		// 	LXI		SP, 0bXXXXXXXXXXXXXXXX		-> 	Load register SP from immediate address
	STA_A16		= 	0x32,		// 	STA		0bXXXXXXXXXXXXXXXX			-> 	Store register A at immediate address
	INX_SP		= 	0x33,		// 	INX		SP							-> 	Increment register pair SP
	INR_M		= 	0x34,		// 	INR		M							-> 	Increment memory location pointed to by register pair HL
	DCR_M		= 	0x35,		// 	DCR		M							-> 	Decrement memory location pointed to by register pair HL
	MVI_M_D8	= 	0x36,		// 	MVI		M, 0bXXXXXXXX				-> 	Load memory location pointed to by register pair HL with 8-bit immediate
	STC			= 	0x37,		// 	STC									->	Set carry flag
	//			= 	0x38,		// --- Unused
	DAD_SP		=	0x39,		//	DAD		SP							->	Add register pair SP to register pair HL and store in register pair HL
	LDA_A16		= 	0x3A,		// 	LDA		0bXXXXXXXXXXXXXXXX			-> 	Load register A from immediate address
	DCX_SP		= 	0x3B,		// 	DCX		SP							-> 	Decrement register pair SP
	INR_A		= 	0x3C,		// 	INR		A							-> 	Increment register A
	DCR_A		= 	0x3D,		// 	DCR		A							-> 	Decrement register A
	MVI_A_D8	= 	0x3E,		// 	MVI		A, 0bXXXXXXXX				-> 	Load register A with 8-bit immediate value
	CMC			= 	0x3F,		// 	CMC									-> 	Complement carry flag
 
	// 4x
	MOV_B_B		= 	0x40, 		//	MOV		B, B						->	Move value from register B to register B
	MOV_B_C 	= 	0x41, 		//	MOV		B, C						->	Move value from register C to register B 
	MOV_B_D 	= 	0x42,  		//	MOV		B, D						->	Move value from register D to register B
	MOV_B_E 	= 	0x43, 		//	MOV		B, E						->	Move value from register E to register B
	MOV_B_H		= 	0x44, 		//	MOV		B, H						->	Move value from register H to register B
	MOV_B_L 	= 	0x45,  		//	MOV		B, L						->	Move value from register L to register B
	MOV_B_M 	= 	0x46,  		//	MOV		B, M						->	Move value from memory address in register pair HL to register B
	MOV_B_A 	= 	0x47, 		//	MOV		B, A						->	Move value from register A to register B
	MOV_C_B		= 	0x48,  		//	MOV		C, B						->	Move value from register B to register C
	MOV_C_C 	= 	0x49,  		//	MOV		C, C						->	Move value from register C to register C
	MOV_C_D 	=	0x4A,  		//	MOV		C, D						->	Move value from register D to register C
	MOV_C_E 	= 	0x4B,  		//	MOV		C, E						->	Move value from register E to register C
	MOV_C_H		= 	0x4C,  		//	MOV		C, H						->	Move value from register H to register C 
	MOV_C_L 	= 	0x4D,  		//	MOV		C, L						->	Move value from register L to register C 
	MOV_C_M 	= 	0x4E,  		//	MOV		C, M						->	Move value frommemory address in register pair HL to register C 
	MOV_C_A 	= 	0x4F,  		//	MOV		C, A						->	Move value from register A to register C
 
	//	5x
	MOV_D_B		= 	0x50,  		//	MOV		D, B						->	Move value from register B to register D
	MOV_D_C 	= 	0x51,  		//	MOV		D, C						->	Move value from register C to register D 
	MOV_D_D 	= 	0x52,  		//	MOV		D, D						->	Move value from register D to register D
	MOV_D_E 	= 	0x53,  		//	MOV		D, E						->	Move value from register E to register D
	MOV_D_H		= 	0x54,  		//	MOV		D, H						->	Move value from register H to register D
	MOV_D_L 	= 	0x55,  		//	MOV		D, L						->	Move value from register L to register D
	MOV_D_M 	= 	0x56,  		//	MOV		D, M						->	Move value from memory address in register pair HL to register D
	MOV_D_A 	= 	0x57,  		//	MOV		D, A						->	Move value from register A to register D
	MOV_E_B		= 	0x58,  		//	MOV		E, B						->	Move value from register B to register E
	MOV_E_C 	= 	0x59,  		//	MOV		E, C						->	Move value from register C to register E
	MOV_E_D 	= 	0x5A,  		//	MOV		E, D						->	Move value from register D to register E
	MOV_E_E 	= 	0x5B,  		//	MOV		E, E						->	Move value from register E to register E
	MOV_E_H		= 	0x5C,  		//	MOV		E, H						->	Move value from register H to register E
	MOV_E_L 	= 	0x5D,  		//	MOV		E, L						->	Move value from register L to register E
	MOV_E_M 	= 	0x5E,  		//	MOV		E, M						->	Move value from memory address in register pair HL to register E
	MOV_E_A 	= 	0x5F,  		//	MOV		E, A						->	Move value from register A to register E
 
	//	6x
	MOV_H_B		= 	0x60,		//	MOV		H, B						-> 	Move value from register B to register H
	MOV_H_C 	= 	0x61,		//	MOV		H, C						-> 	Move value from register C to register H
	MOV_H_D 	= 	0x62,		//	MOV		H, D						-> 	Move value from register D to register H
	MOV_H_E 	= 	0x63,		//	MOV		H, E						-> 	Move value from register E to register H
	MOV_H_H		= 	0x64,		//	MOV		H, H						-> 	Move value from register H to register H
	MOV_H_L 	= 	0x65,		//	MOV		H, L						-> 	Move value from register L to register H
	MOV_H_M 	= 	0x66,		//	MOV		H, M						-> 	Move value from memory address in register pair HL to register H
	MOV_H_A 	= 	0x67,		//	MOV		H, A						-> 	Move value from register A to register H
	MOV_L_B		= 	0x68,		//	MOV		L, B						-> 	Move value from register B to register L
	MOV_L_C 	= 	0x69,		//	MOV		L, C						-> 	Move value from register C to register L
	MOV_L_D 	= 	0x6A,		//	MOV		L, D						-> 	Move value from register D to register L
	MOV_L_E 	= 	0x6B,		//	MOV		L, E						-> 	Move value from register E to register L
	MOV_L_H		= 	0x6C,		//	MOV		L, H						-> 	Move value from register H to register L
	MOV_L_L 	= 	0x6D,		//	MOV		L, L						-> 	Move value from register L to register L
	MOV_L_M 	= 	0x6E,		//	MOV		L, M						-> 	Move value from memory address in register pair HL to register L
	MOV_L_A 	= 	0x6F,		//	MOV		L, A						-> 	Move value from register A to register L
 
	//	7x
	MOV_M_B		= 	0x70,		//	MOV		M, B						-> 	Move value from register B to memory address in register pair HL
	MOV_M_C 	= 	0x71,		//	MOV		M, C						-> 	Move value from register C to memory address in register pair HL
	MOV_M_D 	= 	0x72,		//	MOV		M, D						-> 	Move value from register D to memory address in register pair HL
	MOV_M_E 	= 	0x73,		//	MOV		M, E						-> 	Move value from register E to memory address in register pair HL
	MOV_M_H		= 	0x74,		//	MOV		M, H						-> 	Move value from register H to memory address in register pair HL
	MOV_M_L 	= 	0x75,		//	MOV		M, L						-> 	Move value from register L to memory address in register pair HL
	HLT			= 	0x76, 		//	HLT									-> 	Halt CPU
	MOV_M_A 	= 	0x77,		//	MOV		M, A						-> 	Move value from register A to memory address in register pair HL
	MOV_A_B		= 	0x78,		//	MOV		A, B						-> 	Move value from register B to register A
	MOV_A_C 	= 	0x79,		//	MOV		A, C						-> 	Move value from register C to register A
	MOV_A_D 	= 	0x7A,		//	MOV		A, D						-> 	Move value from register D to register A
	MOV_A_E 	= 	0x7B,		//	MOV		A, E						-> 	Move value from register E to register A
	MOV_A_H		= 	0x7C,		//	MOV		A, H						-> 	Move value from register H to register A
	MOV_A_L 	= 	0x7D,		//	MOV		A, L						-> 	Move value from register L to register A
	MOV_A_M 	= 	0x7E,		//	MOV		A, M						-> 	Move value from memory address in register pair HL to register A
	MOV_A_A 	= 	0x7F,		//	MOV		A, A						-> 	Move value from register A to register A
 
	// 8x
	ADD_B		= 	0x80, 		//	ADD		B							->	Add value from register B to register A
	ADD_C 		= 	0x81, 		//	ADD		C							->	Add value from register C to register A
	ADD_D 		= 	0x82, 		//	ADD		D							->	Add value from register D to register A
	ADD_E 		= 	0x83, 		//	ADD		E							->	Add value from register E to register A
	ADD_H		= 	0x84, 		//	ADD		H							->	Add value from register H to register A
	ADD_L 		= 	0x85, 		//	ADD		L							->	Add value from register L to register A
	ADD_M 		= 	0x86, 		//	ADD		M							->	Add value from memory address in register pair HL to register A
	ADD_A 		= 	0x87, 		//	ADD		A							->	Add value from register A to register A
	ADC_B		= 	0x88, 		//	ADC		B							->	Add value from register B to register A with carry
	ADC_C 		= 	0x89, 		//	ADC		C							->	Add value from register C to register A with carry 
	ADC_D 		= 	0x8A, 		//	ADC		D							->	Add value from register D to register A with carry
	ADC_E 		= 	0x8B, 		//	ADC		E							->	Add value from register E to register A with carry
	ADC_H		= 	0x8C, 		//	ADC		H							->	Add value from register H to register A with carry
	ADC_L	 	= 	0x8D, 		//	ADC		L							->	Add value from register L to register A with carry
	ADC_M		= 	0x8E, 		//	ADC		M							->	Add value from memory address in register pair HL to register A with carry
	ADC_A		= 	0x8F, 		//	ADC		A							->	Add value from register A to register A with carry
 
	//	9x
	SUB_B		= 	0x90,		//	SUB		B							-> 	Substract value from register B to register A
	SUB_C 		= 	0x91,		//	SUB		C							-> 	Substract value from register C to register A
	SUB_D 		= 	0x92,		//	SUB		D							-> 	Substract value from register D to register A
	SUB_E 		= 	0x93,		//	SUB		E							-> 	Substract value from register E to register A
	SUB_H		= 	0x94,		//	SUB		H							-> 	Substract value from register H to register A
	SUB_L 		= 	0x95,		//	SUB		L							-> 	Substract value from register L to register A
	SUB_M 		= 	0x96,		//	SUB		M							-> 	Substract value from memory address in register pair HL to register A
	SUB_A 		= 	0x97,		//	SUB		A							-> 	Substract value from register A to register A
	SBB_B		= 	0x98,		//	SBB		B							->	Substract value from register B to register A with borrow
	SBB_C 		= 	0x99,		//	SBB		C							->	Substract value from register C to register A with borrow
	SBB_D 		= 	0x9A,		//	SBB		D							->	Substract value from register D to register A with borrow
	SBB_E 		= 	0x9B,		//	SBB		E							->	Substract value from register E to register A with borrow
	SBB_H		= 	0x9C,		//	SBB		H							->	Substract value from register H to register A with borrow
	SBB_L 		= 	0x9D,		//	SBB		L							->	Substract value from register L to register A with borrow
	SBB_M 		= 	0x9E,		//	SBB		M							->	Substract value from memory address in register pair HL to register A with borrow
	SBB_A 		= 	0x9F,		//	SBB		A							->	Substract value from register A to register A with borrow
 
	//	Ax
	ANA_B		= 	0xA0,		//	ANA		B							->	Logical AND register A with register B
	ANA_C 		= 	0xA1,		//	ANA		C							->	Logical AND register A with register C
	ANA_D 		= 	0xA2,		//	ANA		D							->	Logical AND register A with register D
	ANA_E 		= 	0xA3,		//	ANA		E							->	Logical AND register A with register E
	ANA_H		= 	0xA4,		//	ANA		H							->	Logical AND register A with register H
	ANA_L 		= 	0xA5,		//	ANA		L							->	Logical AND register A with register L
	ANA_M 		= 	0xA6,		//	ANA		M							->	Logical AND register A with value from memory address in register pair HL
	ANA_A 		= 	0xA7,		//	ANA		A							->	Logical AND register A with register A
	XRA_B		= 	0xA8,		//	XRA		B							->	Logical XOR	register A with register B
	XRA_C 		= 	0xA9,		//	XRA		C							->	Logical XOR	register A with register C
	XRA_D 		= 	0xAA,		//	XRA		D							->	Logical XOR	register A with register D
	XRA_E 		= 	0xAB,		//	XRA		E							->	Logical XOR	register A with register E
	XRA_H		= 	0xAC,		//	XRA		H							->	Logical XOR	register A with register H
	XRA_L 		= 	0xAD,		//	XRA		L							->	Logical XOR	register A with register L
	XRA_M 		= 	0xAE,		//	XRA		M							->	Logical XOR	register A with value from memory address in register pair HL
	XRA_A 		= 	0xAF,		//	XRA		A							->	Logical XOR	register A with register A
 
	//	Bx
	ORA_B		= 	0xB0,		//	ORA		B							->	Logical OR register A with register B
	ORA_C 		= 	0xB1,		//	ORA		C							->	Logical OR register A with register C
	ORA_D 		= 	0xB2,		//	ORA		D							->	Logical OR register A with register D
	ORA_E 		= 	0xB3,		//	ORA		E							->	Logical OR register A with register E
	ORA_H		= 	0xB4,		//	ORA		H							->	Logical OR register A with register H
	ORA_L 		= 	0xB5,		//	ORA		L							->	Logical OR register A with register L
	ORA_M 		= 	0xB6,		//	ORA		M							->	Logical OR register A with value from memory address in register pair HL
	ORA_A 		= 	0xB7,		//	ORA		A							->	Logical OR register A with register A
	CMP_B		= 	0xB8, 		//	CMP		B							->	Compare register A with register B
	CMP_C 		= 	0xB9, 		//	CMP		C							->	Compare register A with register C
	CMP_D 		= 	0xBA, 		//	CMP		D							->	Compare register A with register D
	CMP_E 		= 	0xBB, 		//	CMP		E							->	Compare register A with register E
	CMP_H		= 	0xBC, 		//	CMP		H							->	Compare register A with register H
	CMP_L 		= 	0xBD, 		//	CMP		L							->	Compare register A with register L
	CMP_M 		= 	0xBE, 		//	CMP		M							->	Compare register A with value from memory address in register pair HL
	CMP_A 		= 	0xBF, 		//	CMP		A							->	Compare register A with register A
 
	//	Cx
	RNZ			= 	0xC0,		//	RNZ									->	Return from subroutine if not zero														(Flag ZERO = 0)
	POP_B 		= 	0xC1, 		//	POP		B							->	Pop from stack into register pair BC
	JNZ_A16		= 	0xC2,		//	JNZ		0bXXXXXXXXXXXXXXXX			->	Jump if not zero to immediate address 													(Flag ZERO = 0)
	JMP_A16		= 	0xC3,		//	JMP		0bXXXXXXXXXXXXXXXX			->	Jump to immediate address
	CNZ_A16		= 	0xC4, 		//	CNZ		0bXXXXXXXXXXXXXXXX			->	Call subroutine if not zero at immediate address 										(Flag ZERO = 0)
	PUSH_B 		= 	0xC5,		//	PUSH	B							->	Push register pair BC to stack
	ADI_D8 		= 	0xC6,		//	ADI		0bXXXXXXXX					->	Add immediate 8-bit value to register A
	RST_0 		= 	0xC7,		//	RST		0							->	Call restart subroutine at address 0x0000
	RZ			= 	0xC8, 		//	RZ									->	Return from subroutine if zero															(Flag ZERO = 1)
	RET 		= 	0xC9,		//	RET									->	Return from subroutine
	JZ_A16		= 	0xCA,		//	JZ		0bXXXXXXXXXXXXXXXX			->	Jump if zero to immediate address														(Flag ZERO = 1)
	// 			=	0xCB, 		// --- Unused
	CZ_A16		=	0xCC,		// 	CZ		0bXXXXXXXXXXXXXXXX			->	Call subroutine if zero at immediate address											(Flag ZERO = 1)
	CALL_A16	= 	0xCD,		//	CALL	0bXXXXXXXXXXXXXXXX			->	Call subroutine at immediate address
	ACI_D8 		= 	0xCE, 		//	ACI		0bXXXXXXXX					->	Add immediate value to register A with carry
	RST_1 		= 	0xCF,		//	RST		1							->	Call restart subroutine at address 0x0008
 
	//	Dx
	RNC			= 	0xD0,		//	RNC									->	Return from subroutine if not carry														(Flag CARRY = 0)
	POP_D 		= 	0xD1, 		//	POP		D							-> 	Pop from stack into register pair DE
	JNC_A16		= 	0xD2,		//	JNC		0bXXXXXXXXXXXXXXXX			->	Jump if not carry to immediate address													(Flag CARRY = 0)
	OUT_D8 		= 	0xD3,		//	OUT		0bXXXXXXXX					->	Output register A to specified port
	CNC_A16		= 	0xD4, 		//	CNC		0bXXXXXXXXXXXXXXXX			->	Call subroutine if not carry at immediate address										(Flag CARRY	= 0)
	PUSH_D 		= 	0xD5,		//	PUSH	D							->	Push register pair DE to stack
	SUI_D8 		= 	0xD6, 		//	SUI		0bXXXXXXXX					->	Substract immediate 8-bit value from register A
	RST_2 		= 	0xD7,		//	RST		2							->	Call restart subroutine at address 0x0010
	RC			= 	0xD8, 		//	RC									->	Return from subroutine if carry															(Flag CARRY = 1)
	// 			=	0xD9,		// --- Unused
	JC_A16		= 	0xDA,		//	JC		0bXXXXXXXXXXXXXXXX			->	Jump if carry to immediate address														(Flag CARRY = 1)
	IN_D8 		= 	0xDB,		//	IN		0bXXXXXXXX					->	Input from specified port to register A
	CC_A16		=	0xDC,		//	CC		0bXXXXXXXXXXXXXXXX			->	Call subroutine if carry at immediate address											(Flag CARRY = 1)
	// 			=	0xDD		// --- Unused
	SBI_D8		= 	0xDE, 		//	SBI		0bXXXXXXXX					->	Substract immediate 8-bit value from register A with borrow
	RST_3 		=	0xDF,		//	RST		3							->	Call restart subroutine at address 0x0018
 
	//	Ex
	RPO			= 	0xE0, 		//	RPO									->	Return from subroutine if parity is odd													(Flag PARITY = 0)
	POP_H 		= 	0xE1,		//	POP		H							->	Pop from stack into register pair HL
	JPO_A16		= 	0xE2,		//	JPO		0bXXXXXXXXXXXXXXXX			->	Jump if parity is odd to immediate address												(Flag PARITY = 0)
	//	 		= 	0xE3,		// --- Unused
	CPO_A16		= 	0xE4, 		//	CPO		0bXXXXXXXXXXXXXXXX			->	Call subroutine if parity is odd at immediate address									(Flag PARITY = 0)
	PUSH_H 		= 	0xE5,		//	PUSH	H							->	Push register pair HL to stack
	ANI_D8 		= 	0xE6, 		//	ANI		0bXXXXXXXX					->	Logicial AND register A with immediate 8-bit value
	RST_4 		= 	0xE7,		//	RST		4							->	Call restart subroutine at address 0x0020
	RPE			= 	0xE8,		//	RPE									->	Return from subroutine if parity is even												(Flag PARITY = 1)
	PCHL 		= 	0xE9,		//	PCHL								->	Load program counter from register pair HL
	JPE_A16		= 	0xEA, 		//	JPE		0bXXXXXXXXXXXXXXXX			->	Jump if parity is even to immediate address												(Flag PARITY = 1)
	XCHG		= 	0xEB,		//	XCHG								->	Exchange register pair DE and register pair HL
	CPE_A16		=	0xEC, 		//	CPE		0bXXXXXXXXXXXXXXXX			->	Call subroutine if parity is even at immediate address									(Flag PARITY = 1)
	// 			=	0xED		// --- Unused
	XRI_D8		= 	0xEE, 		//	XRI		0bXXXXXXXX					->	Logicial XOR register A with immediate 8-bit value
	RST_5 		= 	0xEF,		//	RST		5							->	Call restart subroutine at address 0x0028
 
	//	Fx
	RP			= 	0xF0,		//	RP									->	Return from subroutine if sign is plus													(Flag SIGN = 0)
	POP_PSW 	= 	0xF1,		//	POP		PSW							->	Pop from stack into register pair constructed from register A and register FLAGS
	JP_A16		= 	0xF2,		//	JP		0bXXXXXXXXXXXXXXXX			->	Jump if sign is plus to immediate address												(Flag SIGN = 0)
	DI 			= 	0xF3,		//	DI									->	Disable interrupts
	CP_A16		= 	0xF4, 		//	CP		0bXXXXXXXXXXXXXXXX			->	Call subroutine if sign is plus at immediate address									(Flag SIGN = 0)
	PUSH_PSW 	= 	0xF5, 		//	PUSH	PSW							-> 	Push register pair constructed from register A and register FLAGS to stack
	ORI_D8 		= 	0xF6, 		//	ORI		0bXXXXXXXX					->	Logical OR register A with immediate 8-bit value
	RST_6 		= 	0xF7,		//	RST		6							->	Call restart subroutine at address 0x0030
	RM			= 	0xF8,		//	RM									->	Return from subroutine is sign is minus													(FLAG SIGN = 1)
	SPHL 		= 	0xF9, 		//	SPHL								->	Load stack pointer from register pair HL
	JM_A16		= 	0xFA, 		//	JM		0bXXXXXXXXXXXXXXXX			->	Jump if sign is minus to immediate address												(Flag SIGN = 1)
	EI 			= 	0xFB,		//	EI									->	Enable interrupts
	CM_A16		= 	0xFC, 		//	CM		0bXXXXXXXXXXXXXXXX			->	Call subroutine if sign is minus at immediate address									(Flag SIGN = 1)
	// 			=	0xFD		// --- Unused
	CPI_D8		= 	0xFE, 		//	CPI		0bXXXXXXXX					->	Compare register A with immediate value
	RST_7 		= 	0xFF		//	RST		7							->	Call restart subroutine at address 0x0038
};
 
void MOV(RegisterRefs dest, RegisterRefs src){
    setRegister(dest, getRegister(src));
}
void MVI(RegisterRefs dest, uint8_t d8){
    setRegister(dest, d8);
}  
void LXI(RegisterPairsRefs dest, uint8_t d8h, uint8_t d8l){
	setRegisterPair(dest, d8h, d8l);
} 
void STAX(RegisterPairsRefs destAddr){
	memory[getRegister(destAddr)] = getRegister(RegisterRefs::A);
} 
void INX(RegisterPairsRefs dest){
	setRegisterPair(dest, getRegister(dest)+1);
}  
void DCX(RegisterPairsRefs dest){
	setRegisterPair(dest, getRegister(dest)-1);
} 
void INR(RegisterRefs dest){
	uint8_t prev = getRegister(dest);
	setRegister(dest, getRegister(dest)+1);
	checkFlags(getRegister(dest), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P);
} 
void DCR(RegisterRefs dest){
	uint8_t prev = getRegister(dest);
	setRegister(dest, getRegister(dest)-1);
	checkFlags(getRegister(dest), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P);
} 
void RLC_op(){};
void RRC_op(){}; 
void RAL_op(){};
void RAR_op(){};  
void RIM_op(){}; 
void SIM_op(){}; 
void DAA_op(){}; 
void CMA_op(){}; 
void DAD(RegisterPairsRefs src){
	uint8_t prev = getRegister(src);
	setRegisterPair(RegisterPairsRefs::HL, getRegister(RegisterPairsRefs::HL)+getRegister(src));
	checkFlags(getRegister(src), prev, FLAG_CY);
}  
void LDAX(RegisterPairsRefs srcAddr){
	setRegister(RegisterRefs::A, memory[getRegister(srcAddr)]);
} 
void SHLD(uint16_t destAddr){
	memory[destAddr] = getRegister(RegisterRefs::L);
	memory[destAddr+1] = getRegister(RegisterRefs::H);  
} 
void LHLD(uint16_t srcAddr){
	setRegister(RegisterRefs::L, memory[srcAddr]);
	setRegister(RegisterRefs::H, memory[srcAddr+1]);
} 
void STA(uint16_t destAddr){
	memory[destAddr] = getRegister(RegisterRefs::A); 
}  
void LDA(uint16_t srcAddr){
	setRegister(RegisterRefs::A, memory[srcAddr]);
} 
void STC_op(){}; 
void CMC_op(){};  
void ADD(RegisterRefs src){
	uint8_t prev = getRegister(src);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) + getRegister(src));
	checkFlags(getRegister(src), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
};
void ADC(RegisterRefs src){
	uint8_t prev = getRegister(src);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) + getRegister(src));
	checkFlags(getRegister(src), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
};
void ADI(uint8_t d8){
	uint8_t prev = getRegister(A);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) + d8);
	checkFlags(getRegister(A), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
}; 
void ACI(uint8_t d8){
	uint8_t prev = getRegister(A);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) + d8);
	checkFlags(getRegister(A), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
}; 
void SUB(RegisterRefs src){
	uint8_t prev = getRegister(src);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) - getRegister(src));
	checkFlags(getRegister(src), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
};
void SBB(RegisterRefs src){
	uint8_t prev = getRegister(src);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) - getRegister(src));
	checkFlags(getRegister(src), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
};
void SUI(uint8_t d8){
	uint8_t prev = getRegister(A);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) - d8);
	checkFlags(getRegister(A), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
} 
void SBI(uint8_t d8){
	uint8_t prev = getRegister(A);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) - d8);
	checkFlags(getRegister(A), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
}
void ANA(RegisterRefs src){
	uint8_t prev = getRegister(src);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) & getRegister(src));
	checkFlags(getRegister(src), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
};
void ANI(uint8_t d8){
	uint8_t prev = getRegister(A);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) & d8);
	checkFlags(getRegister(A), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
}; 
void XRA(RegisterRefs src){
	uint8_t prev = getRegister(src);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) ^ getRegister(src));
	checkFlags(getRegister(src), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
};
void XRI(uint8_t d8){
	uint8_t prev = getRegister(A);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) ^ d8);
	checkFlags(getRegister(A), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
};
void ORA(RegisterRefs src){
	uint8_t prev = getRegister(src);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) | getRegister(src));
	checkFlags(getRegister(src), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
};
void ORI(uint8_t d8){
	uint8_t prev = getRegister(A);
	setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) | d8);
	checkFlags(getRegister(A), prev, FLAG_S | FLAG_Z | FLAG_AC | FLAG_P | FLAG_CY);
}; 
void CMP(RegisterRefs src){
	uint8_t acc = getRegister(RegisterRefs::A);
    uint8_t value = getRegister(src);
    uint8_t result = acc - value;

    // Set flags based on the result
    flag_S = (result & 0x80) != 0;
    flag_Z = (result == 0);
    flag_P = __builtin_parity(result);
    flag_CY = (acc < value); // Carry flag is set if borrow occurs
    flag_AC = ((acc & 0x0F) < (value & 0x0F)); // Auxiliary carry
}; 
void CPI(uint8_t d8){
	uint8_t acc = getRegister(RegisterRefs::A);
    uint8_t value = d8;
    uint8_t result = acc - value;

    // Set flags based on the result
    flag_S = (result & 0x80) != 0;
    flag_Z = (result == 0);
    flag_P = __builtin_parity(result);
    flag_CY = (acc < value); // Carry flag is set if borrow occurs
    flag_AC = ((acc & 0x0F) < (value & 0x0F)); // Auxiliary carry
}; 
void RNZ_op(){
	if(flag_Z == false){
		reg_PC = reg_RET;
	} 
} 
void RZ_op(){
	if(flag_Z == true){
		reg_PC = reg_RET;
	}  
} ;
void RNC_op(){
	if(flag_CY == false){
		reg_PC = reg_RET;
	} 
};
void RC_op(){
	if(flag_CY == true){
		reg_PC = reg_RET;
	} 
}; 
void RPO_op(){
	if(flag_P == false){
		reg_PC = reg_RET;
	} 
} 
void RPE_op(){
	if(flag_P == true){
		reg_PC = reg_RET;
	} 
}; 
void RP_op(){
	if(flag_S == false){
		reg_PC = reg_RET;
	} 
};
void RM_op(){
	if(flag_S == true){
		reg_PC = reg_RET;
	} 
}  
void RET_op(){
	reg_PC = reg_RET;
};
void JNZ(uint16_t destAddr){
	if(flag_Z == false){
		reg_RET = reg_PC + 0x2;
		reg_PC = destAddr;
	} 
};
void JZ(uint16_t destAddr){
	if(flag_Z == true){
		reg_RET = reg_PC + 0x2;
		reg_PC = destAddr;
	} 
};  
void JNC(uint16_t destAddr){
	if(flag_CY == false){
		reg_RET = reg_PC + 0x2;
		reg_PC = destAddr;
	} 
};
void JC(uint16_t destAddr){
	if(flag_CY == true){
		reg_RET = reg_PC + 0x2;
		reg_PC = destAddr;
	} 
};  
void JPO(uint16_t destAddr){
	if(flag_CY == false){
		reg_RET = reg_PC + 0x2;
		reg_PC = destAddr;
	} 
} 
void JPE(uint16_t destAddr){
	if(flag_CY == true){
		reg_RET = reg_PC + 0x2;
		reg_PC = destAddr;
	} 
} 
void JP(uint16_t destAddr){
	if(flag_S == false){
		reg_RET = reg_PC + 0x2;
		reg_PC = destAddr;
	} 
};
void JM(uint16_t destAddr){
	if(flag_S == true){
		reg_RET = reg_PC + 0x2;
		reg_PC = destAddr;
	} 
};
void JMP(uint16_t destAddr){
	reg_RET = reg_PC + 0x2;
	reg_PC = destAddr;
} 
void CNZ(uint16_t destAddr){}; 
void CZ(uint16_t destAddr){};
void CNC(uint16_t destAddr){};
void CC(uint16_t destAddr){};
void CPO(uint16_t destAddr){}; 
void CPE(uint16_t destAddr){}; 
void CP(uint16_t destAddr){};
void CM(uint16_t destAddr){};  
void CALL(uint16_t destAddr){}; 
void POP_op(RegisterRefs dest){
	reg_SP++;
    setRegister(dest, memory[getRegister(RegisterPairsRefs::SP)]);
    memory[getRegister(RegisterPairsRefs::SP)] = 0x00;
}; 
void POPpsw(){
	reg_SP++;
    setRegister(FLAGS, memory[getRegister(RegisterPairsRefs::SP)]);
    memory[getRegister(RegisterPairsRefs::SP)] = 0x00;
	reg_SP++;
    setRegister(A, memory[getRegister(RegisterPairsRefs::SP)]);
    memory[getRegister(RegisterPairsRefs::SP)] = 0x00;
};
void PUSH_op(RegisterRefs src){
	memory[getRegister(RegisterPairsRefs::SP)] = getRegister(src);
    --reg_SP;
}; 
void PUSHpsw(){
	memory[getRegister(RegisterPairsRefs::SP)] = getRegister(FLAGS);
    --reg_SP;
	memory[getRegister(RegisterPairsRefs::SP)] = getRegister(A);
    --reg_SP;
}; 
void RST(int mode){
	switch(mode){
		case 0:
			reg_RET = reg_PC+1;
			reg_PC = 0x0000;
			break;
		case 1:
			reg_RET = reg_PC+1;
			reg_PC = 0x0008;
			break;
		case 2:
			reg_RET = reg_PC+1;
			reg_PC = 0x0010;
			break;
		case 3:
			reg_RET = reg_PC+1;
			reg_PC = 0x0018;
			break;
		case 4:
			reg_RET = reg_PC+1;
			reg_PC = 0x0020;
			break;
		case 5:
			reg_RET = reg_PC+1;
			reg_PC = 0x0028;
			break;
		case 6:
			reg_RET = reg_PC+1;
			reg_PC = 0x0030;
			break;
		case 7:
			reg_RET = reg_PC+1;
			reg_PC = 0x0038;
			break;
	} 
}; 
void OUT(uint8_t portAddr){
	memory[portAddr] = getRegister(RegisterRefs::A); 
} 
void IN(uint8_t portAddr){
	setRegister(RegisterRefs::A, memory[portAddr]);
} 
void PCHL_op(){
	setRegisterPair(RegisterPairsRefs::PC, getRegister(RegisterPairsRefs::HL));
} 
void SPHL_op(){
	setRegisterPair(RegisterPairsRefs::SP, getRegister(RegisterPairsRefs::HL));
};  

void getOperation(){
    std::cout << "Actual instruction at 0x" << std::hex << std::setw(4) << std::setfill('0') << reg_PC << " : 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)memory[reg_PC] << std::endl;
	uint16_t ref = reg_PC;
	uint16_t temp = 0;
	switch(memory[reg_PC]){
		case NOP:
			break;
		case LXI_B_D16:
			LXI(RegisterPairsRefs::BC, memory[ref+0x2], memory[ref+0x1]);
			reg_PC = reg_PC + 2;
			break;
		case STAX_B:
			STAX(RegisterPairsRefs::BC);
			break;
		case INX_B:
			INX(RegisterPairsRefs::BC);
			break;
		case INR_B:
			INR(RegisterRefs::B);
			break;
		case DCR_B:
			DCR(RegisterRefs::B);
			break;
		case MVI_B_D8:
			MVI(RegisterRefs::B, memory[ref+0x1]);
			reg_PC++;
			break;
		case RLC: 
			RLC_op();
			break;
		case DAD_B:
			DAD(RegisterPairsRefs::BC);
			break;
		case LDAX_B:
			LDAX(RegisterPairsRefs::BC);
			break;
		case DCX_B:
			DCX(RegisterPairsRefs::BC);
			break;
		case INR_C:
			INR(RegisterRefs::C);
			break;
		case DCR_C:
			DCR(RegisterRefs::C);
			break;
		case MVI_C_D8:
			MVI(RegisterRefs::C, memory[ref+0x1]);
			reg_PC++;
			break;
		case RRC:
			RRC_op();
			break;

		case LXI_D_D16:
			LXI(RegisterPairsRefs::DE, memory[ref+0x2], memory[ref+0x1]);
			reg_PC = reg_PC + 2;
			break;
		case STAX_D:
			STAX(RegisterPairsRefs::DE);
			break;
		case INX_D:
			INX(RegisterPairsRefs::DE);
			break;
		case INR_D:
			INR(RegisterRefs::D);
			break;
		case DCR_D:
			DCR(RegisterRefs::D);
			break;
		case MVI_D_D8:
			MVI(RegisterRefs::D, memory[ref+0x1]);
			reg_PC++;
			break;
		case RAL:
			RAL_op();
			break;
		case DAD_D:
			DAD(RegisterPairsRefs::DE);
			break;
		case LDAX_D:
			LDAX(RegisterPairsRefs::DE);
			break;
		case DCX_D:
			DCX(RegisterPairsRefs::DE);
			break;
		case INR_E:
			INR(RegisterRefs::E);
			break;
		case DCR_E:
			DCR(RegisterRefs::E);
			break;
		case MVI_E_D8:
			MVI(RegisterRefs::E, memory[ref+0x1]);
			reg_PC++;
			break;
		case RAR:
			RAR_op();
			break;
		
		case RIM:
			RIM_op();
			break;
		case LXI_H_D16:
			LXI(RegisterPairsRefs::HL, memory[ref+0x2], memory[ref+0x1]);
			reg_PC = reg_PC + 2;
			break;
		case SHLD_A16:
			SHLD(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			reg_PC = reg_PC + 2;
			break;
		case INX_H:
			INX(RegisterPairsRefs::HL);
			break;
		case INR_H:
			INR(RegisterRefs::H);
			break;
		case DCR_H:
			DCR(RegisterRefs::H);
			break;
		case MVI_H_D8:
			MVI(RegisterRefs::H, memory[ref+0x1]);
			reg_PC++;
			break;
		case DAA:
			DAA_op();
			break;
		case DAD_H:
			DAD(RegisterPairsRefs::HL);
			break;
		case LHLD_A16:
			LHLD(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			reg_PC = reg_PC + 2;
			break;
		case DCX_H:
			DCX(RegisterPairsRefs::HL);
			break;
		case INR_L:
			INR(RegisterRefs::L);
			break;
		case DCR_L:
			DCR(RegisterRefs::L);
			break;
		case MVI_L_D8:
			MVI(RegisterRefs::L, memory[ref+0x1]);
			reg_PC++;
			break;
		case CMA:
			CMA_op();
			break;

		case SIM:
			SIM_op();
			break;
		case LXI_SP_D16:
			LXI(RegisterPairsRefs::SP, memory[ref+0x2], memory[ref+0x1]);
			reg_PC = reg_PC + 2;
			break;
		case STA_A16:
			STA(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			reg_PC++;
			break;
		case INX_SP:
			INX(RegisterPairsRefs::SP);
			break;
		case INR_M:
			memory[getRegister(RegisterPairsRefs::HL)] = memory[getRegister(RegisterPairsRefs::HL)] + 1;
			break;
		case DCR_M:
			memory[getRegister(RegisterPairsRefs::HL)] = memory[getRegister(RegisterPairsRefs::HL)] - 1;
			break;
		case MVI_M_D8:
			memory[getRegister(RegisterPairsRefs::HL)] = memory[ref+0x1];
			reg_PC++;
			break;
		case STC:
			STC_op();
			break;  
		case DAD_SP:
			DAD(RegisterPairsRefs::SP);
			break;
		case LDA_A16:
			LDA(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			reg_PC = reg_PC + 2;
			break;
		case DCX_SP:
			DCX(RegisterPairsRefs::SP);
			break;
		case INR_A:
			INR(RegisterRefs::A);
			break;
		case DCR_A:
			DCR(RegisterRefs::A);
			break;
		case MVI_A_D8:
			MVI(RegisterRefs::A, memory[ref+0x1]);
			reg_PC++;
			break;
		case CMC:
			CMC_op();
			break;

		case MOV_B_B:
			MOV(RegisterRefs::B, RegisterRefs::B);
			break;
		case MOV_B_C:
			MOV(RegisterRefs::B, RegisterRefs::C);
			break;
		case MOV_B_D:
			MOV(RegisterRefs::B, RegisterRefs::D);
			break;
		case MOV_B_E:
			MOV(RegisterRefs::B, RegisterRefs::E);
			break;
		case MOV_B_H:
			MOV(RegisterRefs::B, RegisterRefs::H);
			break;
		case MOV_B_L:
			MOV(RegisterRefs::B, RegisterRefs::L);
			break;
		case MOV_B_M:
			setRegister(RegisterRefs::B, memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case MOV_B_A:
			MOV(RegisterRefs::B, RegisterRefs::A);
			break;
		case MOV_C_B:
			MOV(RegisterRefs::C, RegisterRefs::B);
			break;
		case MOV_C_C:
			MOV(RegisterRefs::C, RegisterRefs::C);
			break;
		case MOV_C_D:
			MOV(RegisterRefs::C, RegisterRefs::D);
			break;
		case MOV_C_E:
			MOV(RegisterRefs::C, RegisterRefs::E);
			break;
		case MOV_C_H:
			MOV(RegisterRefs::C, RegisterRefs::H);
			break;
		case MOV_C_L:
			MOV(RegisterRefs::C, RegisterRefs::L);
			break;
		case MOV_C_M:
			setRegister(RegisterRefs::C, memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case MOV_C_A:
			MOV(RegisterRefs::C, RegisterRefs::A);
			break;

		case MOV_D_B:
			MOV(RegisterRefs::D, RegisterRefs::B);
			break;
		case MOV_D_C:
			MOV(RegisterRefs::D, RegisterRefs::C);
			break;
		case MOV_D_D:
			MOV(RegisterRefs::D, RegisterRefs::D);
			break;
		case MOV_D_E:
			MOV(RegisterRefs::D, RegisterRefs::E);
			break;
		case MOV_D_H:
			MOV(RegisterRefs::D, RegisterRefs::H);
			break;
		case MOV_D_L:
			MOV(RegisterRefs::D, RegisterRefs::L);
			break;
		case MOV_D_M:
			setRegister(RegisterRefs::D, memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case MOV_D_A:
			MOV(RegisterRefs::D, RegisterRefs::A);
			break;
		case MOV_E_B:
			MOV(RegisterRefs::E, RegisterRefs::B);
			break;
		case MOV_E_C:
			MOV(RegisterRefs::E, RegisterRefs::C);
			break;
		case MOV_E_D:
			MOV(RegisterRefs::E, RegisterRefs::D);
			break;
		case MOV_E_E:
			MOV(RegisterRefs::E, RegisterRefs::E);
			break;
		case MOV_E_H:
			MOV(RegisterRefs::E, RegisterRefs::H);
			break;
		case MOV_E_L:
			MOV(RegisterRefs::E, RegisterRefs::L);
			break;
		case MOV_E_M:
			setRegister(RegisterRefs::E, memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case MOV_E_A:
			MOV(RegisterRefs::E, RegisterRefs::A);
			break;

		case MOV_H_B:
			MOV(RegisterRefs::H, RegisterRefs::B);
			break;
		case MOV_H_C:
			MOV(RegisterRefs::H, RegisterRefs::C);
			break;
		case MOV_H_D:
			MOV(RegisterRefs::H, RegisterRefs::D);
			break;
		case MOV_H_E:
			MOV(RegisterRefs::H, RegisterRefs::E);
			break;
		case MOV_H_H:
			MOV(RegisterRefs::H, RegisterRefs::H);
			break;
		case MOV_H_L:
			MOV(RegisterRefs::H, RegisterRefs::L);
			break;
		case MOV_H_M:
			setRegister(RegisterRefs::H, memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case MOV_H_A:
			MOV(RegisterRefs::H, RegisterRefs::A);
			break;
		case MOV_L_B:
			MOV(RegisterRefs::L, RegisterRefs::B);
			break;
		case MOV_L_C:
			MOV(RegisterRefs::L, RegisterRefs::C);
			break;
		case MOV_L_D:
			MOV(RegisterRefs::L, RegisterRefs::D);
			break;
		case MOV_L_E:
			MOV(RegisterRefs::L, RegisterRefs::E);
			break;
		case MOV_L_H:
			MOV(RegisterRefs::L, RegisterRefs::H);
			break;
		case MOV_L_L:
			MOV(RegisterRefs::L, RegisterRefs::L);
			break;
		case MOV_L_M:
			setRegister(RegisterRefs::L, memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case MOV_L_A:
			MOV(RegisterRefs::L, RegisterRefs::A);
			break;

		case MOV_M_B:
			memory[getRegister(RegisterPairsRefs::HL)] = getRegister(RegisterRefs::B); 
			break;
		case MOV_M_C:
			memory[getRegister(RegisterPairsRefs::HL)] = getRegister(RegisterRefs::C);
			break;
		case MOV_M_D:
			memory[getRegister(RegisterPairsRefs::HL)] = getRegister(RegisterRefs::D);
			break;
		case MOV_M_E:
			memory[getRegister(RegisterPairsRefs::HL)] = getRegister(RegisterRefs::E);
			break;
		case MOV_M_H:
			memory[getRegister(RegisterPairsRefs::HL)] = getRegister(RegisterRefs::H);
			break;
		case MOV_M_L:
			memory[getRegister(RegisterPairsRefs::HL)] = getRegister(RegisterRefs::L);
			break;
		case HLT:
			HALT = true;
			break;
		case MOV_M_A:
			memory[getRegister(RegisterPairsRefs::HL)] = getRegister(RegisterRefs::A);
			break;
		case MOV_A_B:
			MOV(RegisterRefs::A, RegisterRefs::B);
			break;
		case MOV_A_C:
			MOV(RegisterRefs::A, RegisterRefs::C);
			break;
		case MOV_A_D:
			MOV(RegisterRefs::A, RegisterRefs::D);
			break;
		case MOV_A_E:
			MOV(RegisterRefs::A, RegisterRefs::E);
			break;
		case MOV_A_H:
			MOV(RegisterRefs::A, RegisterRefs::H);
			break;
		case MOV_A_L:
			MOV(RegisterRefs::A, RegisterRefs::L);
			break;
		case MOV_A_M:
			setRegister(RegisterRefs::A, memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case MOV_A_A:
			MOV(RegisterRefs::A, RegisterRefs::A);
			break;

		case ADD_B:
			ADD(RegisterRefs::B);
			break;
		case ADD_C:
			ADD(RegisterRefs::C);
			break;
		case ADD_D:
			ADD(RegisterRefs::D);
			break;
		case ADD_E:
			ADD(RegisterRefs::E);
			break;
		case ADD_H:
			ADD(RegisterRefs::H);
			break;
		case ADD_L:
			ADD(RegisterRefs::L);
			break;
		case ADD_M:
			setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) + memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case ADD_A:
			ADD(RegisterRefs::A);
			break;
		case ADC_B:
			ADC(RegisterRefs::B);
			break;
		case ADC_C:
			ADC(RegisterRefs::C);
			break;
		case ADC_D:
			ADC(RegisterRefs::D);
			break;
		case ADC_E:
			ADC(RegisterRefs::E);
			break;
		case ADC_H:
			ADC(RegisterRefs::H);
			break;
		case ADC_L:
			ADC(RegisterRefs::L);
			break;
		case ADC_M:
			setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) + memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case ADC_A:
			ADC(RegisterRefs::A);
			break;

		case SUB_B:
			SUB(RegisterRefs::B);
			break;
		case SUB_C:
			SUB(RegisterRefs::C);
			break;
		case SUB_D:
			SUB(RegisterRefs::D);
			break;
		case SUB_E:
			SUB(RegisterRefs::E);
			break;
		case SUB_H:
			SUB(RegisterRefs::H);
			break;
		case SUB_L:
			SUB(RegisterRefs::L);
			break;
		case SUB_M:
			setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) - memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case SUB_A:
			SUB(RegisterRefs::A);
			break;
		case SBB_B:
			SBB(RegisterRefs::B);
			break;
		case SBB_C:
			SBB(RegisterRefs::C);
			break;
		case SBB_D:
			SBB(RegisterRefs::D);
			break;
		case SBB_E:
			SBB(RegisterRefs::E);
			break;
		case SBB_H:
			SBB(RegisterRefs::H);
			break;
		case SBB_L:
			SBB(RegisterRefs::L);
			break;
		case SBB_M:
			setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) - memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case SBB_A:
			SBB(RegisterRefs::A);
			break;

		case ANA_B:
			ANA(RegisterRefs::B);
			break;
		case ANA_C:
			ANA(RegisterRefs::C);
			break;
		case ANA_D:
			ANA(RegisterRefs::D);
			break;
		case ANA_E:
			ANA(RegisterRefs::E);
			break;
		case ANA_H:
			ANA(RegisterRefs::H);
			break;
		case ANA_L:
			ANA(RegisterRefs::L);
			break;
		case ANA_M:
			setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) & memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case ANA_A:
			ANA(RegisterRefs::A);
			break;
		case XRA_B:
			XRA(RegisterRefs::B);
			break;
		case XRA_C:
			XRA(RegisterRefs::C);
			break;
		case XRA_D:
			XRA(RegisterRefs::D);
			break;
		case XRA_E:
			XRA(RegisterRefs::E);
			break;
		case XRA_H:
			XRA(RegisterRefs::H);
			break;
		case XRA_L:
			XRA(RegisterRefs::L);
			break;
		case XRA_M:
			setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) ^ memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case XRA_A:
			XRA(RegisterRefs::A);
			break;

		case ORA_B:
			ORA(RegisterRefs::B);
			break;
		case ORA_C:
			ORA(RegisterRefs::C);
			break;
		case ORA_D:
			ORA(RegisterRefs::D);
			break;
		case ORA_E:
			ORA(RegisterRefs::E);
			break;
		case ORA_H:
			ORA(RegisterRefs::H);
			break;
		case ORA_L:
			ORA(RegisterRefs::L);
			break;
		case ORA_M:
			setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) | memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case ORA_A:
			ORA(RegisterRefs::A);
			break;
		case CMP_B:
			CMP(RegisterRefs::B);
			break;
		case CMP_C:
			CMP(RegisterRefs::C);
			break;
		case CMP_D:
			CMP(RegisterRefs::D);
			break;
		case CMP_E:
			CMP(RegisterRefs::E);
			break;
		case CMP_H:
			CMP(RegisterRefs::H);
			break;
		case CMP_L:
			CMP(RegisterRefs::L);
			break;
		case CMP_M:
			setRegister(RegisterRefs::A, getRegister(RegisterRefs::A) | memory[getRegister(RegisterPairsRefs::HL)]);
			break;
		case CMP_A:
			CMP(RegisterRefs::A);
			break;

		case RNZ:
			RNZ_op();
			break;
		case POP_B:
			POP_op(RegisterRefs::B);
			break;
		case JNZ_A16:
			JNZ(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case JMP_A16:
			JMP(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case CNZ_A16:
			CNZ(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case PUSH_B:
			PUSH_op(RegisterRefs::B);
			break;
		case ADI_D8:
			ADI(memory[ref+0x1]);
			reg_PC++;
			break;
		case RST_0:
			RST(0);
			break;
		case RZ:
			RZ_op();
			break;
		case RET:
			RET_op();
			break;
		case JZ_A16:
			JZ(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case CZ_A16:
			CZ(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case RST_1:
			RST(1);
			break;

		case RNC:
			RNC_op();
			break;
		case POP_D:
			POP_op(RegisterRefs::D);
			break;
		case JNC_A16:
			JNC(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case OUT_D8:
			OUT(memory[ref+0x1]);
			reg_PC++;
			break;
		case CNC_A16:
			CNC(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case PUSH_D:
			PUSH_op(RegisterRefs::D);
			break;
		case SUI_D8:
			SUI(memory[ref+0x1]);
			reg_PC++;
			break;
		case RST_2:
			RST(2);
			break;
		case RC:
			RC_op();
			break;
		case JC_A16:
			JC(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case IN_D8:
			IN(memory[ref+0x1]);
			reg_PC++;
			break;
		case CC_A16:
			CC(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case SBI_D8:
			SBI(memory[ref+0x1]);
			reg_PC++;
			break;
		case RST_3:
			RST(3);
			break;
		
		case RPO:
			RPO_op();
			break;
		case POP_H:
			POP_op(RegisterRefs::H);
			break;
		case JPO_A16:
			JPO(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case CPO_A16:
			CPO(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case PUSH_H:
			PUSH_op(RegisterRefs::H);
			break;
		case ANI_D8:
			ANI(memory[ref+0x1]);
			reg_PC++;
			break;
		case RST_4:
			RST(4);
			break;
		case RPE:
			RPE_op();
			break;
		case PCHL:
			PCHL_op();
		case JPE_A16:
			JPE(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case XCHG:
			temp = getRegister(RegisterPairsRefs::DE);
			setRegisterPair(RegisterPairsRefs::DE, getRegister(RegisterPairsRefs::HL));
			setRegisterPair(RegisterPairsRefs::HL, temp);
			break;
		case CPE_A16:
			CPE(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case XRI_D8:
			XRI(memory[ref+0x1]);
			reg_PC++;
			break;
		case RST_5:
			RST(5);
			break;

		case RP:
			RP_op();
			break;
		case POP_PSW:
			POPpsw();
			break;
		case JP_A16:
			JP(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case DI:
			break;
		case CP_A16:
			CP(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case PUSH_PSW:
			PUSHpsw();
			break;
		case ORI_D8:
			ORI(memory[ref+0x1]);
			reg_PC++;
			break;
		case RST_6:
			RST(6);
			break;
		case RM:
			RM_op();
			break;
		case SPHL:
			SPHL_op();
			break;
		case JM_A16:
			JM(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case EI:
			break;
		case CM_A16:
			CM(static_cast<uint16_t>(memory[ref+0x2] << 8 | memory[ref+0x1]));
			break;
		case CPI_D8:
			CPI(memory[ref+0x1]);
			reg_PC++;
			break;
		case RST_7:
			RST(7);
			break;

        default:
            break;
    } 
}

void loadProgramInMemory(std::string path, uint8_t memory[], int memorySize, uint16_t offset){

	std::ifstream file(path, std::ios::binary);
	if(!file){
		std::cerr << "Error: could not open program file.\n";
	} 

	size_t index = offset;
	while(file && index < memorySize){
		char byte;
		file.get(byte);
		if(file){
			memory[index++] = static_cast<uint8_t>(byte); 
		} 
	} 

	file.close();
	std::cout << "Program loaded into memory at address 0x" << std::setw(4) << std::setfill('0') << std::hex << offset << std::endl;

} 

void update(){
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    reg_PC++;
} 

void clearPort(){
	for(uint16_t i = 0xFF00; i > (0xFF00-256); i--){
		memory[i] = 0x00;  
	};
} 

void printRegisters(){
    std::cout << "Accumulator register : " << std::endl;
    std::cout << "A : " << std::bitset<8>(getRegister(A)) << " - 0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(reg_A) << std::endl;
    std::cout << std::endl;
    std::cout << "General purpose registers : " << std::endl;
    std::cout << "B : " << std::bitset<8>(getRegister(B)) << " - 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(reg_B) << std::endl;
    std::cout << "C : " << std::bitset<8>(getRegister(C)) << " - 0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(reg_C) << std::endl;
    std::cout << "D : " << std::bitset<8>(getRegister(D)) << " - 0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(reg_D) << std::endl;
    std::cout << "E : " << std::bitset<8>(getRegister(E)) << " - 0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(reg_E) << std::endl;
    std::cout << "H : " << std::bitset<8>(getRegister(H)) << " - 0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(reg_H) << std::endl;
    std::cout << "L : " << std::bitset<8>(getRegister(L)) << " - 0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(reg_L) << std::endl;
    std::cout << std::endl;
    std::cout << "Register pairs : " << std::endl;
    std::cout << "BC : " << std::bitset<16>(getRegister(BC)) << " - 0x" << std::setw(4) << std::setfill('0') << std::hex << getRegister(BC) << std::endl;
    std::cout << "DE : " << std::bitset<16>(getRegister(DE)) << " - 0x" << std::setw(4) << std::setfill('0') << std::hex << getRegister(DE) << std::endl;
    std::cout << "HL : " << std::bitset<16>(getRegister(HL)) << " - 0x" << std::setw(4) << std::setfill('0') << std::hex << getRegister(HL) << std::endl;
    std::cout << std::endl;
    std::cout << "System registers : " << std::endl;
    std::cout << "SP : " << std::bitset<16>(getRegister(SP)) << " - 0x" << std::setw(4) << std::setfill('0') << std::hex << getRegister(SP) << std::endl;
    std::cout << "PC : " << std::bitset<16>(getRegister(PC)) << " - 0x" << std::setw(4) << std::setfill('0') << std::hex << getRegister(PC) << std::endl;
	std::cout << "FLAGS : " << std::bitset<8>(getRegister(FLAGS)) << " - 0x" << std::setw(2) << std::setfill('0') << std::hex << getRegister(FLAGS) << std::endl;
}  

void printAddressArray(uint8_t address[], int addressSize) {
    const int bytesPerLine = 16;
    std::string previousLine, currentLine;
    bool repeated = false;

    for (int i = 0; i < addressSize; i += bytesPerLine) {
        std::ostringstream hexStream, asciiStream;

        // Building HEX and ASCII line
        for (int j = 0; j < bytesPerLine; ++j) {
            if (i + j < addressSize) {
                unsigned char value = static_cast<unsigned char>(address[i + j]);
                hexStream << std::setw(2) << std::setfill('0') << std::hex
                          << std::uppercase << (int)value << " ";
                asciiStream << (std::isprint(value) ? (char)value : '.');
            } else {
                hexStream << "   ";
            }
        }

        currentLine = hexStream.str() + " " + asciiStream.str();

        // Previous line comparison
        if (currentLine == previousLine) {
            if (!repeated) {
                std::cout << "*" << std::endl;
                repeated = true;
            }
        } else {
            std::cout << std::setw(6) << std::setfill('0') << std::hex << std::uppercase
                      << i << "  " << currentLine << std::endl;
            repeated = false;
            previousLine = currentLine;
        }
    }
}


int main() {

	loadProgramInMemory("prog.bin", memory, sizeof(memory), 0x0000);

	reg_PC = 0x0000;
	memory[0x3000] = 0x05;
	memory[0x3001] = 0x02;
	memory[0x3002] = 0x04;
	memory[0x3003] = 0x01;  
	memory[0x3004] = 0x03;  
	
    while(!HALT){
		setFlagReg();
        printRegisters();
        printAddressArray(memory, sizeof(memory));
		clearPort();
        getOperation();
		update();
    }
}