#include <stdio.h>
#include <algorithm>
#include <string.h>
#include "pipeline.h"
 
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
		
		WB(registers, pipelineRegs);
		
		DM(pipelineRegs);
		
		EX(registers, pipelineRegs);
		
		ID(registers, pipelineRegs);

		Instruction ins;
		ins.value = iMemory[PC >>2];	// Fetch the instruction
		ins.Decode();					// PreDecode for output!
		
		// output
		Output_Stages(ins, pipelineRegs);
		
		// Shift pipeline registers depend on stall status
		pipelineRegs[ MEM_to_WB ].Ins = pipelineRegs[ EX_to_MEM ].Ins;
		pipelineRegs[ MEM_to_WB ].PC = pipelineRegs[ EX_to_MEM ].PC;
		
		pipelineRegs[ EX_to_MEM ].Ins = pipelineRegs[ ID_to_EX ].Ins;
		pipelineRegs[ EX_to_MEM ].PC = pipelineRegs[ ID_to_EX ].PC;
		
		if (pipelineRegs[ ID_to_EX ].Control & Add_NOP) {
			pipelineRegs[ ID_to_EX ].Flush();
		} else {
			pipelineRegs[ ID_to_EX ].Ins = pipelineRegs[ IF_to_ID ].Ins;
			pipelineRegs[ ID_to_EX ].PC = pipelineRegs[ IF_to_ID ].PC;
		}
		
		if (PC_Src) {													// Branch taken
			pipelineRegs[ IF_to_ID ].Flush();
		} else if (!(pipelineRegs[ IF_to_ID ].Control & Add_Stall)) {	// Normal execut
			pipelineRegs[ IF_to_ID ].Ins = ins;
			pipelineRegs[ IF_to_ID ].PC = PC +4;
		} else {														// Stall
			Next_PC = PC;
			PC_Src = 1;
		}
			
		if (!PC_Src)	PC = PC +4;
		else			PC = Next_PC;
		
		PC_Src = 0;
		pipelineRegs[ MEM_to_WB ].Control &= ~(Add_NOP | Add_Stall);	// Clear add NOP, Stall flag 
		pipelineRegs[ EX_to_MEM ].Control &= ~(Add_NOP | Add_Stall);	// Clear add NOP, Stall flag 
		pipelineRegs[ ID_to_EX ].Control &= ~(Add_NOP | Add_Stall);		// Clear add NOP, Stall flag 
		pipelineRegs[ IF_to_ID ].Control &= ~(Add_NOP | Add_Stall);		// Clear add NOP, Stall flag 
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
	
	pipelineRegs[IF_to_ID].Flush();		// Insert NOP
	pipelineRegs[ID_to_EX].Flush();		// Insert NOP
	pipelineRegs[EX_to_MEM].Flush();	// Insert NOP
	pipelineRegs[MEM_to_WB].Flush();	// Insert NOP
	
	machine_halt = 0;
	cycle = 0;
	PC_Src = 0;
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
void Hazard_Detect_unit(PipelineReg *pReg)
{	int hazard_occured = 0;
	char IF_to_ID_rs = pReg[ IF_to_ID ].Ins.rs, IF_to_ID_rt = pReg[ IF_to_ID ].Ins.rt;
	char ID_to_EX_rt = pReg[ ID_to_EX ].Ins.rt;
	
	if ((pReg[ ID_to_EX ].Control & Mem_Read) ) {
		if (IF_to_ID_rs == ID_to_EX_rt)				// Handle instructions like sll srl sra have don't care entry  
			if ((pReg[ IF_to_ID ].Ins.opCode != R_TYPE) || ((pReg[ IF_to_ID ].Ins.value & 0x38) != 0x00))				
				hazard_occured = 1;
		
		if (IF_to_ID_rt == ID_to_EX_rt)
			if (pReg[ IF_to_ID ].Ins.opCode == R_TYPE)				hazard_occured = 1;	// Handle I Type don't need rt except "Store intruction"
			else if ((pReg[ IF_to_ID ].Ins.opCode & 0x38) == 0x28)	hazard_occured = 1;	// Handle "Store instruction"
	}
	// Special Case for instructions which need execute in ID stage
	if (pReg[ IF_to_ID ].Ins.opCode == OP_BEQ || pReg[ IF_to_ID ].Ins.opCode == OP_BNE) {	
		if ((pReg[ EX_to_MEM ].rd != 0) && (IF_to_ID_rs == pReg[ EX_to_MEM ].rd || IF_to_ID_rt == pReg[ EX_to_MEM ].rd)) 
			if (pReg[ MEM_to_WB ].Control & Mem_Read)		hazard_occured = 1;	// If is "Load instructions" in DM, need to wait another cycle
			else if (pReg[ EX_to_MEM ].Control & Reg_Write)	hazard_occured = 1;	// Wait for any instruction which use ALU unit
	} else if (pReg[ IF_to_ID ].Ins.opCode == R_TYPE && (pReg[ IF_to_ID ].Ins.value & 0x3f) == OP_JR) {
		if (pReg[ EX_to_MEM ].rd != 0 && IF_to_ID_rs == pReg[ EX_to_MEM ].rd) 
			if (pReg[ MEM_to_WB ].Control & Mem_Read)		hazard_occured = 1;	// If is "Load instructions" in DM, need to wait another cycle
			else if (pReg[ EX_to_MEM ].Control & Reg_Write)	hazard_occured = 1;	// Wait for any instruction which use ALU unit
	}
	
	if (hazard_occured) {
		pReg[ ID_to_EX ].Control |= Add_NOP;
		pReg[ IF_to_ID ].Control |= Add_Stall;
	}
}
void Forwarding_unit_ID(PipelineReg *pReg)
{	if ((Control_for_WB & Reg_Write) && (rd_for_WB != 0) && (rd_for_WB == pReg[ IF_to_ID ].Ins.rs))
		ForwardA_ID = Forward_WB;
	if ((Control_for_WB & Reg_Write) && (rd_for_WB != 0) && (rd_for_WB == pReg[ IF_to_ID ].Ins.rt))
		if (pReg[ IF_to_ID ].Ins.opCode != R_TYPE)	// Handle special case for jr which don't need rt
			ForwardB_ID = Forward_WB;
		
	if ((pReg[ MEM_to_WB ].Control & Reg_Write) && (pReg[ MEM_to_WB ].rd != 0) && (pReg[ MEM_to_WB ].rd == pReg[ IF_to_ID ].Ins.rs))
		ForwardA_ID = Forward_MEM;
	if ((pReg[ MEM_to_WB ].Control & Reg_Write) && (pReg[ MEM_to_WB ].rd != 0) && (pReg[ MEM_to_WB ].rd == pReg[ IF_to_ID ].Ins.rt))
		if (pReg[ IF_to_ID ].Ins.opCode != R_TYPE)	// Handle special case for jr which don't need rt
			ForwardB_ID = Forward_MEM;
}
void Forwarding_unit_EX(PipelineReg *pReg) 
{	if ((Control_for_WB & Reg_Write) && (rd_for_WB != 0) && (rd_for_WB == pReg[ ID_to_EX ].Ins.rs))
		if ((pReg[ ID_to_EX ].Ins.opCode != R_TYPE) || ((pReg[ ID_to_EX ].Ins.value & 0x38) != 0x00))	// Handle for "Shift instruction"
			if (pReg[ ID_to_EX ].Ins.opCode != OP_LUI)													// which don't need rs
				ForwardA_EX = Forward_WB;
	if ((Control_for_WB & Reg_Write) && (rd_for_WB != 0) && (rd_for_WB == pReg[ ID_to_EX ].Ins.rt))
		if (pReg[ ID_to_EX ].Ins.opCode == R_TYPE || (pReg[ ID_to_EX ].Ins.opCode & 0x38) ==0x28)		// Rtype or Store instruction need rt
			ForwardB_EX = Forward_WB;
		
	if ((pReg[ MEM_to_WB ].Control & Reg_Write) && (pReg[ MEM_to_WB ].rd != 0) && (pReg[ MEM_to_WB ].rd == pReg[ ID_to_EX ].Ins.rs))
		if ((pReg[ ID_to_EX ].Ins.opCode != R_TYPE) || ((pReg[ ID_to_EX ].Ins.value & 0x38) != 0x00))	// Handle for "Shift instruction"
			if (pReg[ ID_to_EX ].Ins.opCode != OP_LUI)													// which don't need rs
				ForwardA_EX = Forward_MEM;
	if ((pReg[ MEM_to_WB ].Control & Reg_Write) && (pReg[ MEM_to_WB ].rd != 0) && (pReg[ MEM_to_WB ].rd == pReg[ ID_to_EX ].Ins.rt))
		if (pReg[ ID_to_EX ].Ins.opCode == R_TYPE || (pReg[ ID_to_EX ].Ins.opCode & 0x38) ==0x28)		// Rtype or Store instruction need rt
			ForwardB_EX = Forward_MEM;
}
void Control_unit(PipelineReg *pReg)
{	char opCode = pReg[ IF_to_ID ].Ins.opCode;
	unsigned char func = pReg[ IF_to_ID ].Ins.value & 0x3f;
	
	if (opCode == R_TYPE)	pReg[ ID_to_EX ].Control |= Reg_Dst;
	else					pReg[ ID_to_EX ].Control &= ~Reg_Dst;
	
	if (opCode != R_TYPE)	pReg[ ID_to_EX ].Control |= ALU_Src;
	else					pReg[ ID_to_EX ].Control &= ~ALU_Src;
	
	if ((opCode & 0x38) == 0x20)	pReg[ ID_to_EX ].Control |= Mem_To_Reg;
	else							pReg[ ID_to_EX ].Control &= ~Mem_To_Reg;
	
	if ((opCode & 0x38) == 0x20)	pReg[ ID_to_EX ].Control |= Mem_Read;
	else							pReg[ ID_to_EX ].Control &= ~Mem_Read;
	
	if (opCode == R_TYPE && func == OP_JR)			pReg[ ID_to_EX ].Control &= ~Reg_Write;
	else if (opCode == OP_BEQ || opCode == OP_BNE) 	pReg[ ID_to_EX ].Control &= ~Reg_Write;
	else if (opCode == OP_SW || opCode == OP_SH)	pReg[ ID_to_EX ].Control &= ~Reg_Write;
	else if (opCode == OP_SB || opCode == OP_J)		pReg[ ID_to_EX ].Control &= ~Reg_Write;
	else if (opCode == OP_HALT)						pReg[ ID_to_EX ].Control &= ~Reg_Write;
	else											pReg[ ID_to_EX ].Control |= Reg_Write;
}
int ALU_unit(int busA, int busB, PipelineReg *pReg)
{	if (pReg[ ID_to_EX ].Ins.opCode == R_TYPE) {
		unsigned char func = pReg[ ID_to_EX ].Ins.value & 0x3f;
		switch (func) {
			case OP_ADD: {	return busA + busB;											}
			case OP_SUB: {	return busA - busB;											}
			case OP_AND: {	return busA & busB;											}
			case OP_OR:  {	return busA | busB;											}
			case OP_XOR: {	return busA ^ busB;											}
			case OP_NOR: {	return ~(busA | busB);										}	
			case OP_NAND:{	return ~(busA & busB);										}
			case OP_SLT: {	return busA < busB;											}
			case OP_SLL: {	return busB << ( pReg[ ID_to_EX ].Ins.extra );				}
			case OP_SRL: {	return (unsigned int)busB >> ( pReg[ ID_to_EX ].Ins.extra );}
			case OP_SRA: {	return busB >> ( pReg[ ID_to_EX ].Ins.extra);				}	
		}	
	} else {
		int immediate = pReg[ ID_to_EX ].Ins.extra;
		switch (pReg[ID_to_EX].Ins.opCode) {
			case OP_ADDI:{	return busA + immediate;					}
			case OP_LW:  {	return busA + immediate;					}
			case OP_LH:  {	return busA + immediate;					}	
			case OP_LHU: {	return busA + immediate;					}
			case OP_LB:  {	return busA + immediate;					}
			case OP_LBU: {	return busA + immediate;					}
			case OP_SW:  {	return busA + immediate;					}
			case OP_SH:  {	return busA + immediate;					}
			case OP_SB:  {	return busA + immediate;					}
			case OP_LUI: {	return immediate << 16;						}
			case OP_ANDI:{	return busA & (immediate & 0x0000ffff);		}
			case OP_ORI: {	return busA | (immediate & 0x0000ffff);		}
			case OP_NORI:{	return ~(busA | (immediate & 0x0000ffff));	}
			case OP_SLTI:{	return busA < immediate;					}
		}
	}
}
void ID(int *reg, PipelineReg *pReg)
{	pReg[ IF_to_ID ].Ins.Decode();	// Decode the instruction
	
	ForwardA_ID = ForwardB_ID = Forward_NONE;
	if (pReg[ IF_to_ID ].Ins.opCode == OP_HALT)	{
		return ;
	} else if (pReg[ IF_to_ID ].Ins.value == NOP)	{
		pReg[ ID_to_EX ].Control = 0x00;
		return;
	}
	
	Hazard_Detect_unit(pReg);
	
	if (pReg[ IF_to_ID ].Ins.opCode == OP_BEQ || pReg[ IF_to_ID ].Ins.opCode == OP_BNE)
		Forwarding_unit_ID(pReg);
	else if (pReg[ IF_to_ID ].Ins.opCode == R_TYPE && (pReg[ IF_to_ID ].Ins.value & 0x3f) == OP_JR)
		Forwarding_unit_ID(pReg);
		
	switch (ForwardA_ID) {
		case Forward_NONE:{	pReg[ ID_to_EX ].reg_rs = reg[ pReg[ IF_to_ID ].Ins.rs ];	break;	}
		case Forward_WB:  {	
			if (pReg[ MEM_to_WB ].Ins.opCode == OP_JAL)		pReg[ ID_to_EX ].reg_rs = pReg[ MEM_to_WB ].PC;		// Special case for jal
			else											pReg[ ID_to_EX ].reg_rs = reg[ rd_for_WB ];					
			break;	
		}  
		case Forward_MEM: {	
			if (pReg[ EX_to_MEM ].Ins.opCode == OP_JAL)		pReg[ ID_to_EX ].reg_rs = pReg[ EX_to_MEM ].PC;		// Special case for jal
			else											pReg[ ID_to_EX ].reg_rs = pReg[ MEM_to_WB ].ALU_Result;		
			break;	
		}
	}
	
	switch (ForwardB_ID) {
		case Forward_NONE:{	pReg[ ID_to_EX ].reg_rt = reg[ pReg[ IF_to_ID ].Ins.rt ];	break;	}
		case Forward_WB:  {	
			if (pReg[ MEM_to_WB ].Ins.opCode == OP_JAL)		pReg[ ID_to_EX ].reg_rt = pReg[ MEM_to_WB ].PC;		// Special case for jal
			else											pReg[ ID_to_EX ].reg_rt = reg[ rd_for_WB ];
			break;	
		}
		case Forward_MEM: {	
			if (pReg[ EX_to_MEM ].Ins.opCode == OP_JAL)		pReg[ ID_to_EX ].reg_rt = pReg[ EX_to_MEM ].PC;		// Special case for jal
			else											pReg[ ID_to_EX ].reg_rt = pReg[ MEM_to_WB ].ALU_Result;
			break;	
		}
	}
	
	Control_unit(pReg);
	// If ID need not to be stalled
	if (!(pReg[ IF_to_ID ].Control & Add_Stall)) { 
		if (pReg[ IF_to_ID ].Ins.opCode == R_TYPE && (pReg[ IF_to_ID ].Ins.value & 0x3f) == OP_JR) {
			Jr(reg, pReg);
			PC_Src = 1;
		} else {
			int reg_Rs = pReg[ ID_to_EX ].reg_rs;
			int reg_Rt = pReg[ ID_to_EX ].reg_rt;
			switch (pReg[ IF_to_ID ].Ins.opCode) {
				case OP_BEQ: {	Beq(reg_Rs, reg_Rt, pReg);	break;	}
				case OP_BNE: {	Bne(reg_Rs, reg_Rt, pReg);	break;	}
				case OP_J:   {	J(pReg);					break;	}
				case OP_JAL: {	J(pReg);					break;	}
			}
		}
	}
}
void EX(int *reg, PipelineReg *pReg)
{	ForwardA_EX = ForwardB_EX = Forward_NONE;

	if (pReg[ ID_to_EX ].Ins.opCode == OP_HALT || pReg[ ID_to_EX ].Ins.value == NOP) {
		pReg[ EX_to_MEM ].Control = pReg[ ID_to_EX ].Control;
		return ;
	}
	
	if (pReg[ ID_to_EX ].Ins.opCode != OP_BEQ && pReg[ ID_to_EX ].Ins.opCode != OP_BNE)
		if (!(pReg[ ID_to_EX ].Ins.opCode == R_TYPE && (pReg[ ID_to_EX ].Ins.value & 0x3f) == OP_JR))	// Aviod jr's don't care entry effect forward
			Forwarding_unit_EX(pReg);
	
	int busA, busB;

	switch (ForwardA_EX) {
		case Forward_NONE: {	busA = pReg[ ID_to_EX ].reg_rs;			break;	}
		case Forward_WB:   {	
			if (pReg[ MEM_to_WB ].Ins.opCode == OP_JAL)	busA = pReg[ MEM_to_WB ].PC;
			else										busA = reg[ rd_for_WB ];				
			break;	
		}
		case Forward_MEM:  {	
			if (pReg[ EX_to_MEM ].Ins.opCode == OP_JAL)	busA = pReg[ EX_to_MEM ].PC;
			else										busA = pReg[ MEM_to_WB ].ALU_Result;	
			break;
		}
	}
	
	switch (ForwardB_EX) {
		case Forward_NONE: {
			if (!(pReg[ ID_to_EX ].Control & ALU_Src))	busB = pReg[ ID_to_EX ].reg_rt;
			else										busB = pReg[ ID_to_EX ].Ins.extra;	
			break;
		}
		case Forward_WB:   {	
			if ((pReg[ ID_to_EX ].Ins.opCode & 0x38) == 0x28) {	// Special case for "Store instructions"
				pReg[ ID_to_EX ].reg_rt = reg[ rd_for_WB ];
				busB = pReg[ ID_to_EX ].Ins.extra;		
			} 
			else if (pReg[ MEM_to_WB ].Ins.opCode == OP_JAL)		busB = pReg[ MEM_to_WB ].PC;
			else													busB = reg[ rd_for_WB ];				
			break;	
		}
		case Forward_MEM:  {	
			if ((pReg[ ID_to_EX ].Ins.opCode & 0x38) == 0x28) {	// Special case for "Store instructions"
				pReg[ ID_to_EX ].reg_rt = pReg[ MEM_to_WB ].ALU_Result;
				busB = pReg[ ID_to_EX ].Ins.extra;	
			} 
			else if (pReg[ EX_to_MEM ].Ins.opCode == OP_JAL)				busB = pReg[ EX_to_MEM ].PC;
			else													busB = pReg[ MEM_to_WB ].ALU_Result;	
			break;	
		}
	}
	
	pReg[ EX_to_MEM ].ALU_Result = ALU_unit(busA, busB, pReg);
	
	if (pReg[ ID_to_EX ].Control & Reg_Dst)	pReg[ EX_to_MEM ].rd = pReg[ ID_to_EX ].Ins.rd;
	else									pReg[ EX_to_MEM ].rd = pReg[ ID_to_EX ].Ins.rt;
	
	if (pReg[ ID_to_EX ].Ins.opCode == OP_JAL)	pReg[ EX_to_MEM ].rd = 31;	// Special case for jal
	
	pReg[ EX_to_MEM ].Control = pReg[ ID_to_EX ].Control;
	pReg[ EX_to_MEM	].reg_rs = pReg[ ID_to_EX ].reg_rs;
	pReg[ EX_to_MEM	].reg_rt = pReg[ ID_to_EX ].reg_rt;
}
void DM(PipelineReg *pReg)
{	if (pReg[ EX_to_MEM ].Ins.opCode == OP_HALT || pReg[ EX_to_MEM ].Ins.value == NOP) {
		pReg[ MEM_to_WB ].Control = pReg[ EX_to_MEM ].Control;
		return ;
	}
	
	switch (pReg[ EX_to_MEM ].Ins.opCode) {
		case OP_LW:	{	Lw(pReg);	break;	}
		case OP_LH:	{	Lh(pReg);	break;	}
		case OP_LHU:{	Lhu(pReg);	break;	}
		case OP_LB:	{	Lb(pReg);	break;	}
		case OP_LBU:{	Lbu(pReg);	break;	}
		case OP_SW:	{	Sw(pReg);	break;	}
		case OP_SH:	{	Sh(pReg);	break;	}
		case OP_SB:	{	Sb(pReg);	break;	}
	}
	
	pReg[ MEM_to_WB ].Control = pReg[ EX_to_MEM ].Control;
	pReg[ MEM_to_WB ].ALU_Result = pReg[ EX_to_MEM ].ALU_Result;
	pReg[ MEM_to_WB ].rd = pReg[ EX_to_MEM ].rd;
}
void WB(int *reg, PipelineReg *pReg)
{	rd_for_WB = pReg[ MEM_to_WB ].rd;
	
	if (pReg[ MEM_to_WB ].Ins.opCode == OP_HALT) {
		machine_halt = 1;
	} else if (pReg[ MEM_to_WB ].Ins.opCode == OP_JAL) {
		rd_for_WB = 31;
		reg[ rd_for_WB ] = pReg[ MEM_to_WB ].PC;
	} else if ((pReg[ MEM_to_WB ].Control & Reg_Write) && (rd_for_WB != 0)) {	// Handle wirte register 0
		if (pReg[ MEM_to_WB ].Control & Mem_To_Reg)	reg[ rd_for_WB ] = pReg[ MEM_to_WB ].Memory_dada;
		else										reg[ rd_for_WB ] = pReg[ MEM_to_WB ].ALU_Result;
	} 
	
	Control_for_WB = pReg[ MEM_to_WB ].Control;
}
void Output_Ins(Instruction ins)
{	if (ins.value == NOP) {
		fprintf(outfile, "NOP");
	} else if (ins.opCode == 0x00) {	// opCode = 0, it's R type
		unsigned int func = ins.value & 0x3f;
		switch(func) {
			case OP_ADD: {	fprintf(outfile, "ADD");	break;	}
			case OP_SUB: {	fprintf(outfile, "SUB");	break;	}
			case OP_AND: {	fprintf(outfile, "AND");	break;	}
			case OP_OR:  {	fprintf(outfile, "OR");		break;	}
			case OP_XOR: {	fprintf(outfile, "XOR");	break;	}
			case OP_NOR: {	fprintf(outfile, "NOR");	break;	}
			case OP_NAND:{	fprintf(outfile, "NAND"); 	break;	}
			case OP_SLT: {	fprintf(outfile, "SLT");	break;	}
			case OP_SLL: {	fprintf(outfile, "SLL");	break;	}
			case OP_SRL: {	fprintf(outfile, "SRL");	break;	}
			case OP_SRA: {	fprintf(outfile, "SRA");	break;	}
			case OP_JR:  {	fprintf(outfile, "JR");		break;	}
		}
	} else {								// opCode != 0, it's I type or Jtype or Halt.
		switch(ins.opCode) {
			case OP_ADDI:{	fprintf(outfile, "ADDI");	break;	}
			case OP_LW:  {	fprintf(outfile, "LW");		break;	}
			case OP_LH:  {	fprintf(outfile, "LH");		break;	}
			case OP_LHU: {	fprintf(outfile, "LHU");	break;	}
			case OP_LB:  {	fprintf(outfile, "LB");		break;	}
			case OP_LBU: {	fprintf(outfile, "LBU");	break;	}
			case OP_SW:  {	fprintf(outfile, "SW");		break;	}
			case OP_SH:  {	fprintf(outfile, "SH");		break;	}
			case OP_SB:  {	fprintf(outfile, "SB");		break;	}
			case OP_LUI: {	fprintf(outfile, "LUI");	break;	}
			case OP_ANDI:{	fprintf(outfile, "ANDI");	break;	}
			case OP_ORI: {	fprintf(outfile, "ORI");	break;	}
			case OP_NORI:{	fprintf(outfile, "NORI");	break;	}
			case OP_SLTI:{	fprintf(outfile, "SLTI");	break;	}
			case OP_BEQ: {	fprintf(outfile, "BEQ");	break;	}
			case OP_BNE: {	fprintf(outfile, "BNE");	break;	}
				
			case OP_J:   {	fprintf(outfile, "J");		break;	}
			case OP_JAL: {	fprintf(outfile, "JAL");	break;	}
	
			case OP_HALT:{	fprintf(outfile, "HALT");	break;	}
		}	
	}
}
void Output_Stages(Instruction ins, PipelineReg *pReg)
{	fprintf(outfile, "IF: ");
	fprintf(outfile, "0x%08X", ins.value);
	if (pReg[ IF_to_ID ].Control & Add_Stall)	fprintf(outfile, " to_be_stalled");
	else if (PC_Src)							fprintf(outfile, " to_be_flushed");
	fprintf(outfile, "\n");
	
	fprintf(outfile, "ID: ");
	Output_Ins(pReg[ IF_to_ID ].Ins);
	if (pReg[ IF_to_ID ].Control & Add_Stall) {
		fprintf(outfile, " to_be_stalled");
	} else if ((ForwardA_ID != Forward_NONE) || (ForwardB_ID != Forward_NONE)) {
		if (ForwardA_ID != Forward_NONE && ForwardA_ID != Forward_WB)	fprintf(outfile, " fwd_EX-DM_rs_$%d", (int)pReg[ IF_to_ID ].Ins.rs);
		if (ForwardB_ID != Forward_NONE	&& ForwardB_ID != Forward_WB)	fprintf(outfile, " fwd_EX-DM_rt_$%d", (int)pReg[ IF_to_ID ].Ins.rt);
	}	
	fprintf(outfile, "\n");
	
	fprintf(outfile, "EX: ");
	Output_Ins(pReg[ ID_to_EX ].Ins);
	if (ForwardA_EX == Forward_MEM)			fprintf(outfile, " fwd_EX-DM_rs_$%d", (int)pReg[ ID_to_EX ].Ins.rs);
	else if (ForwardA_EX == Forward_WB)		fprintf(outfile, " fwd_DM-WB_rs_$%d", (int)pReg[ ID_to_EX ].Ins.rs);
	
	if (ForwardB_EX == Forward_MEM)			fprintf(outfile, " fwd_EX-DM_rt_$%d", (int)pReg[ ID_to_EX ].Ins.rt);
	else if (ForwardB_EX == Forward_WB)		fprintf(outfile, " fwd_DM-WB_rt_$%d", (int)pReg[ ID_to_EX ].Ins.rt);
	fprintf(outfile, "\n");
	
	fprintf(outfile, "DM: ");
	Output_Ins(pReg[ EX_to_MEM].Ins);
	fprintf(outfile, "\n");
	
	fprintf(outfile, "WB: ");
	Output_Ins(pReg[ MEM_to_WB ].Ins);
	fprintf(outfile, "\n\n\n");
}
void Output_Status()
{	fprintf(outfile, "cycle %d\n", cycle);
    for (int reg_index = 0; reg_index < 32; ++reg_index)
    	fprintf(outfile, "$%0.2d: 0x%08X\n", reg_index, registers[reg_index]);
 	fprintf(outfile, "PC: 0x%08X\n", PC);
}
/********************************** R Type **********************************/
void Jr(int *reg, PipelineReg *pReg)
{	Next_PC = pReg[ ID_to_EX ].reg_rs;
}
/********************************** I Type **********************************/
void Lw(PipelineReg *pReg)
{	int Addr = pReg[EX_to_MEM].ALU_Result;
	
	pReg[ MEM_to_WB ].Memory_dada = dMemory[Addr >> 2];
}
void Lh(PipelineReg *pReg)
{	int Addr = pReg[EX_to_MEM].ALU_Result;
	
	int shift = Addr %4;
	pReg[ MEM_to_WB ].Memory_dada = (dMemory[Addr >> 2] & (0xffff << (shift *8))) >> (shift *8);
	if (pReg[ MEM_to_WB ].Memory_dada & 0x8000)		// It's for sign extention	
		pReg[ MEM_to_WB ].Memory_dada |= 0xffff0000;
}
void Lhu(PipelineReg *pReg)
{	int Addr = pReg[EX_to_MEM].ALU_Result;

	int shift = Addr %4;
	pReg[ MEM_to_WB ].Memory_dada = (dMemory[Addr >> 2] & (0xffff << (shift *8))) >> (shift *8);
	pReg[ MEM_to_WB ].Memory_dada &= 0x0000ffff;
}
void Lb(PipelineReg *pReg)
{	int Addr = pReg[EX_to_MEM].ALU_Result;
	
	int shift = Addr %4;
	pReg[ MEM_to_WB ].Memory_dada = (dMemory[Addr >> 2] & (0xff << (shift *8))) >> (shift *8);
	if (pReg[ MEM_to_WB ].Memory_dada & 0x80)		// It's for sign extention
		pReg[ MEM_to_WB ].Memory_dada |= 0xffffff00;
}
void Lbu(PipelineReg *pReg)
{	int Addr = pReg[EX_to_MEM].ALU_Result;
	
	int shift = Addr %4;
	pReg[ MEM_to_WB ].Memory_dada = (dMemory[Addr >> 2] & (0xff << (shift *8))) >> (shift *8);
	pReg[ MEM_to_WB ].Memory_dada &= 0x000000ff;
}
void Sw(PipelineReg *pReg)
{	int Addr = pReg[EX_to_MEM].ALU_Result;

	dMemory[Addr >> 2] = pReg[EX_to_MEM].reg_rt;
}
void Sh(PipelineReg *pReg)
{	int Addr = pReg[EX_to_MEM].ALU_Result;

	int shift = Addr %4;
	dMemory[Addr >> 2] &= ((0xffff << (shift *8)) ^ (0xffffffff));			// Clear the entry you want to write later. 
	dMemory[Addr >> 2] |= ((pReg[EX_to_MEM].reg_rt & 0xffff) << (shift *8));// Write data.
}
void Sb(PipelineReg *pReg)
{	int Addr = pReg[EX_to_MEM].ALU_Result;

	int shift = Addr %4;
	dMemory[Addr >> 2] &= ((0xff << (shift *8)) ^ (0xffffffff));			// Clear the entry you want to write later. 
	dMemory[Addr >> 2] |= ((pReg[EX_to_MEM].reg_rt & 0xff) << (shift *8));	// Write data.
}
void Beq(int reg_Rs, int reg_Rt, PipelineReg *pReg)
{	if (reg_Rs == reg_Rt) {
		Next_PC = pReg[ IF_to_ID ].PC + (pReg[ IF_to_ID ].Ins.extra << 2);
		PC_Src = 1;
	}
}
void Bne(int reg_Rs, int reg_Rt, PipelineReg *pReg)
{	if (reg_Rs != reg_Rt) {
		Next_PC = pReg[ IF_to_ID ].PC + (pReg[ IF_to_ID ].Ins.extra << 2);
		PC_Src = 1;
	}
}
/********************************** J Type **********************************/
void J(PipelineReg *pReg)
{	Next_PC = (pReg[ IF_to_ID ].PC & 0xf0000000) | (pReg[ IF_to_ID ].Ins.extra << 2);
	PC_Src = 1;
}
