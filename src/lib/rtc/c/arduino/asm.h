#define R0        0
#define R1        1
#define ZERO_REG  1
#define R2        2
#define R3        3
#define R4        4
#define R5        5
#define R6        6
#define R7        7
#define R8        8
#define R9        9
#define R10      10
#define R11      11
#define R12      12
#define R13      13
#define R14      14
#define R15      15
#define R16      16
#define R17      17
#define R18      18
#define R19      19
#define R20      20
#define R21      21
#define R22      22
#define R23      23
#define R24      24
#define R25      25
#define R26      26
#define R27      27
#define R28      28
#define R29      29
#define R30      30
#define R31      31

#define Y  1
#define Z  0 



// 6 bit offset q has to be inserted in the opcode like this:
// 00q0 qq00 0000 0qqq
#define makeLDDSTDoffset(offset) ( \
               ((offset) & 0x07) \
            + (((offset) & 0x18) << 7) \
            + (((offset) & 0x20) << 8))

// 0000 00kk kkkk k000
#define makeBranchOffset(offset) ( \
                (offset) << 3)


// 0000 KKKK 0000 KKKK
#define makeLDIconstant(constant) ( \
               ((constant) & 0x0F) \
            + (((constant) & 0xF0) << 4))

// 0000 00r0 0000 rrrr, with d=dest register, r=source register
#define makeSourceRegister(src_register) ( \
               ((src_register) & 0x0F) \
            + (((src_register) & 0x10) << 5))

#define opcodeWithSingleRegOperand(opcode, reg)                 ((opcode) + ((reg) << 4))
#define opcodeWithSrcAndDestRegOperand(opcode, destreg, srcreg) ((opcode) + ((destreg) << 4) + makeSourceRegister(srcreg))



// ADC                                  0001 11rd dddd rrrr, with d=dest register, r=source register
#define OPCODE_ADC                      0x1C00
#define asm_ADC(destreg, srcreg)        opcodeWithSrcAndDestRegOperand(OPCODE_ADC, destreg, srcreg)

// ADD                                  0000 11rd dddd rrrr, with d=dest register, r=source register
#define OPCODE_ADD                      0x0C00
#define asm_ADD(destreg, srcreg)        opcodeWithSrcAndDestRegOperand(OPCODE_ADD, destreg, srcreg)

// AND                                  0010 00rd dddd rrrr, with d=dest register, r=source register
#define OPCODE_AND                      0x2000
#define asm_AND(destreg, srcreg)        opcodeWithSrcAndDestRegOperand(OPCODE_AND, destreg, srcreg)

// BREQ                                 1111 00kk kkkk k001, with k the signed offset to jump to, in WORDS, not bytes. If taken: PC <- PC + k + 1, if not taken: PC <- PC + 1
#define OPCODE_BREQ                     0xF001
#define asm_BREQ(offset)                (OPCODE_BREQ + makeBranchOffset(((offset)/2)))

// BRGE                                 1111 01kk kkkk k100, with k the signed offset to jump to, in WORDS, not bytes. If taken: PC <- PC + k + 1, if not taken: PC <- PC + 1
#define OPCODE_BRGE                     0xF404
#define asm_BRGE(offset)                (OPCODE_BRGE + makeBranchOffset(((offset)/2)))

// BRLT                                 1111 00kk kkkk k100, with k the signed offset to jump to, in WORDS, not bytes. If taken: PC <- PC + k + 1, if not taken: PC <- PC + 1
#define OPCODE_BRLT                     0xF004
#define asm_BRLT(offset)                (OPCODE_BRLT + makeBranchOffset(((offset)/2)))

// BRNE                                 1111 01kk kkkk k001, with k the signed offset to jump to, in WORDS, not bytes. If taken: PC <- PC + k + 1, if not taken: PC <- PC + 1
#define OPCODE_BRNE                     0xF401
#define asm_BRNE(offset)                (OPCODE_BRNE + makeBranchOffset(((offset)/2)))

// CALL                                 1001 010k kkkk 111k
//                                      kkkk kkkk kkkk kkkk
// TODO: support addresses > 128K
#define OPCODE_CALL                     0x940E
#define asm_CALL1(address)              OPCODE_CALL
#define asm_CALL2(address)              (address/2)

// CLR
#define asm_CLR(destreg)                asm_EOR(destreg, destreg)

// CP                                   0001 01rd dddd rrrr, with r,d=the registers to compare
#define OPCODE_CP                       0x1400
#define asm_CP(destreg, srcreg)         opcodeWithSrcAndDestRegOperand(OPCODE_CP, destreg, srcreg)

// CPC                                  0000 01rd dddd rrrr, with r,d=the registers to compare
#define OPCODE_CPC                      0x0400
#define asm_CPC(destreg, srcreg)        opcodeWithSrcAndDestRegOperand(OPCODE_CPC, destreg, srcreg)

