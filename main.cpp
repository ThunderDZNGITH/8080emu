#include <iostream>
#include <bitset>
#include <thread>
#include <chrono>
#include <iomanip> 
 
uint8_t reg_A, reg_B, reg_C, reg_D, reg_E, reg_H, reg_L; // ACCUMULATOR, GENERAL REGISTERS (8 bits)
uint16_t reg_SP, reg_PC; // STACK POINTER, PROGRAM COUNTER (16 bits)

bool HALT = false;
 
// DDD = Destination, SSS = Source
enum RegisterRefs {
    A = 0b111,
    B = 0b000,
    C = 0b001,
    D = 0b010,
    E = 0b011,
    H = 0b100,
    L = 0b101
};
 
// RP = Register Pair
enum RegisterPairsRefs {
    BC = 0b00,
    DE = 0b01,
    HL = 0b10,
    SP = 0b11,
    PC = 0b100
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
void setRegister(RegisterPairsRefs reg, uint16_t d16) {
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
            reg_H = (d16 << 8) & 0xFF;
            reg_L = d16 & 0xFF;
            break;
        case RegisterPairsRefs::SP:
            reg_SP = d16;
            break;
        default:
            reg_PC = d16;
            break;
    }
}
void setRegister(RegisterPairsRefs reg, uint8_t d8h, uint8_t d8l) {
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
        default:
            return 0;
            break;
    } 
    return 0;
} 

struct Flags {
    bool Z, S, P, CY, AC; // ZERO, SIGN, PARITY, CARRY, AUX-CARRY
};
 
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

uint8_t memory[0xFFFF] = {0x00};

void update(){
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    reg_PC++;
} 

void printRegisters(){
    std::cout << "Accumulator register : " << std::endl;
    std::cout << "A : " << std::bitset<8>(getRegister(A)) << std::endl;
    std::cout << std::endl;
    std::cout << "General purpose registers : " << std::endl;
    std::cout << "B : " << std::bitset<8>(getRegister(B)) << std::endl;
    std::cout << "C : " << std::bitset<8>(getRegister(C)) << std::endl;
    std::cout << "D : " << std::bitset<8>(getRegister(D)) << std::endl;
    std::cout << "E : " << std::bitset<8>(getRegister(E)) << std::endl;
    std::cout << "H : " << std::bitset<8>(getRegister(H)) << std::endl;
    std::cout << "L : " << std::bitset<8>(getRegister(L)) << std::endl;
    std::cout << std::endl;
    std::cout << "Register pairs : " << std::endl;
    std::cout << "BC : " << std::bitset<16>(getRegister(BC)) << std::endl;
    std::cout << "DE : " << std::bitset<16>(getRegister(DE)) << std::endl;
    std::cout << "HL : " << std::bitset<16>(getRegister(HL)) << std::endl;
    std::cout << std::endl;
    std::cout << "System registers : " << std::endl;
    std::cout << "SP : " << std::bitset<16>(getRegister(SP)) << std::endl;
    std::cout << "PC : " << std::bitset<16>(getRegister(PC)) << " - 0x" << std::hex << std::setw(4) << std::setfill('0') << getRegister(PC) << std::endl;
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

void getOperation(){
    std::cout << "Actual instruction at 0x" << std::hex << std::setw(4) << std::setfill('0') << reg_PC << " : 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)memory[reg_PC] << std::endl;
    switch(memory[reg_PC]){
        case MOV_C_B:
            MOV(RegisterRefs::C, RegisterRefs::B);
            break;
        case MVI_A_D8:
            MVI(RegisterRefs::A, memory[reg_PC+1]);
            reg_PC++;
            break;
        default:
            break;
    } 
} 

int main() {
    memory[0x7c00] = MOV_C_B; 
    reg_PC = 0x7bFD;

    setRegister(B, 0b10101010);

    memory[0x7c0f] = MVI_A_D8;
    memory[0x7c10] = 0b11001100;  

    while(!HALT){
        update();
        printRegisters();
        printAddressArray(memory, sizeof(memory));
        getOperation();
    } 
}