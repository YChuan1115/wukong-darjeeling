#include "types.h"
#include "panic.h"
#include "debug.h"
#include "parse_infusion.h"
#include "infusion.h"
#include "wkreprog.h"
#include "asm.h"
#include "opcodes.h"
#include "rtc.h"
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <stddef.h>

// Offsets for static variables in an infusion, relative to the start of infusion->staticReferencesFields
#define offset_for_static_ref(infusion_ptr, variable_index)   ((uint16_t)((void*)(&((infusion)->staticReferenceFields[variable_index])) - (void *)((infusion)->staticReferenceFields)))
#define offset_for_static_byte(infusion_ptr, variable_index)  ((uint16_t)((void*)(&((infusion)->staticByteFields[variable_index]))      - (void *)((infusion)->staticReferenceFields)))
#define offset_for_static_short(infusion_ptr, variable_index) ((uint16_t)((void*)(&((infusion)->staticShortFields[variable_index]))     - (void *)((infusion)->staticReferenceFields)))
#define offset_for_static_int(infusion_ptr, variable_index)   ((uint16_t)((void*)(&((infusion)->staticIntFields[variable_index]))       - (void *)((infusion)->staticReferenceFields)))
#define offset_for_static_long(infusion_ptr, variable_index)  ((uint16_t)((void*)(&((infusion)->staticLonFields[variable_index]))       - (void *)((infusion)->staticReferenceFields)))

uint8_t offset_for_intlocal(dj_di_pointer methodimpl, uint8_t local) {
	return (dj_di_methodImplementation_getReferenceLocalVariableCount(methodimpl) * sizeof(ref_t)) +
		   (local * sizeof(int16_t));
}

uint8_t offset_for_reflocal(dj_di_pointer methodimpl, uint8_t local) {
	return (local * sizeof(ref_t));
}


// avr-libgcc functions used by translation
extern void* __divmodhi4;

// USED AT COMPILE TIME:
const unsigned char PROGMEM __attribute__ ((aligned (SPM_PAGESIZE))) rtc_compiled_code_buffer[RTC_COMPILED_CODE_BUFFER_SIZE] = {};
// Buffer for emitting code.
#define RTC_MAX_SIZE_FOR_SINGLE_JVM_INSTRUCTION 10 // Used to check when we need to flush the buffer (when rtc_codebuffer_position-rtc_codebuffer < RTC_MAX_SIZE_FOR_SINGLE_JVM_INSTRUCTION)
uint16_t *rtc_codebuffer;
uint16_t *rtc_codebuffer_position; // A pointer to somewhere within the buffer


void rtc_flush() {
	uint8_t *instructiondata = (uint8_t *)rtc_codebuffer;
	uint16_t count = rtc_codebuffer_position - rtc_codebuffer;
#ifdef DARJEELING_DEBUG
	for (int i=0; i<count; i++) {
		DEBUG_LOG(DBG_RTC, "[rtc]    %x  (%x %x)\n", rtc_codebuffer[i], instructiondata[i*2], instructiondata[i*2+1]);
	}
#endif // DARJEELING_DEBUG
	// Write to flash
    wkreprog_write(2*count, instructiondata);
    // Buffer is now empty
    rtc_codebuffer_position = rtc_codebuffer;
}

static inline void emit(uint16_t wordopcode) {
	*(rtc_codebuffer_position++) = wordopcode;
}


void rtc_update_method_pointers(dj_infusion *infusion, native_method_function_t *rtc_method_start_addresses) {
	DEBUG_LOG(DBG_RTC, "[rtc] handler list is at %p\n", infusion->native_handlers);
	uint16_t native_handlers_address = (uint16_t)infusion->native_handlers;
	wkreprog_open_raw(native_handlers_address);

	uint16_t number_of_methodimpls = dj_di_parentElement_getListSize(infusion->methodImplementationList);

	for (uint16_t i=0; i<number_of_methodimpls; i++) {
		dj_di_pointer methodimpl = dj_infusion_getMethodImplementation(infusion, i);
		native_method_function_t handler;
		if (dj_di_methodImplementation_getFlags(methodimpl) & FLAGS_NATIVE) {
			// Copy existing pointer
			const DJ_PROGMEM native_method_function_t *native_handlers = infusion->native_handlers;
			handler = native_handlers[i];
			DEBUG_LOG(DBG_RTC, "[rtc] method %d is native, copying native handler: %p\n", i, handler);
		} else {
			// Fill in address of RTC compiled method
			handler = rtc_method_start_addresses[i];
			DEBUG_LOG(DBG_RTC, "[rtc] method %d is not native, filling in address from rtc buffer: %p\n", i, handler);
		}
		wkreprog_write(2, (uint8_t *)&handler);
	}

	wkreprog_close();
}


