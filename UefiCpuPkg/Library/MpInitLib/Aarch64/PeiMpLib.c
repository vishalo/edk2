/** @file
  MP Initialize Library implementation for AArch64 PSCI platforms, PEI phase.

  Secondary cores are powered on with PSCI and run the user-supplied procedure
  until it returns. PEI has no events or timers, so only blocking mode is
  supported. Timeouts are measured with the ARM generic timer.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MpInitLib.h>
#include <Library/TimerLib.h>
#include <Guid/ArmMpCoreInfo.h>

#include "MpLib.h"

#define MICROSECONDS_PER_SECOND  1000000

/**
  Converts a timeout in microseconds to generic timer ticks.

  @param[in] Microseconds  The timeout in microseconds. Zero means no timeout.

  @return The number of timer ticks in the timeout.

**/
STATIC
UINT64
MicrosecondsToTicks (
  IN UINTN  Microseconds
  )
{
  return DivU64x32 (
           MultU64x32 (Microseconds, (UINT32)GetPerformanceCounterProperties (NULL, NULL)),
           MICROSECONDS_PER_SECOND
           );
}

/**
  Returns whether the specified processor is the BSP.

  @param[in] ProcessorIndex  The index of the processor to check.

  @return TRUE if the processor is the BSP, FALSE otherwise.

**/
STATIC
BOOLEAN
IsProcessorBSP (
  IN UINTN  ProcessorIndex
  )
{
  return (mCpuMpData.CpuData[ProcessorIndex].Info.StatusFlag & PROCESSOR_AS_BSP_BIT) != 0;
}

/**
  Returns whether the specified processor is enabled.

  @param[in] ProcessorIndex  The index of the processor to check.

  @return TRUE if the processor is enabled, FALSE otherwise.

**/
STATIC
BOOLEAN
IsProcessorEnabled (
  IN UINTN  ProcessorIndex
  )
{
  return (mCpuMpData.CpuData[ProcessorIndex].Info.StatusFlag & PROCESSOR_ENABLED_BIT) != 0;
}

/**
  Returns whether the processor executing this function is the BSP.

  @return TRUE if the current processor is the BSP, FALSE otherwise.

**/
STATIC
BOOLEAN
IsCurrentProcessorBSP (
  VOID
  )
{
  UINTN  ProcessorIndex;

  if (EFI_ERROR (MpInitLibWhoAmI (&ProcessorIndex))) {
    return FALSE;
  }

  return IsProcessorBSP (ProcessorIndex);
}

/**
  Sets the procedure an AP runs and dispatches the AP.

  @param[in] ProcessorIndex         The index of the AP to run the procedure.
  @param[in] Procedure              The procedure to run on the AP.
  @param[in] ProcedureArgument      The argument passed to Procedure.

  @retval EFI_SUCCESS               The AP was dispatched.
  @retval EFI_NOT_READY             The AP is already running.
  @retval EFI_DEVICE_ERROR          The AP could not be powered on.

**/
STATIC
EFI_STATUS
StartAp (
  IN UINTN            ProcessorIndex,
  IN EFI_AP_PROCEDURE Procedure,
  IN VOID             *ProcedureArgument
  )
{
  mCpuMpData.CpuData[ProcessorIndex].Procedure = Procedure;
  mCpuMpData.CpuData[ProcessorIndex].Parameter = ProcedureArgument;
  ArmDataMemoryBarrier ();

  return DispatchCpu (ProcessorIndex);
}

/**
  Waits until the AP finishes its procedure or the deadline passes.

  @param[in] ProcessorIndex  The index of the AP to wait for.
  @param[in] StartCount      The generic timer count when the wait started.
  @param[in] TimeoutTicks    The timeout in generic timer ticks. Zero means no timeout.

  @retval EFI_SUCCESS        The AP finished its procedure.
  @retval EFI_TIMEOUT        The timeout expired before the AP finished.

**/
STATIC
EFI_STATUS
WaitForApFinished (
  IN UINTN   ProcessorIndex,
  IN UINT64  StartCount,
  IN UINT64  TimeoutTicks
  )
{
  while (mCpuMpData.CpuData[ProcessorIndex].State != CpuStateFinished) {
    ArmDataMemoryBarrier ();
    if ((TimeoutTicks != 0) && ((GetPerformanceCounter () - StartCount) >= TimeoutTicks)) {
      return EFI_TIMEOUT;
    }
  }

  mCpuMpData.CpuData[ProcessorIndex].State = CpuStateIdle;
  return EFI_SUCCESS;
}

