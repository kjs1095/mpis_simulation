#define R_TYPE 0
#define SIGN_BIT	0x80000000
#define NONE -1

typedef struct {
	unsigned int value;	// binary representation of the instruction
	char opCode;		// Type of instruction.
	char rs, rt, rd;	// Three registers from instruction.
	int extra;			// Immediate or target or shamt field or offset.
						// Immediates are sign-extended.
	
	void Decode() {
		opCode = (value >> 26) & 0x3f;
		rs = (value >> 21) & 0x1f;
		rt = (value >> 16) & 0x1f;
		rd = (value >> 11) & 0x1f;
		
		if (opCode == 0x00)			extra = (value >> 6) & 0x1f;// Rtype instructions
		else if (opCode == 0x02)	extra = value & 0x3ffffff;	// Jtype instruction : j
		else if (opCode == 0x03)	extra = value & 0x3ffffff;	// Jtype instruction : jal
		else if (opCode == 0x3f) 	extra = value & 0x3ffffff;	// Halt 
		else {													// Itype instructions
			extra = value & 0xffff;		
			if (extra & 0x8000)		extra |= 0xffff0000;		// It's for sign extention.
		} 
	}
}Instruction;

typedef struct {
	char Check_Bit;	// Dirty bit, Valid bit
	int tag;		// Higher bit of physical address
	int *data;		// Store data from lower-level memory
	int usedTime;	// Last used time for LRU
	
	void Clear()	{	Check_Bit = 0;	}
}Cache_Entry;

typedef struct {
	char Check_Bit;	// Dirty bit, Valid bit
	int data;		// Store data from disk
	int usedTime;	// Last used time for LRU
	int VPN;		// Virtual Page Number for inverse
	
	void Clear()	{	
		Check_Bit = 0;
		data = 0;
	}
}Memory_Entry;

typedef struct {
	char Check_Bit;	// Dirty bit, Valid bit
	int PPN;		// Physical Page Number
	
	void Clear()	{	Check_Bit = 0;	}
}PageTable_Entry;

const int default_iMemorySize = 64;		// bytes
const int default_dMemorySize = 32;		// bytes
const int default_iMemoryPageSize = 8;	// bytes
const int default_dMemoryPageSize = 16;	// bytes
const int default_iCacheSize = 16;		// bytes
const int default_iCacheBlockSize = 4;	// bytes
const int default_iCacheWay = 4;		// bytes
const int default_dCacheSize = 16;		// bytes
const int default_dCacheBlockSize = 4;	// bytes
const int default_dCacheWay = 1;		// bytes

const int iDiskSize = 1024;	// bytes
const int dDiskSize = 1024;	// bytes
const int NumOfGPRegs = 32;	// 32 general purpose registers on MIPS

unsigned int PC, Next_PC;							// Program Counter
int registers[NumOfGPRegs];							// Registers, each has 32 bits
unsigned int iDisk[ iDiskSize/sizeof(int) ];		// One entry contains 4 bytes, so size of iDisk[] is (1K bytes / 4 bytes) 
unsigned int dDisk[ dDiskSize/sizeof(int) ];		// One entry contains 4 bytes, so size of dDisk[] is (1K bytes / 4 bytes) 
unsigned int iDiskLen, dDiskLen;					// Length of iDisk (dDisk)
unsigned int inputImage[ iDiskSize/sizeof(int) +2];	// Store image file's, anthoer 2 entry is store PC (sp) , MemLen for iDisk (dDisk).

#define Dirty 0x02
#define Valid 0x01
int iMemorySize, dMemorySize;						// Size of main memory for instruction (data)
int iMemoryPageSize, dMemoryPageSize;				// iMemory (dMemory) page size
int iCacheSize, dCacheSize;							// Size of cache for instruction (data)
int iCacheBlockSize, dCacheBlockSize;				// iCache (dCache) block size
int iCacheWay, dCacheWay;							// # of associative way to access iCache (dCache) 
PageTable_Entry *iPageTable, *dPageTable;
Cache_Entry **iCache, **dCache;
Memory_Entry *iMemory, *dMemory;
int MMU(int, int, unsigned int* , PageTable_Entry*, int, Cache_Entry**, int, int, int, Memory_Entry*, int);
int Page_Allocate(int, unsigned int*, PageTable_Entry *, int, Cache_Entry**, int, int, int, Memory_Entry*, int);
int AccessCache(int, int, int, Cache_Entry**, int, int, int, Memory_Entry*, int);
int Cache_Allocate(int, int, Cache_Entry**, int, int, int, Memory_Entry*, int);

