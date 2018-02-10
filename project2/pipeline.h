#define R_TYPE 		0x00
#define SIGN_BIT	0x80000000
#define NOP			0x00000000

#define Reg_Dst		0x40	// 0100,0000
#define ALU_Src		0x20	// 0010,0000
#define Mem_To_Reg	0x10	// 0001,0000
#define Reg_Write	0x08	// 0000,1000
#define Mem_Read	0x04	// 0000,0100
#define Add_NOP		0x02	// 0000,0010
#define Add_Stall	0x01	// 0000,0001

#define Forward_NONE	0
#define Forward_WB		1
#define Forward_MEM		2
#define Forward_EX		3

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
	unsigned int PC;
	Instruction Ins;
	int reg_rs, reg_rt;		// Result of ID stage
	int ALU_Result;			// Result of EX stage
	char rd;				// Register be writen in WB stage
	int Memory_dada;		// Data read in MEM stage
	unsigned char Control;	// Control signals for EX, MEM, WB
	
	void Flush() {
		Ins.value = NOP;
		Control = 0x00;
	}
}PipelineReg;

enum PipelineRegType {
	IF_to_ID,
	ID_to_EX,
	EX_to_MEM,
	MEM_to_WB
};
const int iMemorySize = 1024;	// bytes
const int dMemorySize = 1024;	// bytes
const int NumOfGPRegs = 32;		// 32 general purpose registers on MIPS

unsigned int PC, Next_PC;								// Program Counter
int registers[NumOfGPRegs];								// Registers, each has 32 bits
PipelineReg pipelineRegs[4];							// Pipeline registers, 4 for each stage
unsigned int iMemory[ iMemorySize/sizeof(int) ];		// One entry contains 4 bytes, so size of iMemory[] is (1K bytes / 4 bytes) 
unsigned int dMemory[ dMemorySize/sizeof(int) ];		// One entry contains 4 bytes, so size of dMemory[] is (1K bytes / 4 bytes) 
unsigned int iMemoryLen, dMemoryLen;					// Length of iMemory (dMemory)
unsigned int inputImage[ iMemorySize/sizeof(int) +2];	// Store image file's, anthoer 2 entry is store PC (sp) , MemLen for iMem (dMem).
int cycle;											
int machine_halt;										// Flag to halt the machine
char PC_Src;
char rd_for_WB;
unsigned char Control_for_WB;
char ForwardA_ID, ForwardB_ID; 
char ForwardA_EX, ForwardB_EX;

enum ByteOrder {
	Little_Endian,
	Big_Endian
};
int byteOrderChecker(void);		// Check machine's byte order
void Machine_Init();			// Initial machine's regiters, memory.
int openImage(char*);			// Load image file.
void ImageToMemory(unsigned int*, unsigned int*, unsigned int*, int);
void Output_Ins(Instruction ins);
void Output_Stages(Instruction ins, PipelineReg *pReg);
void Output_Status();
FILE *outfile = fopen("snapshot.rpt", "w");

void Hazard_Detect_unit(PipelineReg *pReg);
void Forwarding_unit_ID(PipelineReg *pReg);
void Forwarding_unit_EX(PipelineReg *pReg);
void Control_unit(PipelineReg *pReg);
int ALU_unit(int busA, int busB, PipelineReg *pReg);
void ID(int *reg, PipelineReg *pReg);
void EX(int *reg, PipelineReg *pReg);
void DM(PipelineReg *pReg);
void WB(int *reg, PipelineReg *pReg);

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
void Jr(int *reg, PipelineReg *pReg);

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
void Lw(PipelineReg *pReg);
void Lh(PipelineReg *pReg);
void Lhu(PipelineReg *pReg);
void Lb(PipelineReg *pReg);
void Lbu(PipelineReg *pReg);
void Sw(PipelineReg *pReg);
void Sh(PipelineReg *pReg);
void Sb(PipelineReg *pReg);
void Beq(int reg_Rs, int reg_Rt, PipelineReg *pReg);
void Bne(int reg_Rs, int reg_Rt, PipelineReg *pReg);

#define OP_J	0x02
#define OP_JAL	0x03
void J(PipelineReg *pReg);

#define OP_HALT	0x3F
