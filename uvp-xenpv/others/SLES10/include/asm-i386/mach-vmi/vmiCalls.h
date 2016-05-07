/*
 * vmiCalls.h --
 *
 *   List of 32-bit VMI interface calls and parameters
 *
 *   Copyright (C) 2005, VMware, Inc.
 *
 *   If a new VMI call is added at the end of the list, the VMI API minor
 *   revision must be incremented.  No calls can be deleted or rearranged
 *   without incrementing the major revision number.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for more
 *   details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Send feedback to zach@vmware.com
 *
 */

#define VMI_CALLS \
   VDEF(CPUID) \
   VDEF(WRMSR) \
   VDEF(RDMSR) \
   VDEF(SetGDT) \
   VDEF(SetLDT) \
   VDEF(SetIDT) \
   VDEF(SetTR) \
   VDEF(GetGDT) \
   VDEF(GetLDT) \
   VDEF(GetIDT) \
   VDEF(GetTR) \
   VDEF(WriteGDTEntry) \
   VDEF(WriteLDTEntry) \
   VDEF(WriteIDTEntry) \
   VDEF(UpdateKernelStack) \
   VDEF(SetCR0) \
   VDEF(SetCR2) \
   VDEF(SetCR3) \
   VDEF(SetCR4) \
   VDEF(GetCR0) \
   VDEF(GetCR2) \
   VDEF(GetCR3) \
   VDEF(GetCR4) \
   VDEF(WBINVD) \
   VDEF(SetDR) \
   VDEF(GetDR) \
   VDEF(RDPMC) \
   VDEF(RDTSC) \
   VDEF(CLTS) \
   VDEF(EnableInterrupts) \
   VDEF(DisableInterrupts) \
   VDEF(GetInterruptMask) \
   VDEF(SetInterruptMask) \
   VDEF(IRET) \
   VDEF(SYSEXIT) \
   VDEF(Halt) \
   VDEF(Reboot) \
   VDEF(Shutdown) \
   VDEF(SetPxE) \
   VDEF(SetPxELong) \
   VDEF(UpdatePxE) \
   VDEF(UpdatePxELong) \
   VDEF(MachineToPhysical) \
   VDEF(PhysicalToMachine) \
   VDEF(AllocatePage) \
   VDEF(ReleasePage) \
   VDEF(InvalPage) \
   VDEF(FlushTLB) \
   VDEF(SetLinearMapping) \
   VDEF(INL) \
   VDEF(INB) \
   VDEF(INW) \
   VDEF(INSL) \
   VDEF(INSB) \
   VDEF(INSW) \
   VDEF(OUTL) \
   VDEF(OUTB) \
   VDEF(OUTW) \
   VDEF(OUTSL) \
   VDEF(OUTSB) \
   VDEF(OUTSW) \
   VDEF(SetIOPLMask) \
   VDEF(SetInitialAPState) \
   VDEF(APICWrite) \
   VDEF(APICRead) \
   VDEF(IODelay) \
   VDEF(GetCycleFrequency) \
   VDEF(GetCycleCounter) \
   VDEF(SetAlarm) \
   VDEF(CancelAlarm) \
   VDEF(GetWallclockTime) \
   VDEF(WallclockUpdated) \
   VDEF(SetCallbacks) \
   VDEF(SetLazyMode)