char IF;
char Store_Instruction;
unsigned int iCache_Miss, iPageTable_Miss;
unsigned int dCache_Miss, dPageTable_Miss, Memory_Access;
unsigned int cycle;											
int machine_halt;									// Flag to halt the machine

enum ByteOrder {
	Little_Endian,
	Big_Endian
};
int byteOrderChecker(void);		// Check machine's byte order
void Machine_Init();			// Initial machine's regiters, memory.
int openImage(char*);			// Load image file.
void ImageToDisk(unsigned int*, unsigned int*, unsigned int*, int);
void Output_Status();
void Output_Report();
FILE *outfile = fopen("snapshot.rpt", "w");
FILE *outReport = fopen("report.rpt", "w");

#define ADD 0
#define SUB 1
enum Error { 
	NoEerror,             // Everything ok!
	WriteReg0Error,       // Try to write register 0
	OverflowError,        // Integer overflow in add or sub.
	AddressOverflowError, // Unaligned reference or one that was beyond the end of the address space
	DataMisalignedError   // Try to access data at a memory offset not equal to some multiple of the basic block size
};
void RaiseException(int errorType);								// Process errors
int Write_Register0(int rd_index);								// Check write register 0 ?
void Check_Overflow(int src, int nolume, int result, int type);	// Check result of computaion overflow ?
int AddressOutbound(int reg_value);								// Check memory address which we want to access is out of memory ?
int DataMisaligned(int reg_value, int Unit);					// Check dada misaligned ?
FILE *error_file = fopen("Error_dump.rpt", "w");

#define OP_ADD	0x20
#define OP_SUB	0x22
#define OP_AND	0x24
#define OP_OR	0x25
#define OP_XOR	0x26
#define OP_NOR	0x27
#define OP_NAND	0x28
#define OP_SLT	0x2A
#define OP_SLL	0x00
#define OP_SRL	0x02
#define OP_SRA	0x03
#define OP_JR	0x08
void Add(int *reg, Instruction ins);
void Sub(int *reg, Instruction ins);
void And(int *reg, Instruction ins);
void Or(int *reg, Instruction ins);
void Xor(int *reg, Instruction ins);
void Nor(int *reg, Instruction ins);
void Nand(int *reg, Instruction ins);
void Slt(int *reg, Instruction ins);
void Sll(int *reg, Instruction ins);
void Srl(int *reg, Instruction ins);
void Sra(int *reg, Instruction ins);
void Jr(int *reg, Instruction ins);

#define OP_ADDI	0x08
#define OP_LW	0x23
#define OP_LH	0x21
#define OP_LHU	0x25
#define OP_LB	0x20
#define OP_LBU	0x24
#define OP_SW	0x2B
#define OP_SH	0x29
#define OP_SB	0x28
#define OP_LUI	0x0F
#define OP_ANDI	0x0C
#define OP_ORI	0x0D
#define OP_NORI	0x0E
#define OP_SLTI	0x0A
#define OP_BEQ	0x04
#define OP_BNE	0x05
void Addi(int *reg, Instruction ins);
void Lw(int *reg, Instruction ins);
void Lh(int *reg, Instruction ins);
void Lhu(int *reg, Instruction ins);
void Lb(int *reg, Instruction ins);
void Lbu(int *reg, Instruction ins);
void Sw(int *reg, Instruction ins);
void Sh(int *reg, Instruction ins);
void Sb(int *reg, Instruction ins);
void Lui(int *reg, Instruction ins);
void Andi(int *reg, Instruction ins);
void Ori(int *reg, Instruction ins);
void Nori(int *reg, Instruction ins);
void Slti(int *reg, Instruction ins);
void Beq(int *reg, Instruction ins);
void Bne(int *reg, Instruction ins);

#define OP_J	0x02
#define OP_JAL	0x03
void J(Instruction ins);
void Jal(int *reg, Instruction ins);

#define OP_HALT	0x3F
