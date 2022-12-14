	const lsn::CCpu6502::LSN_INSTR_META_DATA lsn::CCpu6502::m_smdInstMetaData[LSN_I_TOTAL] = {					/**< Metadata for the instructions (for assembly and disassembly etc.) */
		{ "ADC",	u8"Add with carry." },
		{ "AND",	u8"Bitwise AND with accumulator." },
		{ "ASL",	u8"Arithmetic shift left (shifts in a zero bit on the right)." },
		{ "BCC",	u8"Branch on carry clear." },
		{ "BCS",	u8"Branch on carry set." },
		{ "BEQ",	u8"Branch on equal (zero set)." },
		{ "BIT",	u8"Bit test." },
		{ "BMI",	u8"Branch on minus (negative set)." },
		{ "BNE",	u8"Branch on not equal (zero clear)." },
		{ "BPL",	u8"Branch on plus (negative clear)." },
		{ "BRK",	u8"Break/interrupt." },
		{ "BVC",	u8"Branch on overflow clear." },
		{ "BVS",	u8"Branch on overflow set." },
		{ "CLC",	u8"Clear carry." },
		{ "CLD",	u8"Clear decimal." },
		{ "CLI",	u8"Clear interrupt-disable." },
		{ "CLV",	u8"Clear overflow." },
		{ "CMP",	u8"Compare (with accumulator)." },
		{ "CPX",	u8"Compare with X." },
		{ "CPY",	u8"Compare with Y." },
		{ "DEC",	u8"Decrement." },
		{ "DEX",	u8"Decrement X." },
		{ "DEY",	u8"Decrement Y." },
		{ "EOR",	u8"Bitwise XOR with accumulator." },
		{ "INC",	u8"Increment." },
		{ "INX",	u8"Increment X." },
		{ "INY",	u8"Increment Y." },
		{ "JMP",	u8"Jump." },
		{ "JSR",	u8"Jump subroutine." },
		{ "LDA",	u8"Load accumulator." },
		{ "LDX",	u8"Load X." },
		{ "LDY",	u8"Load Y." },
		{ "LSR",	u8"Logical shift right (shifts in a zero bit on the left." },
		{ "NOP",	u8"No operation." },
		{ "ORA",	u8"Bitwise OR with accumulator." },
		{ "PHA",	u8"Push accumulator." },
		{ "PHP",	u8"Push processor status (SR)." },
		{ "PLA",	u8"Pull accumulator." },
		{ "PLP",	u8"Pull processor status (SR)." },
		{ "ROL",	u8"Rotate left (shifts in carry bit on the right." },
		{ "ROR",	u8"Rotate right." },
		{ "RTI",	u8"Return from interrupt." },
		{ "RTS",	u8"Return from subroutine." },
		{ "SBC",	u8"Subtract with carry." },
		{ "SEC",	u8"Set carry." },
		{ "SED",	u8"Set decimal." },
		{ "SEI",	u8"Set interrupt-disable." },
		{ "STA",	u8"Store accumulator." },
		{ "STX",	u8"Store X." },
		{ "STY",	u8"Store Y." },
		{ "TAX",	u8"Transfer accumulator to X." },
		{ "TAY",	u8"Transfer accumulator to Y." },
		{ "TSX",	u8"Transfer stack pointer to X." },
		{ "TXA",	u8"Transfer X to accumulator." },
		{ "TXS",	u8"Transfer X to stack pointer." },
		{ "TYA",	u8"Transfer Y to accumulator." },

		{ "ALR",	u8"Illegal. Performs A & OP; LSR." },
		{ "ANC",	u8"Illegal. Bitwise AND with carry. C flag is set using ASL rules." },
		{ "ANC2",	u8"Illegal. Bitwise AND with carry. C flag is set using ROL rules." },
		{ "ANE",	u8"Illegal and highly unstable. Performs A = (A | CONST) & X & OP, where CONST is effectively random." },
		{ "ARR",	u8"Illegal. Performs A = A & OP; A = (A >> 1) | (C << 7)." },
		{ "DCP",	u8"Illegal. Performs OP--; CMP(A, OP)." },
		{ "ISC",	u8"Illegal. Performs INC; SBC." },
		{ "LAS",	u8"Illegal. Performs A = X = S = (OP & S)." },
		{ "LAX",	u8"Illegal. Performs A = X = OP." },
		{ "LXA",	u8"Illegal and highly unstable. Performs X = A = ((A | CONST) & OP), where CONST is effectively random." },
		{ "RLA",	u8"Illegal. ROL OP, A &= OP." },
		{ "RRA",	u8"Illegal. ROR OP, A += OP + C." },
		{ "SAX",	u8"Illegal. Performs OP = A & X." },
		{ "SBX",	u8"Illegal. Performs X = (A & X) - OP, setting flags as with CMP." },
		{ "SHA",	u8"Illegal. Stores A & X & (high-byte of address + 1) at the address." },
		{ "SHX",	u8"Illegal. Stores X & (high-byte of address + 1) at the address." },
		{ "SHY",	u8"Illegal. Stores Y & (high-byte of address + 1) at the address." },
		{ "SLO",	u8"Illegal. OP <<= 1, A &= OP." },
		{ "SRE",	u8"Illegal. OP >>= 1, A ^= OP." },
		{ "TAS",	u8"Illegal. Performs S = A & X; Stores A & X & (high-byte of address + 1) at the address." },
		{ "USBC",	u8"Illegal. Performs SBC + NOP." },
		{ "DOP",	u8"Illegal. No operation." },
		{ "TOP",	u8"Illegal. No operation." },
		{ "JAM",	u8"Illegal. Jams the machine repeatedly putting 0xFF on the bus." },

		{ "NMI",	u8"Non-opcode: NMI handler." },
		{ "IRQ",	u8"Non-opcode: IRQ handler." },
	};