#define rtc_branch_table_size(methodimpl) (dj_di_methodImplementation_getNumberOfBranchTargets(methodimpl)*SIZEOF_RJMP)
#define rtc_branch_target_table_address(i) (branch_target_table_start_ptr + i*SIZEOF_RJMP)
void rtc_compile_method(dj_di_pointer methodimpl, dj_infusion *infusion) {
	uint8_t jvm_operand_byte0;
	uint8_t jvm_operand_byte1;
	uint16_t jvm_operand_word;
	dj_di_pointer tmp_current_position; // Used to temporarily store the current position when processing brtarget instructions.

	uint16_t branch_target_count = 0; // Keep track of how many branch targets we've seen

	// Buffer to hold the code we're building (want to keep this on the stack so it doesn't take up space at runtime)
	uint16_t codebuffer[16];
	rtc_codebuffer = codebuffer;
	rtc_codebuffer_position = codebuffer;

	// Reserve space for the branch table
	uint16_t branchTableSize = rtc_branch_table_size(methodimpl);
	// Remember the start of the branch table
	dj_di_pointer branch_target_table_start_ptr = wkreprog_get_raw_position();
	DEBUG_LOG(DBG_RTC, "[rtc] Reserving %d bytes for %d branch targets at address %p\n", branchTableSize, dj_di_methodImplementation_getNumberOfBranchTargets(methodimpl), branch_target_table_start_ptr);
	// Skip this number of bytes (actually it doesn't matter what we write here, but I just use the same data so nothing changes)
	wkreprog_write(branchTableSize, (uint8_t *)branch_target_table_start_ptr);

	// prologue (is this the right way?)
	emit( asm_PUSH(R2) );
	emit( asm_PUSH(R3) );
	emit( asm_PUSH(R28) ); // Push Y
	emit( asm_PUSH(R29) );
	emit( asm_MOVW(R28, R24) ); // Pointer to locals in Y
	emit( asm_MOVW(R26, R22) ); // Pointer to ref stack in X
	emit( asm_MOVW(R2, R20) ); // Pointer to static in R2 (will be MOVWed to R30 when necessary)

	// translate the method
	dj_di_pointer code = dj_di_methodImplementation_getData(methodimpl);
	uint16_t method_length = dj_di_methodImplementation_getLength(methodimpl);
	DEBUG_LOG(DBG_RTC, "[rtc] method length %d\n", method_length);

	for (uint16_t pc=0; pc<method_length; pc++) {
		if (rtc_codebuffer_position-rtc_codebuffer < RTC_MAX_SIZE_FOR_SINGLE_JVM_INSTRUCTION) {
			// There may not be enough space in the buffer to hold the current opcode.
			rtc_flush();
		}

		uint8_t opcode = dj_di_getU8(code + pc);
		DEBUG_LOG(DBG_RTC, "[rtc] JVM opcode %d\n", opcode);
		switch (opcode) {
			case JVM_SCONST_0:
			case JVM_SCONST_1:
			case JVM_SCONST_2:
			case JVM_SCONST_3:
			case JVM_SCONST_4:
			case JVM_SCONST_5:
				jvm_operand_byte0 = opcode - JVM_SCONST_0;
				emit( asm_LDI(R24, jvm_operand_byte0) );
				emit( asm_LDI(R25, 0) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_BSPUSH:
				jvm_operand_byte0 = dj_di_getU8(code + ++pc);
				emit( asm_LDI(R24, jvm_operand_byte0) );
				emit( asm_LDI(R25, 0) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_SSPUSH:
				// bytecode is big endian, but I've been pushing LSB first. (maybe change that later)
				jvm_operand_byte0 = dj_di_getU8(code + ++pc);
				jvm_operand_byte1 = dj_di_getU8(code + ++pc);
				emit( asm_LDI(R24, jvm_operand_byte1) );
				emit( asm_LDI(R25, jvm_operand_byte0) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_SLOAD:
			case JVM_SLOAD_0:
			case JVM_SLOAD_1:
			case JVM_SLOAD_2:
			case JVM_SLOAD_3:
				if (opcode == JVM_SLOAD)
					jvm_operand_byte0 = dj_di_getU8(code + ++pc);
				else
					jvm_operand_byte0 = opcode - JVM_SLOAD_0;
				emit( asm_LDD(R24, Y, offset_for_intlocal(methodimpl, jvm_operand_byte0)) );
				emit( asm_LDD(R25, Y, offset_for_intlocal(methodimpl, jvm_operand_byte0)+1) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_ALOAD:
			case JVM_ALOAD_0:
			case JVM_ALOAD_1:
			case JVM_ALOAD_2:
			case JVM_ALOAD_3:
				if (opcode == JVM_ALOAD)
					jvm_operand_byte0 = dj_di_getU8(code + ++pc);
				else
					jvm_operand_byte0 = opcode - JVM_ALOAD_0;
				emit( asm_LDD(R24, Y, offset_for_reflocal(methodimpl, jvm_operand_byte0)) );
				emit( asm_LDD(R25, Y, offset_for_reflocal(methodimpl, jvm_operand_byte0)+1) );
				emit( asm_x_PUSHREF(R24) );
				emit( asm_x_PUSHREF(R25) );
			break;
			case JVM_SSTORE:
			case JVM_SSTORE_0:
			case JVM_SSTORE_1:
			case JVM_SSTORE_2:
			case JVM_SSTORE_3:
				if (opcode == JVM_SSTORE)
					jvm_operand_byte0 = dj_di_getU8(code + ++pc);
				else
					jvm_operand_byte0 = opcode - JVM_SSTORE_0;
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_STD(R24, Y, offset_for_intlocal(methodimpl, jvm_operand_byte0)) );
				emit( asm_STD(R25, Y, offset_for_intlocal(methodimpl, jvm_operand_byte0)+1) );
			break;
			case JVM_ADUP:
				emit( asm_x_POPREF(R25) );
				emit( asm_x_POPREF(R24) );
				emit( asm_x_PUSHREF(R24) );
				emit( asm_x_PUSHREF(R25) );
				emit( asm_x_PUSHREF(R24) );
				emit( asm_x_PUSHREF(R25) );
			break;
			case JVM_GETFIELD_S:
				jvm_operand_word = (dj_di_getU8(code + pc + 1) << 8) | dj_di_getU8(code + pc + 2);
				pc += 2;
				emit( asm_x_POPREF(R31) ); // POP the reference into Z
				emit( asm_x_POPREF(R30) );
				emit( asm_LDD(R24, Z, jvm_operand_word) );
				emit( asm_LDD(R25, Z, jvm_operand_word+1) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_PUTFIELD_S:
				jvm_operand_word = (dj_di_getU8(code + pc + 1) << 8) | dj_di_getU8(code + pc + 2);
				pc += 2;
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_x_POPREF(R31) ); // POP the reference into Z
				emit( asm_x_POPREF(R30) );
				emit( asm_STD(R24, Z, jvm_operand_word) );
				emit( asm_STD(R25, Z, jvm_operand_word+1) );
			break;
			case JVM_GETSTATIC_S:
				jvm_operand_byte0 = dj_di_getU8(code + ++pc); // Get the infusion. Should be 0.
				if (jvm_operand_byte0 != 0) {
					DEBUG_LOG(DBG_RTC, "JVM_GETSTATIC_S only supported within current infusion. infusion=%d pc=%d\n", jvm_operand_byte0, pc);
					dj_panic(DJ_PANIC_UNSUPPORTED_OPCODE);
				}
				jvm_operand_byte0 = dj_di_getU8(code + ++pc); // Get the field.
				emit( asm_MOVW(R30, R2) );
				emit( asm_LDD(R24, Z, offset_for_static_short(infusion, jvm_operand_byte0)) );
				emit( asm_LDD(R25, Z, offset_for_static_short(infusion, jvm_operand_byte0)+1) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_PUTSTATIC_S:
				jvm_operand_byte0 = dj_di_getU8(code + ++pc); // Get the infusion. Should be 0.
				if (jvm_operand_byte0 != 0) {
					DEBUG_LOG(DBG_RTC, "JVM_GETSTATIC_S only supported within current infusion. infusion=%d pc=%d\n", jvm_operand_byte0, pc);
					dj_panic(DJ_PANIC_UNSUPPORTED_OPCODE);
				}
				jvm_operand_byte0 = dj_di_getU8(code + ++pc); // Get the field.
				emit( asm_MOVW(R30, R2) );
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_STD(R24, Z, offset_for_static_short(infusion, jvm_operand_byte0)) );
				emit( asm_STD(R25, Z, offset_for_static_short(infusion, jvm_operand_byte0)+1) );
			break;
			case JVM_SADD:
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_POP(R23) );
				emit( asm_POP(R22) );
				emit( asm_ADD(R24, R22) );
				emit( asm_ADC(R25, R23) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_SSUB:
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_POP(R23) );
				emit( asm_POP(R22) );
				emit( asm_SUB(R22, R24) );
				emit( asm_SBC(R23, R25) );
				emit( asm_PUSH(R22) );
				emit( asm_PUSH(R23) );				
			break;
			case JVM_SMUL:
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_POP(R23) );
				emit( asm_POP(R22) );

				// Code generated by avr-gcc -mmcu=atmega2560 -O3
		        // mul r24,r22
		        // movw r18,r0
		        // mul r24,r23
		        // add r19,r0
		        // mul r25,r22
		        // add r19,r0
		        // clr r1
		        // movw r24,r18
		        // ret

				emit( asm_MUL(R24, R22) );
				emit( asm_MOVW(R18, R0) );
				emit( asm_MUL(R24, R23) );
				emit( asm_ADD(R19, R0) );
				emit( asm_MUL(R25, R22) );
				emit( asm_ADD(R19, R0) );
				// gcc generates "clr r1" here, but it doesn't seem necessary?
				emit( asm_MOVW(R24, R18) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_SDIV:
			case JVM_SREM:
				emit( asm_POP(R23) );
				emit( asm_POP(R22) );
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_CALL1((uint16_t)&__divmodhi4) );
				emit( asm_CALL2((uint16_t)&__divmodhi4) );
				if (opcode == JVM_SDIV) {
					emit( asm_PUSH(R22) );
					emit( asm_PUSH(R23) );
				} else { // JVM_SREM
					emit( asm_PUSH(R24) );
					emit( asm_PUSH(R25) );
				}
			break;
			case JVM_SNEG:
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_CLR(R23) );
				emit( asm_CLR(R22) );
				emit( asm_SUB(R22, R24) );
				emit( asm_SBC(R23, R25) );
				emit( asm_PUSH(R22) );
				emit( asm_PUSH(R23) );
			break;
			case JVM_SAND:
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_POP(R23) );
				emit( asm_POP(R22) );
				emit( asm_AND(R24, R22) );
				emit( asm_AND(R25, R23) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_SOR:
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_POP(R23) );
				emit( asm_POP(R22) );
				emit( asm_OR(R24, R22) );
				emit( asm_OR(R25, R23) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_SXOR:
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_POP(R23) );
				emit( asm_POP(R22) );
				emit( asm_EOR(R24, R22) );
				emit( asm_EOR(R25, R23) );
				emit( asm_PUSH(R24) );
				emit( asm_PUSH(R25) );
			break;
			case JVM_IF_SCMPEQ:
			case JVM_IF_SCMPNE:
			case JVM_IF_SCMPLT:
			case JVM_IF_SCMPGE:
			case JVM_IF_SCMPGT:
			case JVM_IF_SCMPLE:
				// Branch instructions first have a bytecode offset, used by the interpreter,
				// followed by a branch target index used when compiling to native code.
				jvm_operand_word = (dj_di_getU8(code + pc + 3) << 8) | dj_di_getU8(code + pc + 4);
				pc += 4;

				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_POP(R23) );
				emit( asm_POP(R22) );
				// Do the complementary branch. Not taking a branch means jumping over the unconditional branch to the branch target table
				if (opcode == JVM_IF_SCMPEQ) {
					emit( asm_CP(R22, R24) );
					emit( asm_CPC(R23, R25) );
					emit( asm_BRNE(SIZEOF_RJMP) );
				} else if (opcode == JVM_IF_SCMPNE) {
					emit( asm_CP(R22, R24) );
					emit( asm_CPC(R23, R25) );
					emit( asm_BREQ(SIZEOF_RJMP) );
				} else if (opcode == JVM_IF_SCMPLT) {
					emit( asm_CP(R22, R24) );
					emit( asm_CPC(R23, R25) );
					emit( asm_BRGE(SIZEOF_RJMP) );
				} else if (opcode == JVM_IF_SCMPGE) {
					emit( asm_CP(R22, R24) );
					emit( asm_CPC(R23, R25) );
					emit( asm_BRLT(SIZEOF_RJMP) );
				} else if (opcode == JVM_IF_SCMPGT) {
					emit( asm_CP(R24, R22) );
					emit( asm_CPC(R25, R23) );
					emit( asm_BRGE(SIZEOF_RJMP) );
				} else if (opcode == JVM_IF_SCMPLE) {
					emit( asm_CP(R24, R22) );
					emit( asm_CPC(R25, R23) );
					emit( asm_BRLT(SIZEOF_RJMP) );
				}
				rtc_flush(); // To make sure wkreprog_get_raw_position returns the right value;
				emit( asm_RJMP(rtc_branch_target_table_address(jvm_operand_word) - wkreprog_get_raw_position() - 2) ); // -2 is because RJMP will add 1 WORD to the PC in addition to the jump offset
			break;
			case JVM_GOTO:
				// Branch instructions first have a bytecode offset, used by the interpreter,
				// followed by a branch target index used when compiling to native code.
				jvm_operand_word = (dj_di_getU8(code + pc + 3) << 8) | dj_di_getU8(code + pc + 4);
				pc += 4;

				rtc_flush(); // To make sure wkreprog_get_raw_position returns the right value;
				emit( asm_RJMP(rtc_branch_target_table_address(jvm_operand_word) - wkreprog_get_raw_position() - 2) ); // -2 is because RJMP will add 1 WORD to the PC in addition to the jump offset
			break;
			case JVM_SRETURN:
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );

				// epilogue (is this the right way?)
				emit( asm_POP(R29) ); // Pop Y
				emit( asm_POP(R28) );
				emit( asm_POP(R3) );
				emit( asm_POP(R2) );
				emit( asm_RET );
			break;
			case JVM_IRETURN:
				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				emit( asm_POP(R23) );
				emit( asm_POP(R22) );

				// epilogue (is this the right way?)
				emit( asm_POP(R29) ); // Pop Y
				emit( asm_POP(R28) );
				emit( asm_POP(R3) );
				emit( asm_POP(R2) );
				emit( asm_RET );
			break;
			case JVM_RETURN:
				// epilogue (is this the right way?)
				emit( asm_POP(R29) ); // Pop Y
				emit( asm_POP(R28) );
				emit( asm_POP(R3) );
				emit( asm_POP(R2) );
				emit( asm_RET );
			break;
			// BRANCHES
			case JVM_SIFEQ:
			case JVM_SIFNE:
			case JVM_SIFLT:
			case JVM_SIFGE:
			case JVM_SIFGT:
			case JVM_SIFLE:
				// Branch instructions first have a bytecode offset, used by the interpreter,
				// followed by a branch target index used when compiling to native code.
				jvm_operand_word = (dj_di_getU8(code + pc + 3) << 8) | dj_di_getU8(code + pc + 4);
				pc += 4;

				emit( asm_POP(R25) );
				emit( asm_POP(R24) );
				// Do the complementary branch. Not taking a branch means jumping over the unconditional branch to the branch target table
				if (opcode == JVM_SIFEQ) {
					emit( asm_OR(R24, R25) );
					emit( asm_BRNE(SIZEOF_RJMP) );
				} else if (opcode == JVM_SIFNE) {
					emit( asm_OR(R24, R25) );
					emit( asm_BREQ(SIZEOF_RJMP) );
				} else if (opcode == JVM_SIFLT) {
					emit( asm_SBRC(R25, 7) ); // value is >0 if the highest bit is cleared
				} else if (opcode == JVM_SIFGE) {
					emit( asm_SBRS(R25, 7) ); // value is <0 if the highest bit is set
				} else if (opcode == JVM_SIFGT) {
					emit( asm_CP(ZERO_REG, R24) );
					emit( asm_CPC(ZERO_REG, R25) );
					emit( asm_BRGE(SIZEOF_RJMP) ); // if (0 >= x), then NOT (x > 0)
				} else if (opcode == JVM_SIFLE) {
					emit( asm_CP(ZERO_REG, R24) );
					emit( asm_CPC(ZERO_REG, R25) );
					emit( asm_BRLT(SIZEOF_RJMP) ); // if (0 < x), then NOT (x <= 0)
				}

				rtc_flush(); // To make sure wkreprog_get_raw_position returns the right value;
				emit( asm_RJMP(rtc_branch_target_table_address(jvm_operand_word) - wkreprog_get_raw_position() - 2) ); // -2 is because RJMP will add 1 WORD to the PC in addition to the jump offset
			break;
			case JVM_BRTARGET:
				// This is a noop, but we need to record the address of the next instruction
				// in the branch table as a RJMP instruction.
				tmp_current_position = wkreprog_get_raw_position();
				rtc_flush(); // Not strictly necessary at the moment
				wkreprog_close();
				wkreprog_open_raw(rtc_branch_target_table_address(branch_target_count));
				emit( asm_RJMP(tmp_current_position - rtc_branch_target_table_address(branch_target_count) - 2) ); // Relative jump to tmp_current_position from the branch target table. -2 is because RJMP will add 1 WORD to the PC in addition to the jump offset
				rtc_flush();
				wkreprog_close();
				wkreprog_open_raw(tmp_current_position);
				branch_target_count++;
			break;

			// Not implemented
			default:
				DEBUG_LOG(DBG_RTC, "Unimplemented Java opcode %d at pc=%d\n", opcode, pc);
				dj_panic(DJ_PANIC_UNSUPPORTED_OPCODE);
			break;
		}
		// For now, flush after each opcode
		rtc_flush();
	}
}

