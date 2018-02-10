#include <stdio.h>
#include <algorithm>
#include <string.h>
#include "single_cycle.h"
 
using std::min;

int main()
{   Machine_Init();

	if (!openImage("iimage.bin")) {
		fprintf(stderr, "Can't open file iimage.bin!");
		return 0;
	}
	PC = inputImage[0];											// Load program counter 	
	ImageToMemory(&iMemoryLen, iMemory, inputImage, PC >> 2);	// Load iMemory from inputImage

	if (!openImage("dimage.bin")) {
		fprintf(stderr, "Can't open file dimage.bin");
		return 0;
	} 
	registers[29] = inputImage[0];						// Load stack pointer ($29)
	ImageToMemory(&dMemoryLen, dMemory, inputImage, 0);	// Load dMemory from inputImage	
	
	while (!machine_halt) {
		Output_Status();
		
		// Check program counter is legal ?
		if (AddressOutbound(PC) || DataMisaligned(PC, 4))
			break;
			
		Instruction ins;
		ins.value = iMemory[PC >>2];	// Fetch the instruction
		ins.Decode();					// Decode the instruction
		
		Next_PC = PC +4;

		if (ins.opCode == 0x00) {		// opCode = 0, it's R type
			unsigned int func = ins.value & 0x3f;
			switch(func) {
				case OP_ADD: {	Add(registers, ins);	break;	}
				case OP_SUB: {	Sub(registers, ins);	break;	}
				case OP_AND: {	And(registers, ins);	break;	}
				case OP_OR:  {	Or(registers,  ins);	break;	}
				case OP_XOR: {	Xor(registers, ins);	break;	}
				case OP_NOR: {	Nor(registers, ins);	break;	}
				case OP_NAND:{	Nand(registers, ins); 	break;	}
				case OP_SLT: {	Slt(registers, ins);	break;	}
				case OP_SLL: {	Sll(registers, ins);	break;	}
				case OP_SRL: {	Srl(registers, ins);	break;	}
				case OP_SRA: {	Sra(registers, ins);	break;	}
				case OP_JR:  {	Jr(registers,  ins);	break;	}
				default:	 {	machine_halt = 1;		break;	}
			}
		} else {						// opCode != 0, it's I type or Jtype or Halt.
			switch(ins.opCode) {
				case OP_ADDI:{	Addi(registers, ins);	break;	}
				case OP_LW:  {	Lw(registers,  ins);	break;	}
				case OP_LH:  {	Lh(registers,  ins);	break;	}
				case OP_LHU: {	Lhu(registers, ins);	break;	}
				case OP_LB:  {	Lb(registers,  ins);	break;	}
				case OP_LBU: {	Lbu(registers, ins);	break;	}
				case OP_SW:  {	Sw(registers,  ins);	break;	}
				case OP_SH:  {	Sh(registers,  ins);	break;	}
				case OP_SB:  {	Sb(registers,  ins);	break;	}
				case OP_LUI: {	Lui(registers, ins);	break;	}
				case OP_ANDI:{	Andi(registers, ins);	break;	}
				case OP_ORI: {	Ori(registers, ins);	break;	}
				case OP_NORI:{	Nori(registers, ins);	break;	}
				case OP_SLTI:{	Slti(registers, ins);	break;	}
				case OP_BEQ: {	Beq(registers, ins);	break;	}
				case OP_BNE: {	Bne(registers, ins);	break;	}
				
				case OP_J:   {	J(ins);					break;	}
				case OP_JAL: {	Jal(registers, ins);	break;	}
	
				case OP_HALT:{	machine_halt = 1;		break;	}
				default:	 {	machine_halt = 1;		break;	}
			}	
		} 
		
		PC = Next_PC;
		cycle++;
	} 
	
	fclose(outfile);
	return 0;
}