/**
  Gets the number of processors and the number of enabled processors.

  @param[out] NumberOfProcessors        The total number of processors, if not NULL.
  @param[out] NumberOfEnabledProcessors The number of enabled processors, if not NULL.

  @retval EFI_SUCCESS                   The counts were returned.

**/
EFI_STATUS
EFIAPI
MpInitLibGetNumberOfProcessors (
  OUT UINTN  *NumberOfProcessors        OPTIONAL,
  OUT UINTN  *NumberOfEnabledProcessors OPTIONAL
  )
{
  if (NumberOfProcessors != NULL) {
    *NumberOfProcessors = mCpuMpData.NumberOfProcessors;
  }

  if (NumberOfEnabledProcessors != NULL) {
    *NumberOfEnabledProcessors = mCpuMpData.NumberOfEnabledProcessors;
  }

  return EFI_SUCCESS;
}

/**
  Gets detailed MP-related information on the requested processor at the
  instant this call is made. This service may only be called from the BSP.

  @param[in]  ProcessorNumber      The handle number of the processor.
  @param[out] ProcessorInfoBuffer  A pointer to the buffer where information
                                   for the requested processor is deposited.
  @param[out] HealthData           If not NULL, the health status of the
                                   processor is returned here.

  @retval EFI_SUCCESS             Processor information was returned.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_INVALID_PARAMETER   ProcessorInfoBuffer is NULL.
  @retval EFI_NOT_FOUND           The processor does not exist in the platform.

**/
EFI_STATUS
EFIAPI
MpInitLibGetProcessorInfo (
  IN  UINTN                      ProcessorNumber,
  OUT EFI_PROCESSOR_INFORMATION  *ProcessorInfoBuffer,
  OUT EFI_HEALTH_FLAGS           *HealthData  OPTIONAL
  )
{
  if (ProcessorInfoBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!IsCurrentProcessorBSP ()) {
    return EFI_DEVICE_ERROR;
  }

  if (ProcessorNumber >= mCpuMpData.NumberOfProcessors) {
    return EFI_NOT_FOUND;
  }

  CopyMem (
    ProcessorInfoBuffer,
    &mCpuMpData.CpuData[ProcessorNumber].Info,
    sizeof (EFI_PROCESSOR_INFORMATION)
    );

  if (HealthData != NULL) {
    HealthData->Uint32 = mCpuMpData.CpuData[ProcessorNumber].Info.StatusFlag & PROCESSOR_HEALTH_STATUS_BIT;
  }

  return EFI_SUCCESS;
}