// EOR                                  0010 01rd dddd rrrr, with d=dest register, r=source register
#define OPCODE_EOR                      0x2400
#define asm_EOR(destreg, srcreg)        opcodeWithSrcAndDestRegOperand(OPCODE_EOR, destreg, srcreg)

// LD Rd, X+                            1001 000d dddd 1101
#define OPCODE_LD_XINC                  0x900D
#define asm_LD_XINC(reg)                opcodeWithSingleRegOperand(OPCODE_LD_XINC, reg)

// LDD                                  10q0 qq0d dddd yqqq, with d=dest register, q=offset from Y or Z, y=1 for Y 0 for Z
#define OPCODE_LDD                      0x8000
#define asm_LDD(reg, xy, offset)        (OPCODE_LDD \
                                         + ((reg) << 4) \
                                         + ((xy) << 3) \
                                         + makeLDDSTDoffset(offset))

// LDI                                  1110 KKKK dddd KKKK, with K=constant to load, d=dest register-16 (can only load to r16-r31)
#define OPCODE_LDI                      0xE000
#define asm_LDI(reg, constant)          (OPCODE_LDI \
                                         + (((reg) - 16) << 4) \
                                         + makeLDIconstant(constant))

// MOVW                                 0000 0001 dddd rrrr, with d=dest register/2, r=source register/2
#define OPCODE_MOVW                     0x0100
#define asm_MOVW(destreg, srcreg)       opcodeWithSrcAndDestRegOperand(OPCODE_MOVW, (destreg/2), (srcreg/2))

// MUL                                  1001 11rd dddd rrrr, with d=dest register, r=source register
#define OPCODE_MUL                      0x9C00
#define asm_MUL(destreg, srcreg)        opcodeWithSrcAndDestRegOperand(OPCODE_MUL, destreg, srcreg)

// OR                                   0010 10rd dddd rrrr, with d=dest register, r=source register
#define OPCODE_OR                       0x2800
#define asm_OR(destreg, srcreg)         opcodeWithSrcAndDestRegOperand(OPCODE_OR, destreg, srcreg)

// PUSH 	                            1001 001d dddd 1111, with d=source register
#define OPCODE_PUSH		                0x920F
#define asm_PUSH(reg)                   opcodeWithSingleRegOperand(OPCODE_PUSH, reg)

// PUSHREF
#define asm_x_PUSHREF(reg)              asm_ST_DECX(reg)

// POP  	                            1001 000d dddd 1111
#define OPCODE_POP		                0x900F
#define asm_POP(reg)                    opcodeWithSingleRegOperand(OPCODE_POP, reg)

// POPREF
#define asm_x_POPREF(reg)               asm_LD_XINC(reg)

// RET                                  1001 0101 0000 1000
#define OPCODE_RET                      0x9508
#define asm_RET                         OPCODE_RET

// RJMP                                 1100 kkkk kkkk kkkk, with k the signed offset to jump to, in WORDS, not bytes. PC <- PC + k + 1
#define SIZEOF_RJMP                     2
#define OPCODE_RJMP                     0xC000
#define asm_RJMP(offset)                (OPCODE_RJMP + (((offset)/2) & 0xFFF))

// SBC                                  0000 10rd dddd rrrr, with d=dest register, r=source register
#define OPCODE_SBC                      0x0800
#define asm_SBC(destreg, srcreg)        opcodeWithSrcAndDestRegOperand(OPCODE_SBC, destreg, srcreg)

// SBRC                                 1111 110r rrrr 0bbb, with r=a register and b=the bit to test
#define OPCODE_SBRC                     0xFC00
#define asm_SBRC(reg, bit)              (OPCODE_SBRC + (reg << 4) + bit)

// SBRS                                 1111 111r rrrr 0bbb, with r=a register and b=the bit to test
#define OPCODE_SBRS                     0xFE00
#define asm_SBRS(reg, bit)              (OPCODE_SBRS + (reg << 4) + bit)

// ST Rs, -X                            1001 001r rrrr 1110, with r=the register to store
#define OPCODE_ST_DECX                  0x920E
#define asm_ST_DECX(reg)                (OPCODE_ST_DECX + (reg << 4))

// STD                                  10q0 qq1r rrrr yqqq, with r=source register, q=offset from Y or Z, y=1 for Y 0 for Z
#define OPCODE_STD                      0x8200
#define asm_STD(reg, xy, offset)        (OPCODE_STD \
                                         + ((reg) << 4) \
                                         + ((xy) << 3) \
                                         + makeLDDSTDoffset(offset))

// SUB                                  0001 10rd dddd rrrr, with d=dest register, r=source register
#define OPCODE_SUB                      0x1800
#define asm_SUB(destreg, srcreg)        opcodeWithSrcAndDestRegOperand(OPCODE_SUB, destreg, srcreg)