void rtc_compile_lib(dj_infusion *infusion) {
	// uses 512bytes on the stack... maybe optimise this later
	native_method_function_t rtc_method_start_addresses[256];
	for (uint16_t i=0; i<256; i++)
		rtc_method_start_addresses[i] = 0;

	wkreprog_open_raw((dj_di_pointer)rtc_compiled_code_buffer);

	uint16_t number_of_methodimpls = dj_di_parentElement_getListSize(infusion->methodImplementationList);
	DEBUG_LOG(DBG_RTC, "[rtc] infusion contains %d methods\n", number_of_methodimpls);

	const DJ_PROGMEM native_method_function_t *handlers = infusion->native_handlers;
	DEBUG_LOG(DBG_RTC, "[rtc] handler list is at %p\n", infusion->native_handlers);
	for (uint16_t i=0; i<number_of_methodimpls; i++) {		
		DEBUG_LOG(DBG_RTC, "[rtc] (compile) pointer for method %i %p\n", i, infusion->native_handlers[i]);	

		dj_di_pointer methodimpl = dj_infusion_getMethodImplementation(infusion, i);
		if (dj_di_methodImplementation_getFlags(methodimpl) & FLAGS_NATIVE) {
			DEBUG_LOG(DBG_RTC, "[rtc] skipping native method %d\n", i);
			continue;
		}

		if (handlers[i] != NULL) {
			DEBUG_LOG(DBG_RTC, "[rtc] should skip already compiled method %d with pointer %p, but won't for now\n", i, handlers[i]);
			// continue; // Skip native or already rtc compiled methods
		}

		// TMPRTC
		if (i==0) {
			DEBUG_LOG(DBG_RTC, "[rtc] skipping method 0 for now\n", i);
			continue;
		}
		
		DEBUG_LOG(DBG_RTC, "[rtc] compiling method %d\n", i);

		// store the starting address for this method;
		// IMPORTANT!!!! the PC in AVR stores WORD addresses, so we need to divide the address
		// of a function by 2 in order to get a function pointer!
		dj_di_pointer method_address = wkreprog_get_raw_position() + rtc_branch_table_size(methodimpl);
		rtc_method_start_addresses[i] = (native_method_function_t)(method_address/2);

		rtc_compile_method(methodimpl, infusion);
	}

	wkreprog_close();

	// At this point, the addresses in the rtc_method_start_addresses are 0
	// for the native methods, while the handler table is 0 for the java methods.
	// We need to fill in the addresses in rtc_method_start_addresses in the
	// empty slots in the handler table.
	rtc_update_method_pointers(infusion, rtc_method_start_addresses);

	// Mark the infusion as translated (how?)
}

