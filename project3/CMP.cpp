#include <stdio.h>
#include <algorithm>
#include <string.h>
#include "CMP.h"
 
using std::min;

int main(int argc, char *argv[])
{   if (argc == 11) {
		sscanf(argv[1], "%d", &iMemorySize);
		sscanf(argv[2], "%d", &dMemorySize);
		sscanf(argv[3], "%d", &iMemoryPageSize);
		sscanf(argv[4], "%d", &dMemoryPageSize);
		sscanf(argv[5], "%d", &iCacheSize);
		sscanf(argv[6], "%d", &iCacheBlockSize);
		sscanf(argv[7], "%d", &iCacheWay);
		sscanf(argv[8], "%d", &dCacheSize);
		sscanf(argv[9], "%d", &dCacheBlockSize);
		sscanf(argv[10], "%d", &dCacheWay);
	} else {
		iMemorySize 	= default_iMemorySize;
		dMemorySize 	= default_dMemorySize;
		iMemoryPageSize = default_iMemoryPageSize;
		dMemoryPageSize = default_dMemoryPageSize;
		iCacheSize 		= default_iCacheSize;
		iCacheBlockSize = default_iCacheBlockSize;
		iCacheWay 		= default_iCacheWay;
		dCacheSize 		= default_dCacheSize;
		dCacheBlockSize = default_dCacheBlockSize;
		dCacheWay 		= default_dCacheWay;
	}
	
	Machine_Init();

	if (!openImage("iimage.bin")) {
		fprintf(stderr, "Can't open file iimage.bin!");
		return 0;
	}
	PC = inputImage[0];									// Load program counter 	
	ImageToDisk(&iDiskLen, iDisk, inputImage, PC >> 2);	// Load iMemory from inputImage

	if (!openImage("dimage.bin")) {
		fprintf(stderr, "Can't open file dimage.bin");
		return 0;
	} 
	registers[29] = inputImage[0];						// Load stack pointer ($29)
	ImageToDisk(&dDiskLen, dDisk, inputImage, 0);		// Load dMemory from inputImage	
	
	while (!machine_halt) {
		Output_Status();
		
		// Check program counter is legal ?
		if (AddressOutbound(PC) || DataMisaligned(PC, 4))
			break;
			
		Instruction ins;
		// Fetch instruction
		IF = 1;
		ins.value = MMU(PC, NONE, iDisk, iPageTable, iMemoryPageSize, iCache, iCacheSize, iCacheBlockSize, iCacheWay, iMemory, iMemorySize);
		IF = 0;
		// Decode the instruction
		ins.Decode();
		
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
	
	Output_Report();
	
	fclose(outfile);
	fclose(outReport);
	
	delete [] iPageTable;
	delete [] iMemory;
	int Num_Of_iCacheBlock = iCacheSize / iCacheBlockSize;
	for (int i = 0; i < (Num_Of_iCacheBlock / iCacheWay); ++i)
		delete [] iCache[i];
	delete [] iCache;
	
	delete [] dPageTable;
	delete [] dMemory;
	int Num_Of_dCacheBlock = dCacheSize / dCacheBlockSize;
	for (int i = 0; i < (Num_Of_dCacheBlock / dCacheWay); ++i)
		delete [] dCache[i];
	delete [] dCache;
	return 0;
}

int byteOrderChecker(void)
{   unsigned int x = 1;
    unsigned char *y = (unsigned char *)&x;
    if (y[0]) 	return Little_Endian;
    return Big_Endian;
}
void Machine_Init()
{	memset(iDisk, 0, sizeof(iDisk));
	memset(dDisk, 0, sizeof(dDisk));
	memset(registers, 0, sizeof(registers));
	
	// Declare instruction page table
	iPageTable = new PageTable_Entry[ iDiskSize / iMemoryPageSize ];
	// Initialize instruction page table 
	for (int id = 0; id < (iDiskSize / iMemoryPageSize); ++id)
		iPageTable[ id ].Clear();
	
	// Declare instruction cache	
	int Num_Of_iCacheBlock = iCacheSize / iCacheBlockSize;
	iCache = new Cache_Entry*[ Num_Of_iCacheBlock / iCacheWay ];
	for (int r = 0; r < (Num_Of_iCacheBlock / iCacheWay); ++r) {
		iCache[ r ] = new Cache_Entry[ iCacheWay ];
		for (int c = 0; c < iCacheWay; ++c)
			iCache[r][c].data = new int[ iCacheBlockSize / sizeof(int) ];
	}
	// Initialize instruction cache	
	for (int r = 0; r < (Num_Of_iCacheBlock / iCacheWay); ++r)
		for (int c = 0; c < iCacheWay; ++c)
			iCache[r][c].Clear();
	
	// Declare instruciton memory;
	iMemory = new Memory_Entry[ iMemorySize / sizeof(int) ];
	// Initialize instruction memroy
	for (int r = 0; r < (iMemorySize / sizeof(int)); ++r)
		iMemory[r].Clear();
	
	// Declare data page table
	dPageTable = new PageTable_Entry[ dDiskSize / dMemoryPageSize ];
	// Initialize data page table 
	for (int id = 0; id < (dDiskSize / dMemoryPageSize); ++id)
		dPageTable[ id ].Clear();
	
	// Declare data cache	
	int Num_Of_dCacheBlock = dCacheSize / dCacheBlockSize;
	dCache = new Cache_Entry*[ Num_Of_dCacheBlock / dCacheWay ];
	for (int r = 0; r < (Num_Of_dCacheBlock / dCacheWay); ++r) {
		dCache[ r ] = new Cache_Entry[ dCacheWay ];
		for (int c = 0; c < dCacheWay; ++c)
			dCache[r][c].data = new int[ dCacheBlockSize / sizeof(int) ];
	}
	// Initialize data cache	
	for (int r = 0; r < (Num_Of_dCacheBlock / dCacheWay); ++r)
		for (int c = 0; c < dCacheWay; ++c)
			dCache[r][c].Clear();
	
	// Declare data memory;
	dMemory = new Memory_Entry[ dMemorySize / sizeof(int) ];
	// Initialize data memroy
	for (int r = 0; r < (dMemorySize / sizeof(int)); ++r)
		dMemory[r].Clear();
	
	iCache_Miss = iPageTable_Miss = 0;
	dCache_Miss = dPageTable_Miss = 0;
	Memory_Access = 0;
	Store_Instruction = 0;
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
	fread(inputImage, ImageLen, 1, Image);		// read content in iDisk (dDisk)
	fclose(Image);								// close iimage.bin (dimage)
	
	return 1;
}
void ImageToDisk(unsigned int* DiskLen, unsigned int *Disk, unsigned int *Image, int start_position)
{	// Get memory length from image[1]
	*DiskLen = Image[1];
	// Get memory content
	for (int i = 0; i < Image[1]; ++i)
		Disk[start_position + i] = Image[i +2];
}
void Output_Status()
{	fprintf(outfile, "cycle %u\n", cycle);
    for (int reg_index = 0; reg_index < 32; ++reg_index)
    	fprintf(outfile, "$%0.2d: 0x%08X\n", reg_index, registers[reg_index]);
 	fprintf(outfile, "PC: 0x%08X\n\n\n", PC);
}
void Output_Report()
{	fprintf(outReport, "ICache :\n");
	fprintf(outReport, "# hits: %u\n", cycle - iCache_Miss );
	fprintf(outReport, "# misses: %u\n\n", iCache_Miss );

	fprintf(outReport, "DCache :\n");
	fprintf(outReport, "# hits: %u\n", Memory_Access - dCache_Miss );
	fprintf(outReport, "# misses: %u\n\n", dCache_Miss );

	fprintf(outReport, "IPageTable :\n");
	fprintf(outReport, "# hits: %u\n", cycle - iPageTable_Miss );
	fprintf(outReport, "# misses: %u\n\n", iPageTable_Miss );

	fprintf(outReport, "DPageTable :\n");
	fprintf(outReport, "# hits: %u\n", Memory_Access - dPageTable_Miss );
	fprintf(outReport, "# misses: %u\n\n", dPageTable_Miss );
}
/***************************** Memory Architechre *****************************/
int MMU(int VA, int Data, unsigned int *Disk, PageTable_Entry *PageTable, int PageSize, Cache_Entry **Cache, 
		int CacheSize, int CacheBlockSize, int CacheWay, Memory_Entry *Memory, int MemorySize)
{	int VPN = VA / PageSize;									// VPN = Virtual Page Number

	if (!(PageTable[ VPN ].Check_Bit & Valid)) {				// Page Fault
		int replace_PPN = Page_Allocate(VPN, Disk, PageTable, PageSize, Cache, CacheSize, CacheBlockSize, CacheWay, Memory, MemorySize);
		
		PageTable[ VPN ].Check_Bit |= Valid;					// Update page table
		PageTable[ VPN ].PPN = replace_PPN;	
		
		if (IF)	++iPageTable_Miss;
		else	++dPageTable_Miss;		
	} 
	// PA = Physical Address
	int PA = PageTable[ VPN ].PPN * PageSize + VA % PageSize;	

	// Access Data from cache
	int request_data = AccessCache(PA, Data, PageSize, Cache, CacheSize, CacheBlockSize, CacheWay, Memory, MemorySize); 
	
	return request_data;
}
int Page_Allocate(	int VPN, unsigned int *Disk, PageTable_Entry *PageTable, int PageSize, Cache_Entry **Cache, 
					int CacheSize, int CacheBlockSize, int CacheWay, Memory_Entry *Memory, int MemorySize)
{	// Find which entry in memory can put page
	int replace_PPN = NONE;			// For invalid entry
	int potential_replace_PPN = 0;	// For LRU
	for (int r = 0; r < (MemorySize / PageSize) && (replace_PPN == NONE); ++r)
		if (!(Memory[ r * PageSize / sizeof(int) ].Check_Bit & Valid))
			replace_PPN = r;
		else if (Memory[ r * PageSize / sizeof(int) ].usedTime < Memory[ potential_replace_PPN * PageSize / sizeof(int)].usedTime)
			potential_replace_PPN = r;

	if (replace_PPN == NONE)	replace_PPN = potential_replace_PPN;

	// Clear page talbe, cache where last page used.
	if (Memory[ replace_PPN * PageSize / sizeof(int) ].Check_Bit & Valid) {
		int past_VPN = Memory[ replace_PPN * PageSize / sizeof(int)].VPN;
		PageTable[ past_VPN ].Check_Bit &= (~Valid);

		// Clear cache entry where pagh which would be replace used before.
		for (int i = 0; i < (PageSize / sizeof(int)); ++i) {
			int PA = replace_PPN * PageSize + i * sizeof(int);
			int Set_Size = (CacheSize / CacheBlockSize) / CacheWay;
			int Set_Number = ((PA / CacheBlockSize) / CacheWay) % Set_Size;
			int tag = (PA / CacheBlockSize) / Set_Size;
			
			for (int c = 0; c < CacheWay; ++c)
				if ((Cache[Set_Number][c].Check_Bit & Valid) && Cache[Set_Number][c].tag == tag) {
					// Write back from cache to memory if dirty
					if (Cache[Set_Number][c].Check_Bit & Dirty) {
						for (int j = 0; j < CacheBlockSize / sizeof(int); ++j) {
							int WB_PA = (Cache[Set_Number][c].tag * Set_Size + Set_Number) * CacheBlockSize + j * sizeof(int);
							
							Memory[ WB_PA >>2 ].data = Cache[Set_Number][c].data[j]; 
						}
						Memory[ (replace_PPN * PageSize) / sizeof(int) ].Check_Bit |= Dirty;
					}
					Cache[Set_Number][c].Clear();
				}
		}
	}
	
	// If page which would be replaced has dirty bit, write back
	if (Memory[ (replace_PPN * PageSize) / sizeof(int) ].Check_Bit & Dirty) {
		for (int i = 0; i < (PageSize / sizeof(int)); ++i) {
			int WB_VA = Memory[ replace_PPN * PageSize / sizeof(int) ].VPN * PageSize + i * sizeof(int);
			int PA = replace_PPN * PageSize + i * sizeof(int);

			Disk[ WB_VA >>2 ] = Memory[ PA >>2 ].data;
		}
		Memory[ replace_PPN * PageSize / sizeof(int)].Check_Bit &= (~Dirty);
	}
	
	// Put page to memory from disk
	Memory[ replace_PPN * PageSize / sizeof(int)].Check_Bit |= Valid;
	Memory[ replace_PPN * PageSize / sizeof(int)].VPN = VPN;
	Memory[ replace_PPN * PageSize / sizeof(int)].usedTime = cycle;
	for (int i = 0; i < (PageSize / sizeof(int)); ++i) {
		int VA = VPN * PageSize + i * sizeof(int);
		int PA = replace_PPN * PageSize + i * sizeof(int);

		Memory[ PA >> 2 ].data = Disk[ VA >> 2 ];
	}
	
	return replace_PPN;		
}
int AccessCache(int PA, int Data, int PageSize, Cache_Entry **Cache, int CacheSize, int CacheBlockSize, int CacheWay, Memory_Entry *Memory, int MemorySize)
{	int Access_Index = NONE;
	int Set_Size = (CacheSize / CacheBlockSize) / CacheWay;
	int Set_Number = ((PA / CacheBlockSize) / CacheWay) % Set_Size;
	
	for (int c = 0; c < CacheWay && (Access_Index == NONE); ++c)
		if ((Cache[Set_Number][c].Check_Bit & Valid) && Cache[Set_Number][c].tag == ((PA / CacheBlockSize) / Set_Size))
			Access_Index = c; 
			
	if (Access_Index == NONE) {
		Access_Index = Cache_Allocate(PA, PageSize, Cache, CacheSize, CacheBlockSize, CacheWay, Memory, MemorySize);	
		
		if (IF)	++iCache_Miss;
		else	++dCache_Miss;
	}
	
	if (Store_Instruction) {
		Cache[Set_Number][Access_Index].data[(PA % CacheBlockSize) / sizeof(int)] = Data;
		Cache[Set_Number][Access_Index].Check_Bit |= Dirty;
	}

	// Update memory frame's usedtime
	int upDate_PPN = ((Cache[Set_Number][Access_Index].tag * Set_Size + Set_Number) * CacheBlockSize / PageSize) * PageSize;
	Memory[ upDate_PPN >>2 ].usedTime = cycle;
	
	return Cache[Set_Number][Access_Index].data[(PA % CacheBlockSize) / sizeof(int)];
}
int Cache_Allocate(int PA, int PageSize, Cache_Entry **Cache, int CacheSize, int CacheBlockSize, int CacheWay, Memory_Entry *Memory, int MemorySize)
{	int Set_Size = (CacheSize / CacheBlockSize) / CacheWay;
	int Set_Number = ((PA / CacheBlockSize) / CacheWay) % Set_Size;
	// Find which entry in cache can put block
	int replace_index = NONE;
	int potential_replace_index = 0;

	for (int c = 0; c < CacheWay && (replace_index == NONE); ++c)
		if (!(Cache[Set_Number][c].Check_Bit & Valid))
			replace_index = c;
		else if (Cache[Set_Number][c].usedTime < Cache[Set_Number][potential_replace_index].usedTime)
			potential_replace_index = c;
	
	if (replace_index == NONE)	replace_index = potential_replace_index;

	// If cache which would be replaced has dirty bit, write back
	if (Cache[Set_Number][replace_index].Check_Bit & Dirty) {
		for (int i = 0; i < CacheBlockSize / sizeof(int); ++i) {
			int WB_PA = (Cache[Set_Number][replace_index].tag * Set_Size + Set_Number) * CacheBlockSize + i * sizeof(int);
		
			Memory[ WB_PA >>2 ].data = Cache[Set_Number][replace_index].data[i]; 
		}
		Cache[Set_Number][replace_index].Check_Bit &= (~Dirty);
		
		int WB_PPN = ((Cache[Set_Number][replace_index].tag * Set_Size + Set_Number) * CacheBlockSize / PageSize) * PageSize / sizeof(int);

		Memory[ WB_PPN ].Check_Bit |= Dirty;
	}

	// Put block to cache from memory
	Cache[Set_Number][replace_index].Check_Bit |= Valid;
	Cache[Set_Number][replace_index].tag = (PA / CacheBlockSize) / Set_Size;
	Cache[Set_Number][replace_index].usedTime = cycle;
	for (int i = 0; i < CacheBlockSize / sizeof(int); ++i) {
		int upDate_PA = (Cache[Set_Number][replace_index].tag * Set_Size + Set_Number) * CacheBlockSize + i * sizeof(int);

		Cache[Set_Number][replace_index].data[i] = Memory[ upDate_PA >>2 ].data;
	}
	
	return replace_index;	
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
{	if (reg_value < 0 || reg_value >= dDiskSize) {
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
	
	int data = MMU(Addr, NONE, dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
	++Memory_Access;
	
	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 4) && !Write_Register0( ins.rt )) 
		reg[ins.rt] = data;
}
void Lh(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	int data = MMU((Addr /4) *4, NONE, dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
	++Memory_Access;

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 2) && !Write_Register0( ins.rt )) {
		reg[ins.rt] = data;
		
		int shift = Addr %4;
		reg[ins.rt] = (reg[ins.rt] & (0xffff << (shift *8))) >> (shift *8);
		if (reg[ins.rt] & 0x8000)		// It's for sign extention	
			reg[ins.rt] |= 0xffff0000;
	}
}
void Lhu(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	int data = MMU((Addr /4) *4, NONE, dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
	++Memory_Access;
		
	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 2) && !Write_Register0( ins.rt )) {
		reg[ins.rt] = data;
		
		int shift = Addr %4;
		reg[ins.rt] = (reg[ins.rt] & (0xffff << (shift *8))) >> (shift *8);
		reg[ins.rt] &= 0x0000ffff;
	}

}
void Lb(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	int data = MMU((Addr /4) *4, NONE, dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
	++Memory_Access;
		
	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 1) && !Write_Register0( ins.rt )) {
		reg[ins.rt] = data;
		
		int shift = Addr %4;
		reg[ins.rt] = (reg[ins.rt] & (0xff << (shift *8))) >> (shift *8);
		if (reg[ins.rt] & 0x80)		// It's for sign extention
			reg[ins.rt] |= 0xffffff00;
	}
}
void Lbu(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	int data = MMU((Addr /4) *4, NONE, dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
	++Memory_Access;
	
	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 1) && !Write_Register0( ins.rt )) {
		reg[ins.rt] = data;
		
		int shift = Addr %4;
		reg[ins.rt] = (reg[ins.rt] & (0xff << (shift *8))) >> (shift *8);
		reg[ins.rt] &= 0x000000ff;
	}
}
void Sw(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);
	
	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 4)) {
		Store_Instruction = 1;
		MMU(Addr, reg[ins.rt], dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
		Store_Instruction = 0;
	
		++Memory_Access;
	}
}
void Sh(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 2)) {
		int tmp = MMU((Addr /4) *4, NONE, dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
		
		int shift = Addr %4;
		tmp &= ((0xffff << (shift *8)) ^ (0xffffffff));	// Clear the entry you want to write later. 
		tmp |= ((reg[ins.rt] & 0xffff) << (shift *8));	// Write data.
		
		Store_Instruction = 1;
		MMU((Addr /4) *4, tmp, dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
		Store_Instruction = 0;
	
		++Memory_Access;
	}
}
void Sb(int *reg, Instruction ins)
{	int Addr = reg[ins.rs] + ins.extra;
	Check_Overflow(reg[ins.rs], ins.extra, Addr, ADD);

	if (!AddressOutbound(Addr) && !DataMisaligned(Addr, 1)) {
		int tmp = MMU((Addr /4) *4, NONE, dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
		
		int shift = Addr %4;
		tmp &= ((0xff << (shift *8)) ^ (0xffffffff));		// Clear the entry you want to write later. 
		tmp |= ((reg[ins.rt] & 0xff) << (shift *8));		// Write data.
		
		Store_Instruction = 1;
		MMU((Addr /4) *4, tmp, dDisk, dPageTable, dMemoryPageSize, dCache, dCacheSize, dCacheBlockSize, dCacheWay, dMemory, dMemorySize);
		Store_Instruction = 0;
	
		++Memory_Access;
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