/**
  Executes a caller provided function on all enabled APs. This PEI
  implementation only supports blocking mode, so WaitEvent must be NULL.

  @param[in]  Procedure                The function to run on enabled APs.
  @param[in]  SingleThread             If TRUE, APs run Procedure one at a time
                                       in ascending processor number order.
                                       If FALSE, all APs are started before
                                       waiting for any of them.
  @param[in]  WaitEvent                Must be NULL in PEI.
  @param[in]  TimeoutInMicroseconds    Time limit for all APs to finish. Zero
                                       means no limit.
  @param[in]  ProcedureArgument        The parameter passed into Procedure for
                                       all APs.
  @param[out] FailedCpuList            If not NULL, set to NULL when all APs
                                       finish. Otherwise set to a buffer of the
                                       failed processor numbers, terminated by
                                       END_OF_CPU_LIST. The caller frees it with
                                       FreePool().

  @retval EFI_SUCCESS             All enabled APs finished.
  @retval EFI_UNSUPPORTED         WaitEvent is not NULL.
  @retval EFI_DEVICE_ERROR        Caller processor is an AP.
  @retval EFI_NOT_STARTED         No enabled APs exist in the system.
  @retval EFI_INVALID_PARAMETER   Procedure is NULL.
  @retval EFI_TIMEOUT             At least one AP did not finish in time.

**/
EFI_STATUS
EFIAPI
MpInitLibStartupAllAPs (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  BOOLEAN           SingleThread,
  IN  EFI_EVENT         WaitEvent               OPTIONAL,
  IN  UINTN             TimeoutInMicroseconds,
  IN  VOID              *ProcedureArgument      OPTIONAL,
  OUT UINTN             **FailedCpuList         OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINTN       Index;
  UINTN       EnabledApCount;
  UINTN       FailedCount;
  UINTN       *FailedList;
  UINT64      StartCount;
  UINT64      TimeoutTicks;

  if (Procedure == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (WaitEvent != NULL) {
    return EFI_UNSUPPORTED;
  }

  if (!IsCurrentProcessorBSP ()) {
    return EFI_DEVICE_ERROR;
  }

  if (FailedCpuList != NULL) {
    *FailedCpuList = NULL;
  }

  EnabledApCount = 0;
  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    if (!IsProcessorBSP (Index) && IsProcessorEnabled (Index)) {
      EnabledApCount++;
    }
  }

  if (EnabledApCount == 0) {
    return EFI_NOT_STARTED;
  }

  FailedList = AllocatePool ((EnabledApCount + 1) * sizeof (UINTN));
  if (FailedList == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  FailedCount  = 0;
  StartCount   = GetPerformanceCounter ();
  TimeoutTicks = MicrosecondsToTicks (TimeoutInMicroseconds);

  //
  // In SingleThread mode each AP is run to completion before the next one starts.
  // Otherwise every AP is started first and then waited for.
  //
  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    if (IsProcessorBSP (Index) || !IsProcessorEnabled (Index)) {
      continue;
    }

    Status = StartAp (Index, Procedure, ProcedureArgument);
    if (!EFI_ERROR (Status) && SingleThread) {
      Status = WaitForApFinished (Index, StartCount, TimeoutTicks);
    }

    if (EFI_ERROR (Status)) {
      FailedList[FailedCount++] = Index;
    }
  }

  if (!SingleThread) {
    for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
      if (IsProcessorBSP (Index) || !IsProcessorEnabled (Index)) {
        continue;
      }

      if (EFI_ERROR (WaitForApFinished (Index, StartCount, TimeoutTicks))) {
        FailedList[FailedCount++] = Index;
      }
    }
  }

  if (FailedCount == 0) {
    FreePool (FailedList);
    return EFI_SUCCESS;
  }

  FailedList[FailedCount] = END_OF_CPU_LIST;
  if (FailedCpuList != NULL) {
    *FailedCpuList = FailedList;
  } else {
    FreePool (FailedList);
  }

  return EFI_TIMEOUT;
}