int byteOrderChecker(void)
{   unsigned int x = 1;
    unsigned char *y = (unsigned char *)&x;
    if (y[0]) 	return Little_Endian;
    return Big_Endian;
}
void Machine_Init()
{	memset(iMemory, 0, sizeof(iMemory));
	memset(dMemory, 0, sizeof(dMemory));
	memset(registers, 0, sizeof(registers));
	
	machine_halt = 0;
	cycle = 0;
}
int openImage(char* filename)
{	unsigned int ImageLen;

	// Open file of iimage.bin (dimage.bin)
	FILE *Image = fopen(filename, "rb");
    if (!Image)
		return 0;
	
    // Get file length of iimage.bin (dimage.bin)
	fseek(Image, 0, SEEK_END);
    ImageLen = ftell(Image);
    fseek(Image, 0, SEEK_SET);

	memset(inputImage, 0, sizeof(inputImage));	// initial
	fread(inputImage, ImageLen, 1, Image);		// read content in iMemory (dMemory)
	fclose(Image);								// close iimage.bin (dimage)
	
	return 1;
}
void ImageToMemory(unsigned int* MemLen, unsigned int *Memory, unsigned int *Image, int start_position)
{	// Get memory length from image[1]
	*MemLen = Image[1];
	// Get memory content
	for (int i = 0; i < Image[1]; ++i)
		Memory[start_position + i] = Image[i +2];
}
void Output_Status()
{	fprintf(outfile, "cycle %d\n", cycle);
    for (int reg_index = 0; reg_index < 32; ++reg_index)
    	fprintf(outfile, "$%0.2d: 0x%08X\n", reg_index, registers[reg_index]);
 	fprintf(outfile, "PC: 0x%08X\n\n\n", PC);
}
/********************************* Exception *********************************/
void RaiseException(int errorType)
{	switch(errorType) {
		case NoEerror: 
			break;
		case WriteReg0Error: 
			fprintf(error_file, "PC: 0x%08X Write $0 error\n", cycle); 
			break;
		case OverflowError:
			fprintf(error_file, "PC: 0x%08X Number overflow\n", cycle); 
			break;
		case AddressOverflowError: 
			fprintf(error_file, "PC: 0x%08X Address overflow\n",cycle);
			machine_halt = 1;
			break;
		case DataMisalignedError: 
			fprintf(error_file, "PC: 0x%08X Miss align error\n", cycle);
			machine_halt = 1;
			break;
		default: 
			fprintf(error_file, "PC: 0x%08X Undefine error\n", cycle);
			break;
	}
}
void Check_Overflow(int src, int nolume, int result, int type)
{	switch(type) {
		case ADD:
			if (!((src ^ nolume) & SIGN_BIT) && ((src ^ result) & SIGN_BIT))
				RaiseException(OverflowError);
			break;
		case SUB:
			if (((src ^ nolume) & SIGN_BIT) && ((src ^ result) & SIGN_BIT))
				RaiseException(OverflowError);
			break;
	}
}
int Write_Register0(int reg_index)
{	if (reg_index == 0)	{
		RaiseException(WriteReg0Error);
		return 1;
	}
	return 0;
}
int AddressOutbound(int reg_value)
{	if (reg_value < 0 || reg_value >= dMemorySize) {
		RaiseException(AddressOverflowError);
		return 1;
	} 
	return 0;
}
int DataMisaligned(int reg_value, int Unit)
{	if (reg_value % Unit != 0) {
		RaiseException(DataMisalignedError);
		return 1;
	}
	return 0;
}
/********************************** R Type **********************************/
void Add(int *reg, Instruction ins)
{	int sum = reg[ins.rs] + reg[ins.rt];

	Check_Overflow(reg[ins.rs], reg[ins.rt], sum, ADD);
	if (!Write_Register0( ins.rd ))
		 reg[ins.rd] = sum;
}
void Sub(int *reg, Instruction ins)
{	int diff = reg[ins.rs] - reg[ins.rt];

	Check_Overflow(reg[ins.rs], reg[ins.rt], diff, SUB);
	if (!Write_Register0( ins.rd ))
		 reg[ins.rd] = diff;
}
void And(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rd ))
		reg[ins.rd] = reg[ins.rs] & reg[ins.rt];
}
void Or(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rd ))
		reg[ins.rd] = reg[ins.rs] | reg[ins.rt];
}
void Xor(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rd ))
		reg[ins.rd] = reg[ins.rs] ^ reg[ins.rt];
}
void Nor(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rd ))
		reg[ins.rd] = ~(reg[ins.rs] | reg[ins.rt]);
}
void Nand(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rd ))
		reg[ins.rd] = ~(reg[ins.rs] & reg[ins.rt]);
}
void Slt(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rd )) {
		if (reg[ins.rs] < reg[ins.rt])	reg[ins.rd] = 1;
		else							reg[ins.rd] = 0;
	}
}
void Sll(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rd ))
		reg[ins.rd] = reg[ins.rt] << (ins.extra);
}
void Srl(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rd )) 
		reg[ins.rd] = (unsigned int)reg[ins.rt] >> (ins.extra);
}
void Sra(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rd )) 
		reg[ins.rd] = reg[ins.rt] >> (ins.extra);
}
void Jr(int *reg, Instruction ins)
{	Next_PC = reg[ins.rs];
}
/********************************** I Type **********************************/
void Addi(int *reg, Instruction ins)
{	int sum = reg[ins.rs] + ins.extra;

	Check_Overflow(reg[ins.rs], ins.extra, sum, ADD);
	if (!Write_Register0( ins.rt ))
		 reg[ins.rt] = sum;
}
void Lw(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 4) && !Write_Register0( ins.rt ))
		reg[ins.rt] = dMemory[Addr >> 2];
}
void Lh(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 2) && !Write_Register0( ins.rt )) {
		int shift = Addr %4;
		reg[ins.rt] = (dMemory[Addr >> 2] & (0xffff << (shift *8))) >> (shift *8);
		if (reg[ins.rt] & 0x8000)		// It's for sign extention	
			reg[ins.rt] |= 0xffff0000;
	}
}
void Lhu(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 2) && !Write_Register0( ins.rt )) {
		int shift = Addr %4;
		reg[ins.rt] = (dMemory[Addr >> 2] & (0xffff << (shift *8))) >> (shift *8);
		reg[ins.rt] &= 0x0000ffff;
	}
}
void Lb(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 1) && !Write_Register0( ins.rt )) {
		int shift = Addr %4;
		reg[ins.rt] = (dMemory[Addr >> 2] & (0xff << (shift *8))) >> (shift *8);
		if (reg[ins.rt] & 0x80)		// It's for sign extention
			reg[ins.rt] |= 0xffffff00;
	}
}
void Lbu(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 1) && !Write_Register0( ins.rt )) {
		int shift = Addr %4;
		reg[ins.rt] = (dMemory[Addr >> 2] & (0xff << (shift *8))) >> (shift *8);
		reg[ins.rt] &= 0x000000ff;
	}
}
void Sw(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 4))
		dMemory[Addr >> 2] = reg[ins.rt];
}
void Sh(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 2)) {
		int shift = Addr %4;
		dMemory[Addr >> 2] &= ((0xffff << (shift *8)) ^ (0xffffffff));	// Clear the entry you want to write later. 
		dMemory[Addr >> 2] |= ((reg[ins.rt] & 0xffff) << (shift *8));	// Write data.
	}
}
void Sb(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 1)) {
		int shift = Addr %4;
		dMemory[Addr >> 2] &= ((0xff << (shift *8)) ^ (0xffffffff));	// Clear the entry you want to write later. 
		dMemory[Addr >> 2] |= ((reg[ins.rt] & 0xff) << (shift *8));		// Write data.
	}
}
void Lui(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rt ))
		reg[ins.rt] = ins.extra << 16;
}
void Andi(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rt ))
		reg[ins.rt] = reg[ins.rs] & (ins.extra & 0x0000ffff);	// Logic operation need not to sign extention
}
void Ori(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rt ))
		reg[ins.rt] = reg[ins.rs] | (ins.extra & 0x0000ffff);	// Logic operation need not to sign extention
}
void Nori(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rt ))
		reg[ins.rt] = ~(reg[ins.rs] | (ins.extra & 0x0000ffff));// Logic operation need not to sign extention
}
void Slti(int *reg, Instruction ins)
{	if (!Write_Register0( ins.rt )) {
		if (reg[ins.rs] < ins.extra)	reg[ins.rt] = 1;
		else							reg[ins.rt] = 0;
	}
}
void Beq(int *reg, Instruction ins)
{	if (reg[ins.rs] == reg[ins.rt]) {
		Next_PC = (PC +4) + (ins.extra << 2);
		Check_Overflow(PC +4, ins.extra << 2, Next_PC, ADD);
	}
}
void Bne(int *reg, Instruction ins)
{	if (reg[ins.rs] != reg[ins.rt]) {
		Next_PC = (PC + 4) + (ins.extra << 2);
		Check_Overflow(PC +4, ins.extra << 2, Next_PC, ADD);
	}
}
/********************************** J Type **********************************/
void J(Instruction ins)
{	Next_PC = ((PC + 4) & 0xf0000000) | (ins.extra << 2);
}
void Jal(int *reg, Instruction ins)
{	reg[31] = PC +4;
	J(ins);
}
