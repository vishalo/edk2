/** @file

Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
Portions copyright (c) 2011, Apple Inc. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Protocol/MpService.h>

#include <Library/BaseLib.h>
#include <Guid/ArmMpCoreInfo.h>

#define AP_STACK_SIZE  0x1000

//
// Internal Data Structures
//

//
// AP state
//
// The state transitions for an AP when it processes a procedure are:
//  Idle ----> Ready ----> Busy ----> Finished ----> Idle
//       [BSP]       [BSP]      [AP]           [BSP]
//
typedef enum {
  CpuStateIdle,
  CpuStateReady,
  CpuStateBlocked,
  CpuStateBusy,
  CpuStateFinished,
  CpuStateDisabled
} CPU_STATE;

//
// Define Individual Processor Data block.
//
typedef struct {
  EFI_PROCESSOR_INFORMATION    Info;
  EFI_AP_PROCEDURE             Procedure;
  VOID                         *Parameter;
  CPU_STATE                    State;
  EFI_EVENT                    CheckThisAPEvent;
  EFI_EVENT                    WaitEvent;
  UINTN                        Timeout;
  UINTN                        TimeTaken;
  BOOLEAN                      TimeoutActive;
  BOOLEAN                      *SingleApFinished;
} CPU_AP_DATA;

//
// Define MP data block which consumes individual processor block.
//
typedef struct {
  UINTN               NumberOfProcessors;
  UINTN               NumberOfEnabledProcessors;
  EFI_EVENT           CheckAllAPsEvent;
  EFI_EVENT           AllWaitEvent;
  UINTN               FinishCount;
  UINTN               StartCount;
  EFI_AP_PROCEDURE    Procedure;
  VOID                *ProcedureArgument;
  BOOLEAN             SingleThread;
  UINTN               StartedNumber;
  CPU_AP_DATA         *CpuData;
  UINTN               *FailedList;
  UINTN               FailedListIndex;
  UINTN               AllTimeout;
  UINTN               AllTimeTaken;
  BOOLEAN             AllTimeoutActive;
} CPU_MP_DATA;

extern CPU_MP_DATA   mCpuMpData;
extern UINT64       *gApStacksBase;
extern UINT64       *gProcessorIDs;
extern CONST UINT64 gApStackSize;
extern VOID         *gTtbr0;
extern UINTN        gTcr;
extern UINTN        gMair;
extern UINTN        gHcr;

/** Turns on the specified core using PSCI and dispatches it to ApEntryPoint.

  @param[in] ProcessorIndex  The index of the processor to turn on.

  @retval EFI_SUCCESS        Success.
  @retval EFI_NOT_READY      The processor is already on.
  @retval EFI_DEVICE_ERROR   The processor could not be turned on.

**/
EFI_STATUS
EFIAPI
DispatchCpu (
  IN UINTN  ProcessorIndex
  );

/**
  Allocates and initializes the MP data shared by the DXE and PEI instances.

  @param[in] NumberOfProcessors  The number of processors described by CoreInfo.
  @param[in] CoreInfo            The MPIDR of each processor, from the ARM MP core info HOB.

  @retval EFI_SUCCESS            The MP data was initialized.
  @retval EFI_OUT_OF_RESOURCES   Memory for the MP data could not be allocated.

**/
EFI_STATUS
InitializeMpCommonData (
  IN UINTN                NumberOfProcessors,
  IN CONST ARM_CORE_INFO  *CoreInfo
  );

/** Secondary core entry point.

**/
VOID
ApEntryPoint (
  VOID
  );

/** C entry-point for the AP.
    This function gets called from the assembly function ApEntryPoint.
**/
VOID
ApProcedure (
  VOID
  );