/**
  Executes a caller provided function on one enabled AP. This PEI
  implementation only supports blocking mode, so WaitEvent must be NULL.

  @param[in]  Procedure                The function to run on the AP.
  @param[in]  ProcessorNumber          The handle number of the AP.
  @param[in]  WaitEvent                Must be NULL in PEI.
  @param[in]  TimeoutInMicroseconds    Time limit for the AP to finish. Zero
                                       means no limit.
  @param[in]  ProcedureArgument        The parameter passed into Procedure.
  @param[out] Finished                 If not NULL, set to TRUE when the AP finished.

  @retval EFI_SUCCESS             The AP finished its procedure.
  @retval EFI_UNSUPPORTED         WaitEvent is not NULL.
  @retval EFI_DEVICE_ERROR        Caller processor is an AP.
  @retval EFI_INVALID_PARAMETER   Procedure is NULL, or ProcessorNumber is the
                                  BSP or a disabled processor.
  @retval EFI_NOT_FOUND           ProcessorNumber does not exist.
  @retval EFI_NOT_READY           The AP is already running a procedure.
  @retval EFI_TIMEOUT             The AP did not finish in time.

**/
EFI_STATUS
EFIAPI
MpInitLibStartupThisAP (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  UINTN             ProcessorNumber,
  IN  EFI_EVENT         WaitEvent               OPTIONAL,
  IN  UINTN             TimeoutInMicroseconds,
  IN  VOID              *ProcedureArgument      OPTIONAL,
  OUT BOOLEAN           *Finished               OPTIONAL
  )
{
  EFI_STATUS  Status;

  if (Finished != NULL) {
    *Finished = FALSE;
  }

  if (Procedure == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (WaitEvent != NULL) {
    return EFI_UNSUPPORTED;
  }

  if (!IsCurrentProcessorBSP ()) {
    return EFI_DEVICE_ERROR;
  }

  if (ProcessorNumber >= mCpuMpData.NumberOfProcessors) {
    return EFI_NOT_FOUND;
  }

  if (IsProcessorBSP (ProcessorNumber) || !IsProcessorEnabled (ProcessorNumber)) {
    return EFI_INVALID_PARAMETER;
  }

  if (mCpuMpData.CpuData[ProcessorNumber].State != CpuStateIdle) {
    return EFI_NOT_READY;
  }

  Status = StartAp (ProcessorNumber, Procedure, ProcedureArgument);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = WaitForApFinished (
             ProcessorNumber,
             GetPerformanceCounter (),
             MicrosecondsToTicks (TimeoutInMicroseconds)
             );
  if (!EFI_ERROR (Status) && (Finished != NULL)) {
    *Finished = TRUE;
  }

  return Status;
}

/**
  Executes the procedure on the BSP, then on all enabled APs in blocking mode.

  @param[in]  Procedure              The function to run on all processors.
  @param[in]  TimeoutInMicroseconds  Time limit for the APs to finish. Zero
                                     means no limit.
  @param[in]  ProcedureArgument      The parameter passed into Procedure.

  @retval EFI_SUCCESS                All processors finished.
  @retval EFI_INVALID_PARAMETER      Procedure is NULL.
  @retval EFI_TIMEOUT                At least one AP did not finish in time.

**/
EFI_STATUS
EFIAPI
MpInitLibStartupAllCPUs (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  UINTN             TimeoutInMicroseconds,
  IN  VOID              *ProcedureArgument      OPTIONAL
  )
{
  EFI_STATUS  Status;

  if (Procedure == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Procedure (ProcedureArgument);

  Status = MpInitLibStartupAllAPs (
             Procedure,
             FALSE,
             NULL,
             TimeoutInMicroseconds,
             ProcedureArgument,
             NULL
             );
  if (Status == EFI_NOT_STARTED) {
    Status = EFI_SUCCESS;
  }

  return Status;
}

/**
  Switching the BSP is not supported in PEI.

  @retval EFI_UNSUPPORTED  Always.

**/
EFI_STATUS
EFIAPI
MpInitLibSwitchBSP (
  IN UINTN    ProcessorNumber,
  IN BOOLEAN  EnableOldBSP
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Enabling and disabling APs is not supported in PEI.

  @retval EFI_UNSUPPORTED  Always.

**/
EFI_STATUS
EFIAPI
MpInitLibEnableDisableAP (
  IN  UINTN    ProcessorNumber,
  IN  BOOLEAN  EnableAP,
  IN  UINT32   *HealthFlag OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Initializes the MP data from the ARM MP core info HOB.

  @retval EFI_SUCCESS             MP initialization succeeds.
  @retval EFI_NOT_FOUND           The system is uniprocessor, nothing to do.
  @retval EFI_OUT_OF_RESOURCES    Memory for the MP data could not be allocated.

**/
EFI_STATUS
EFIAPI
MpInitLibInitialize (
  VOID
  )
{
  UINTN                   MaxCpus;
  EFI_HOB_GENERIC_HEADER  *Hob;
  CONST ARM_CORE_INFO     *CoreInfo;

  MaxCpus  = 1;
  CoreInfo = NULL;

  Hob = GetFirstGuidHob (&gArmMpCoreInfoGuid);
  if (Hob != NULL) {
    CoreInfo = (CONST ARM_CORE_INFO *)GET_GUID_HOB_DATA (Hob);
    MaxCpus  = GET_GUID_HOB_DATA_SIZE (Hob) / sizeof (ARM_CORE_INFO);
  }

  if (MaxCpus == 1) {
    DEBUG ((DEBUG_WARN, "Trying to use MP services on a UP system\n"));
    return EFI_NOT_FOUND;
  }

  return InitializeMpCommonData (MaxCpus, CoreInfo);
}
