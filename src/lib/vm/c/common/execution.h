/*
 * execution.h
 * 
 * Copyright (c) 2008-2010 CSIRO, Delft University of Technology.
 * 
 * This file is part of Darjeeling.
 * 
 * Darjeeling is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Darjeeling is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with Darjeeling.  If not, see <http://www.gnu.org/licenses/>.
 */
 

#ifndef __execution__
#define __execution__

#include "types.h"
#include "vmthread.h"
#include "vm.h"
#include "object.h"

#include "config.h"

// Exported for RTC only
extern int16_t *intStack;
extern ref_t *refStack;
void callMethod(dj_global_id methodImplId, int virtualCall);
// End Exported for RTC only

int dj_exec_run();
void dj_exec_breakExecution();
void dj_exec_activate_thread(dj_thread *thread);
void dj_exec_deactivateThread(dj_thread *thread);

void dj_exec_throw(dj_object *obj, uint16_t throw_pc);
void dj_exec_throwHere(dj_object *obj);
void dj_exec_createAndThrow(int exceptionId);

uint16_t dj_exec_getNumberOfObjectsOnReferenceStack();

void dj_exec_stackPushShort(int16_t value);
void dj_exec_stackPushInt(int32_t value);
void dj_exec_stackPushLong(int64_t value);
void dj_exec_stackPushRef(ref_t value);

int16_t dj_exec_stackPopShort();
int32_t dj_exec_stackPopInt();
int64_t dj_exec_stackPopLong();
ref_t dj_exec_stackPopRef();

int16_t dj_exec_stackPeekShort();
int32_t dj_exec_stackPeekInt();
ref_t dj_exec_stackPeekRef();
ref_t dj_exec_stackPeekDeepRef(int depth);

dj_thread *dj_exec_getCurrentThread();
dj_infusion *dj_exec_getCurrentInfusion();

void dj_exec_setVM(dj_vm *_vm);
dj_vm *dj_exec_getVM();

void dj_exec_updatePointers();

dj_frame *dj_exec_getCurrentFrame();
bool dj_exec_currentMethodIsRTCCompiled();

#ifdef DARJEELING_DEBUG_FRAME
void dj_exec_dumpFrame( dj_frame *frame );
void dj_exec_dumpFrameTrace( dj_frame *frame );
dj_frame *dj_exec_dumpExecutionState();	// Returns current frame
void dj_exec_debugCurrentFrame();
void dj_exec_debugFrameTrace();
#endif

#endif
