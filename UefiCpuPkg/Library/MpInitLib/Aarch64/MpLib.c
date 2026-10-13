/** @file
  MP Initialize Library implementation for AArch64 PSCI platforms.

  The MP Initialize Library uses PSCI (Power State Coordination Interface) to
  power on secondary cores and dispatch user-supplied procedures to them.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2011, Apple Inc. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/ArmLib.h>
#include <Library/ArmMmuLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/CpuExceptionHandlerLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MpInitLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Guid/ArmMpCoreInfo.h>

#include "MpLib.h"

#define POLL_INTERVAL_US  50000

STATIC BOOLEAN      mNonBlockingModeAllowed;

/** Returns whether the specified processor is the BSP.

  @param[in] ProcessorIndex The index the processor to check.

  @return TRUE if the processor is the BSP, FALSE otherwise.
**/
STATIC
BOOLEAN
IsProcessorBSP (
  UINTN  ProcessorIndex
  )
{
  EFI_PROCESSOR_INFORMATION  *CpuInfo;

  CpuInfo = &mCpuMpData.CpuData[ProcessorIndex].Info;

  return (CpuInfo->StatusFlag & PROCESSOR_AS_BSP_BIT) != 0;
}

/** Get the Application Processors state.

  @param[in]  CpuData    The pointer to CPU_AP_DATA of specified AP.

  @return The AP status.
**/
STATIC
CPU_STATE
GetApState (
  IN  CPU_AP_DATA  *CpuData
  )
{
  return CpuData->State;
}

/** Configures the processor context with the user-supplied procedure and
    argument.

   @param CpuData           The processor context.
   @param Procedure         The user-supplied procedure.
   @param ProcedureArgument The user-supplied procedure argument.

**/
STATIC
VOID
SetApProcedure (
  IN   CPU_AP_DATA       *CpuData,
  IN   EFI_AP_PROCEDURE  Procedure,
  IN   VOID              *ProcedureArgument
  )
{
  ASSERT (CpuData != NULL);
  ASSERT (Procedure != NULL);

  CpuData->Parameter = ProcedureArgument;
  CpuData->Procedure = Procedure;
}

/** Returns the index of the next processor that is blocked.

   @param[out] NextNumber The index of the next blocked processor.

   @retval EFI_SUCCESS   Successfully found the next blocked processor.
   @retval EFI_NOT_FOUND There are no blocked processors.

**/
STATIC
EFI_STATUS
GetNextBlockedNumber (
  OUT UINTN  *NextNumber
  )
{
  UINTN        Index;
  CPU_STATE    State;
  CPU_AP_DATA  *CpuData;

  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    CpuData = &mCpuMpData.CpuData[Index];
    if (IsProcessorBSP (Index)) {
      // Skip BSP
      continue;
    }

    State = CpuData->State;

    if (State == CpuStateBlocked) {
      *NextNumber = Index;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/** Stalls the BSP for the minimum of POLL_INTERVAL_US and Timeout.

   @param[in]  Timeout    The time limit in microseconds remaining for
                          APs to return from Procedure.

   @retval     StallTime  Time of execution stall.
**/
STATIC
UINTN
CalculateAndStallInterval (
  IN UINTN  Timeout
  )
{
  UINTN  StallTime;

  if ((Timeout < POLL_INTERVAL_US) && (Timeout != 0)) {
    StallTime = Timeout;
  } else {
    StallTime = POLL_INTERVAL_US;
  }

  gBS->Stall (StallTime);

  return StallTime;
}



/** Returns whether the processor executing this function is the BSP.

    @return Whether the current processor is the BSP.
**/
STATIC
BOOLEAN
IsCurrentProcessorBSP (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       ProcessorIndex;

  Status = MpInitLibWhoAmI (&ProcessorIndex);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    return FALSE;
  }

  return IsProcessorBSP (ProcessorIndex);
}

/** Returns whether the specified processor is enabled.

   @param[in] ProcessorIndex The index of the processor to check.

   @return TRUE if the processor is enabled, FALSE otherwise.
**/
STATIC
BOOLEAN
IsProcessorEnabled (
  UINTN  ProcessorIndex
  )
{
  EFI_PROCESSOR_INFORMATION  *CpuInfo;

  CpuInfo = &mCpuMpData.CpuData[ProcessorIndex].Info;

  return (CpuInfo->StatusFlag & PROCESSOR_ENABLED_BIT) != 0;
}

/** Adds the specified processor the list of failed processors.

   @param ProcessorIndex The processor index to add.
   @param ApState        Processor state.

**/
STATIC
VOID
AddProcessorToFailedList (
  UINTN      ProcessorIndex,
  CPU_STATE  ApState
  )
{
  UINTN    Index;
  BOOLEAN  Found;

  Found = FALSE;

  if ((mCpuMpData.FailedList == NULL) ||
      (ApState == CpuStateIdle) ||
      (ApState == CpuStateFinished) ||
      IsProcessorBSP (ProcessorIndex))
  {
    return;
  }

  // If we are retrying make sure we don't double count
  for (Index = 0; Index < mCpuMpData.FailedListIndex; Index++) {
    if (mCpuMpData.FailedList[Index] == ProcessorIndex) {
      Found = TRUE;
      break;
    }
  }

  /* If the CPU isn't already in the FailedList, add it */
  if (!Found) {
    mCpuMpData.FailedList[mCpuMpData.FailedListIndex++] = ProcessorIndex;
  }
}

/** Handles the StartupAllAPs case where the timeout has occurred.

**/
STATIC
VOID
ProcessStartupAllAPsTimeout (
  VOID
  )
{
  CPU_AP_DATA  *CpuData;
  UINTN        Index;

  if (mCpuMpData.FailedList == NULL) {
    return;
  }

  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    CpuData = &mCpuMpData.CpuData[Index];
    if (IsProcessorBSP (Index)) {
      // Skip BSP
      continue;
    }

    if (!IsProcessorEnabled (Index)) {
      // Skip Disabled processors
      continue;
    }

    CpuData = &mCpuMpData.CpuData[Index];
    AddProcessorToFailedList (Index, GetApState (CpuData));
  }
}

/** Updates the status of the APs.

   @param[in] ProcessorIndex The index of the AP to update.
**/
STATIC
VOID
UpdateApStatus (
  IN UINTN  ProcessorIndex
  )
{
  EFI_STATUS   Status;
  CPU_AP_DATA  *CpuData;
  CPU_AP_DATA  *NextCpuData;
  CPU_STATE    State;
  UINTN        NextNumber;

  CpuData = &mCpuMpData.CpuData[ProcessorIndex];

  if (IsProcessorBSP (ProcessorIndex)) {
    // Skip BSP
    return;
  }

  if (!IsProcessorEnabled (ProcessorIndex)) {
    // Skip Disabled processors
    return;
  }

  State = GetApState (CpuData);

  switch (State) {
    case CpuStateFinished:
      if (mCpuMpData.SingleThread) {
        Status = GetNextBlockedNumber (&NextNumber);
        if (!EFI_ERROR (Status)) {
          NextCpuData = &mCpuMpData.CpuData[NextNumber];

          NextCpuData->State = CpuStateReady;

          SetApProcedure (
            NextCpuData,
            mCpuMpData.Procedure,
            mCpuMpData.ProcedureArgument
            );

          Status = DispatchCpu (NextNumber);
          if (!EFI_ERROR (Status)) {
            mCpuMpData.StartCount++;
          } else {
            AddProcessorToFailedList (NextNumber, NextCpuData->State);
          }
        }
      }

      CpuData->State = CpuStateIdle;
      mCpuMpData.FinishCount++;
      break;

    default:
      break;
  }
}

/**
  If a timeout is specified in StartupAllAPs(), a timer is set, which invokes
  this procedure periodically to check whether all APs have finished.

  @param[in] Event   The WaitEvent the user supplied.
  @param[in] Context The event context.
**/
STATIC
VOID
EFIAPI
CheckAllAPsStatus (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  mCpuMpData.AllTimeTaken += POLL_INTERVAL_US;

  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    UpdateApStatus (Index);
  }

  if (mCpuMpData.AllTimeoutActive && (mCpuMpData.AllTimeTaken > mCpuMpData.AllTimeout)) {
    ProcessStartupAllAPsTimeout ();

    // Force terminal exit
    mCpuMpData.FinishCount = mCpuMpData.StartCount;
  }

  if (mCpuMpData.FinishCount != mCpuMpData.StartCount) {
    return;
  }

  gBS->SetTimer (
         mCpuMpData.CheckAllAPsEvent,
         TimerCancel,
         0
         );

  Status = gBS->SignalEvent (mCpuMpData.AllWaitEvent);
  ASSERT_EFI_ERROR (Status);
  mCpuMpData.AllWaitEvent = NULL;
}

/** Invoked periodically via a timer to check the state of the processor.

   @param Event   The event supplied by the timer expiration.
   @param Context The processor context.

**/
STATIC
VOID
EFIAPI
CheckThisAPStatus (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS   Status;
  CPU_AP_DATA  *CpuData;
  CPU_STATE    State;

  CpuData = Context;

  CpuData->TimeTaken += POLL_INTERVAL_US;

  State = GetApState (CpuData);

  if (State == CpuStateFinished) {
    Status = gBS->SetTimer (CpuData->CheckThisAPEvent, TimerCancel, 0);
    ASSERT_EFI_ERROR (Status);

    if (CpuData->SingleApFinished != NULL) {
      *(CpuData->SingleApFinished) = TRUE;
    }

    if (CpuData->WaitEvent != NULL) {
      Status = gBS->SignalEvent (CpuData->WaitEvent);
      ASSERT_EFI_ERROR (Status);
    }

    CpuData->State = CpuStateIdle;
  }

  if (CpuData->TimeoutActive && (CpuData->TimeTaken > CpuData->Timeout)) {
    Status = gBS->SetTimer (CpuData->CheckThisAPEvent, TimerCancel, 0);
    if (CpuData->WaitEvent != NULL) {
      Status = gBS->SignalEvent (CpuData->WaitEvent);
      ASSERT_EFI_ERROR (Status);
      CpuData->WaitEvent = NULL;
    }
  }
}


/**
  Event notification function called when the EFI_EVENT_GROUP_READY_TO_BOOT is
  signaled. After this point, non-blocking mode is no longer allowed.

  @param  Event     Event whose notification function is being invoked.
  @param  Context   The pointer to the notification function's context,
                    which is implementation-dependent.

**/
STATIC
VOID
EFIAPI
ReadyToBootSignaled (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  mNonBlockingModeAllowed = FALSE;
}

/** Initializes the MP Services system data

   @param NumberOfProcessors The number of processors, both BSP and AP.
   @param CoreInfo           CPU information gathered earlier during boot.

**/
STATIC
EFI_STATUS
MpServicesInitialize (
  IN   UINTN                NumberOfProcessors,
  IN   CONST ARM_CORE_INFO  *CoreInfo
  )
{
  EFI_STATUS  Status;
  UINTN       Index;
  EFI_EVENT   ReadyToBootEvent;

  Status = InitializeMpCommonData (NumberOfProcessors, CoreInfo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  CheckAllAPsStatus,
                  NULL,
                  &mCpuMpData.CheckAllAPsEvent
                  );
  ASSERT_EFI_ERROR (Status);

  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    Status = gBS->CreateEvent (
                    EVT_TIMER | EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    CheckThisAPStatus,
                    (VOID *)&mCpuMpData.CpuData[Index],
                    &mCpuMpData.CpuData[Index].CheckThisAPEvent
                    );
    ASSERT_EFI_ERROR (Status);
  }

  mNonBlockingModeAllowed = TRUE;

  Status = EfiCreateEventReadyToBootEx (
             TPL_CALLBACK,
             ReadyToBootSignaled,
             NULL,
             &ReadyToBootEvent
             );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  MP Initialize Library initialization.

  Discovers the cores described by the ARM MP core info HOB and prepares the
  MP data structures used by the rest of this library.

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
    DEBUG ((DEBUG_WARN, "Trying to use EFI_MP_SERVICES_PROTOCOL on a UP system"));
    // We are not MP so nothing to do
    return EFI_NOT_FOUND;
  }

  return MpServicesInitialize (MaxCpus, CoreInfo);
}

/** Handles execution of StartupAllAPs when a WaitEvent has been specified.

  @param Procedure         The user-supplied procedure.
  @param ProcedureArgument The user-supplied procedure argument.
  @param WaitEvent         The wait event to be signaled when the work is
                           complete or a timeout has occurred.
  @param TimeoutInMicroseconds The timeout for the work to be completed. Zero
                               indicates an infinite timeout.
  @param SingleThread          Whether the APs will execute sequentially.
  @param FailedCpuList         User-supplied pointer for list of failed CPUs.

   @return EFI_SUCCESS on success.
**/
STATIC
EFI_STATUS
StartupAllAPsWithWaitEvent (
  IN EFI_AP_PROCEDURE  Procedure,
  IN VOID              *ProcedureArgument,
  IN EFI_EVENT         WaitEvent,
  IN UINTN             TimeoutInMicroseconds,
  IN BOOLEAN           SingleThread,
  IN UINTN             **FailedCpuList
  )
{
  EFI_STATUS   Status;
  UINTN        Index;
  CPU_AP_DATA  *CpuData;

  Status = EFI_SUCCESS;

  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    CpuData = &mCpuMpData.CpuData[Index];
    if (IsProcessorBSP (Index)) {
      // Skip BSP
      continue;
    }

    if (!IsProcessorEnabled (Index)) {
      // Skip Disabled processors
      continue;
    }

    if (GetApState (CpuData) == CpuStateReady) {
      SetApProcedure (CpuData, Procedure, ProcedureArgument);
      if ((mCpuMpData.StartCount == 0) || !SingleThread) {
        Status = DispatchCpu (Index);
        if (EFI_ERROR (Status)) {
          AddProcessorToFailedList (Index, CpuData->State);
          break;
        }
      }
    }
  }

  if (EFI_ERROR (Status)) {
    return EFI_NOT_READY;
  }

  //
  // Save data into private data structure, and create timer to poll AP state
  // before exiting
  //
  mCpuMpData.Procedure         = Procedure;
  mCpuMpData.ProcedureArgument = ProcedureArgument;
  mCpuMpData.AllWaitEvent      = WaitEvent;
  mCpuMpData.AllTimeout        = TimeoutInMicroseconds;
  mCpuMpData.AllTimeTaken      = 0;
  mCpuMpData.AllTimeoutActive  = (BOOLEAN)(TimeoutInMicroseconds != 0);
  Status                       = gBS->SetTimer (
                                        mCpuMpData.CheckAllAPsEvent,
                                        TimerPeriodic,
                                        POLL_INTERVAL_US
                                        );

  return Status;
}

/** Sets up the state for the StartupAllAPs function.

   @param SingleThread Whether the APs will execute sequentially.

**/
STATIC
VOID
StartupAllAPsPrepareState (
  IN BOOLEAN  SingleThread
  )
{
  UINTN        Index;
  CPU_STATE    APInitialState;
  CPU_AP_DATA  *CpuData;

  mCpuMpData.FinishCount  = 0;
  mCpuMpData.StartCount   = 0;
  mCpuMpData.SingleThread = SingleThread;

  APInitialState = CpuStateReady;

  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    CpuData = &mCpuMpData.CpuData[Index];

    //
    // Get APs prepared, and put failing APs into FailedCpuList.
    // If "SingleThread", only 1 AP will put into ready state, other AP will be
    // put into ready state 1 by 1, until the previous 1 finished its task.
    // If not "SingleThread", all APs are put into ready state from the
    // beginning
    //

    if (IsProcessorBSP (Index)) {
      // Skip BSP
      continue;
    }

    if (!IsProcessorEnabled (Index)) {
      // Skip Disabled processors
      if (mCpuMpData.FailedList != NULL) {
        mCpuMpData.FailedList[mCpuMpData.FailedListIndex++] = Index;
      }

      continue;
    }

    // If any APs finished after timing out, reset state to Idle
    if (GetApState (CpuData) == CpuStateFinished) {
      CpuData->State = CpuStateIdle;
    }

    if (GetApState (CpuData) != CpuStateIdle) {
      // Skip busy processors
      if (mCpuMpData.FailedList != NULL) {
        mCpuMpData.FailedList[mCpuMpData.FailedListIndex++] = Index;
      }
    }

    CpuData->State = APInitialState;

    mCpuMpData.StartCount++;
    if (SingleThread) {
      APInitialState = CpuStateBlocked;
    }
  }
}

/** Handles execution of StartupAllAPs when no wait event has been specified.

  @param Procedure             The user-supplied procedure.
  @param ProcedureArgument     The user-supplied procedure argument.
  @param TimeoutInMicroseconds The timeout for the work to be completed. Zero
                                indicates an infinite timeout.
  @param SingleThread          Whether the APs will execute sequentially.
  @param FailedCpuList         User-supplied pointer for list of failed CPUs.

  @return EFI_SUCCESS on success.
**/
STATIC
EFI_STATUS
StartupAllAPsNoWaitEvent (
  IN EFI_AP_PROCEDURE  Procedure,
  IN VOID              *ProcedureArgument,
  IN UINTN             TimeoutInMicroseconds,
  IN BOOLEAN           SingleThread,
  IN UINTN             **FailedCpuList
  )
{
  EFI_STATUS   Status;
  UINTN        Index;
  UINTN        NextIndex;
  UINTN        Timeout;
  CPU_AP_DATA  *CpuData;
  BOOLEAN      DispatchError;

  Timeout       = TimeoutInMicroseconds;
  DispatchError = FALSE;

  while (TRUE) {
    for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
      CpuData = &mCpuMpData.CpuData[Index];
      if (IsProcessorBSP (Index)) {
        // Skip BSP
        continue;
      }

      if (!IsProcessorEnabled (Index)) {
        // Skip Disabled processors
        continue;
      }

      switch (GetApState (CpuData)) {
        case CpuStateReady:
          SetApProcedure (CpuData, Procedure, ProcedureArgument);
          Status = DispatchCpu (Index);
          if (EFI_ERROR (Status)) {
            AddProcessorToFailedList (Index, CpuData->State);
            CpuData->State = CpuStateIdle;
            mCpuMpData.StartCount--;
            DispatchError = TRUE;

            if (SingleThread) {
              // Dispatch the next available AP
              Status = GetNextBlockedNumber (&NextIndex);
              if (!EFI_ERROR (Status)) {
                mCpuMpData.CpuData[NextIndex].State = CpuStateReady;
              }
            }
          }

          break;

        case CpuStateFinished:
          mCpuMpData.FinishCount++;
          if (SingleThread) {
            Status = GetNextBlockedNumber (&NextIndex);
            if (!EFI_ERROR (Status)) {
              mCpuMpData.CpuData[NextIndex].State = CpuStateReady;
            }
          }

          CpuData->State = CpuStateIdle;
          break;

        default:
          break;
      }
    }

    if (mCpuMpData.FinishCount == mCpuMpData.StartCount) {
      Status = EFI_SUCCESS;
      break;
    }

    if ((TimeoutInMicroseconds != 0) && (Timeout == 0)) {
      Status = EFI_TIMEOUT;
      break;
    }

    Timeout -= CalculateAndStallInterval (Timeout);
  }

  if (Status == EFI_TIMEOUT) {
    // Add any remaining CPUs to the FailedCpuList
    if (FailedCpuList != NULL) {
      for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
        AddProcessorToFailedList (Index, mCpuMpData.CpuData[Index].State);
      }
    }
  }

  if (DispatchError) {
    Status = EFI_NOT_READY;
  }

  return Status;
}

/**
  Retrieves the number of logical processor in the platform and the number of
  those logical processors that are enabled on this boot. This service may only
  be called from the BSP.

  @param[out] NumberOfProcessors        Pointer to the total number of logical
                                        processors, or NULL.
  @param[out] NumberOfEnabledProcessors Pointer to the number of enabled logical
                                        processors, or NULL.

  @retval EFI_SUCCESS             The requested counts were returned.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_INVALID_PARAMETER   Both NumberOfProcessors and
                                  NumberOfEnabledProcessors are NULL.
**/
EFI_STATUS
EFIAPI
MpInitLibGetNumberOfProcessors (
  OUT UINTN  *NumberOfProcessors        OPTIONAL,
  OUT UINTN  *NumberOfEnabledProcessors OPTIONAL
  )
{
  if ((NumberOfProcessors == NULL) && (NumberOfEnabledProcessors == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!IsCurrentProcessorBSP ()) {
    return EFI_DEVICE_ERROR;
  }

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
  Executes a caller provided function on all enabled APs, either in blocking
  or non-blocking mode. This service may only be called from the BSP.

  @param[in]  Procedure                The function to run on enabled APs.
  @param[in]  SingleThread             If TRUE, APs run Procedure one at a time
                                       in ascending processor number order.
  @param[in]  WaitEvent                Event signaled on completion or timeout
                                       in non-blocking mode, or NULL for
                                       blocking mode.
  @param[in]  TimeoutInMicroseconds    Time limit for APs. Zero means infinity.
  @param[in]  ProcedureArgument        Argument passed to Procedure.
  @param[out] FailedCpuList            If not NULL, receives the list of
                                       processors that did not finish.

  @retval EFI_SUCCESS             Blocking mode: all APs finished. Non-blocking
                                  mode: Procedure was dispatched to all APs.
  @retval EFI_UNSUPPORTED         Non-blocking request after ReadyToBoot.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_NOT_STARTED         No enabled APs exist in the system.
  @retval EFI_NOT_READY           Any enabled APs are busy.
  @retval EFI_TIMEOUT             Blocking mode: the timeout expired.
  @retval EFI_INVALID_PARAMETER   Procedure is NULL.
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

  if (!IsCurrentProcessorBSP ()) {
    return EFI_DEVICE_ERROR;
  }

  if ((mCpuMpData.NumberOfProcessors == 1) || (mCpuMpData.NumberOfEnabledProcessors == 1)) {
    return EFI_NOT_STARTED;
  }

  if (Procedure == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((WaitEvent != NULL) && !mNonBlockingModeAllowed) {
    return EFI_UNSUPPORTED;
  }

  if (FailedCpuList != NULL) {
    mCpuMpData.FailedList = AllocateZeroPool (
                              (mCpuMpData.NumberOfProcessors + 1) *
                              sizeof (UINTN)
                              );
    if (mCpuMpData.FailedList == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    SetMemN (
      mCpuMpData.FailedList,
      (mCpuMpData.NumberOfProcessors + 1) *
      sizeof (UINTN),
      END_OF_CPU_LIST
      );
    mCpuMpData.FailedListIndex = 0;
    *FailedCpuList             = mCpuMpData.FailedList;
  }

  StartupAllAPsPrepareState (SingleThread);

  // If any enabled APs are busy (ignoring the BSP), return EFI_NOT_READY
  if (mCpuMpData.StartCount != (mCpuMpData.NumberOfEnabledProcessors - 1)) {
    return EFI_NOT_READY;
  }

  if (WaitEvent != NULL) {
    Status = StartupAllAPsWithWaitEvent (
               Procedure,
               ProcedureArgument,
               WaitEvent,
               TimeoutInMicroseconds,
               SingleThread,
               FailedCpuList
               );

    if (EFI_ERROR (Status) && (FailedCpuList != NULL)) {
      if (mCpuMpData.FailedListIndex == 0) {
        FreePool (*FailedCpuList);
        *FailedCpuList = NULL;
      }
    }
  } else {
    Status = StartupAllAPsNoWaitEvent (
               Procedure,
               ProcedureArgument,
               TimeoutInMicroseconds,
               SingleThread,
               FailedCpuList
               );

    if (FailedCpuList != NULL) {
      if (mCpuMpData.FailedListIndex == 0) {
        FreePool (*FailedCpuList);
        *FailedCpuList = NULL;
      }
    }
  }

  return Status;
}

/**
  Executes a caller provided function on one enabled AP, either in blocking or
  non-blocking mode. This service may only be called from the BSP.

  @param[in]  Procedure                The function to run on the AP.
  @param[in]  ProcessorNumber          The handle number of the AP.
  @param[in]  WaitEvent                Event signaled on completion or timeout
                                       in non-blocking mode, or NULL for
                                       blocking mode.
  @param[in]  TimeoutInMicroseconds    Time limit for the AP. Zero means infinity.
  @param[in]  ProcedureArgument        Argument passed to Procedure.
  @param[out] Finished                 In non-blocking mode, set to TRUE if the
                                       AP returned before the timeout.

  @retval EFI_SUCCESS             Blocking mode: the AP finished. Non-blocking
                                  mode: Procedure was dispatched to the AP.
  @retval EFI_UNSUPPORTED         Non-blocking request after ReadyToBoot.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_TIMEOUT             Blocking mode: the timeout expired.
  @retval EFI_NOT_READY           The specified AP is busy.
  @retval EFI_NOT_FOUND           The processor does not exist.
  @retval EFI_INVALID_PARAMETER   ProcessorNumber specifies the BSP or a disabled
                                  AP, or Procedure is NULL.
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
  EFI_STATUS   Status;
  UINTN        Timeout;
  CPU_AP_DATA  *CpuData;

  if (!IsCurrentProcessorBSP ()) {
    return EFI_DEVICE_ERROR;
  }

  if (Procedure == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ProcessorNumber >= mCpuMpData.NumberOfProcessors) {
    return EFI_NOT_FOUND;
  }

  CpuData = &mCpuMpData.CpuData[ProcessorNumber];

  if (IsProcessorBSP (ProcessorNumber)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!IsProcessorEnabled (ProcessorNumber)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((GetApState (CpuData) != CpuStateIdle) &&
      (GetApState (CpuData) != CpuStateFinished))
  {
    return EFI_NOT_READY;
  }

  if ((WaitEvent != NULL) && !mNonBlockingModeAllowed) {
    return EFI_UNSUPPORTED;
  }

  Timeout = TimeoutInMicroseconds;

  CpuData->Timeout       = TimeoutInMicroseconds;
  CpuData->TimeTaken     = 0;
  CpuData->TimeoutActive = (BOOLEAN)(TimeoutInMicroseconds != 0);

  SetApProcedure (
    CpuData,
    Procedure,
    ProcedureArgument
    );

  Status = DispatchCpu (ProcessorNumber);
  if (EFI_ERROR (Status)) {
    CpuData->State = CpuStateIdle;
    return EFI_NOT_READY;
  }

  if (WaitEvent != NULL) {
    // Non Blocking
    if (Finished != NULL) {
      CpuData->SingleApFinished = Finished;
      *Finished                 = FALSE;
    }

    CpuData->WaitEvent = WaitEvent;
    Status             = gBS->SetTimer (
                                CpuData->CheckThisAPEvent,
                                TimerPeriodic,
                                POLL_INTERVAL_US
                                );

    return EFI_SUCCESS;
  }

  // Blocking
  while (TRUE) {
    if (GetApState (CpuData) == CpuStateFinished) {
      CpuData->State = CpuStateIdle;
      break;
    }

    if ((TimeoutInMicroseconds != 0) && (Timeout == 0)) {
      return EFI_TIMEOUT;
    }

    Timeout -= CalculateAndStallInterval (Timeout);
  }

  return EFI_SUCCESS;
}

/**
  Switches the BSP. Not supported on this platform.

  @param[in] ProcessorNumber   The handle number of the AP to become the BSP.
  @param[in] EnableOldBSP      If TRUE, the old BSP is listed as enabled AP.

  @retval EFI_UNSUPPORTED      Switching the BSP is not supported.
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
  Enables or disables an AP from this point onward. This service may only be
  called from the BSP.

  @param[in] ProcessorNumber   The handle number of the AP.
  @param[in] EnableAP          TRUE to enable the AP, FALSE to disable it.
  @param[in] HealthFlag        If not NULL, the new health status of the AP.
                               Only PROCESSOR_HEALTH_STATUS_BIT is used.

  @retval EFI_SUCCESS             The AP was enabled or disabled.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_NOT_FOUND           Processor does not exist.
  @retval EFI_INVALID_PARAMETER   ProcessorNumber specifies the BSP.
  @retval EFI_UNSUPPORTED         The AP is busy.
**/
EFI_STATUS
EFIAPI
MpInitLibEnableDisableAP (
  IN  UINTN    ProcessorNumber,
  IN  BOOLEAN  EnableAP,
  IN  UINT32   *HealthFlag OPTIONAL
  )
{
  UINTN        StatusFlag;
  CPU_AP_DATA  *CpuData;

  if (!IsCurrentProcessorBSP ()) {
    return EFI_DEVICE_ERROR;
  }

  if (ProcessorNumber >= mCpuMpData.NumberOfProcessors) {
    return EFI_NOT_FOUND;
  }

  CpuData = &mCpuMpData.CpuData[ProcessorNumber];

  if (IsProcessorBSP (ProcessorNumber)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((GetApState (CpuData) != CpuStateIdle) &&
      (GetApState (CpuData) != CpuStateFinished))
  {
    return EFI_UNSUPPORTED;
  }

  StatusFlag = CpuData->Info.StatusFlag;

  if (EnableAP) {
    if (!IsProcessorEnabled (ProcessorNumber)) {
      mCpuMpData.NumberOfEnabledProcessors++;
    }

    StatusFlag |= PROCESSOR_ENABLED_BIT;
  } else {
    if (IsProcessorEnabled (ProcessorNumber) && !IsProcessorBSP (ProcessorNumber)) {
      mCpuMpData.NumberOfEnabledProcessors--;
    }

    StatusFlag &= ~PROCESSOR_ENABLED_BIT;
  }

  if ((HealthFlag != NULL) && !IsProcessorBSP (ProcessorNumber)) {
    StatusFlag &= ~PROCESSOR_HEALTH_STATUS_BIT;
    StatusFlag |= (*HealthFlag & PROCESSOR_HEALTH_STATUS_BIT);
  }

  CpuData->Info.StatusFlag = StatusFlag;
  return EFI_SUCCESS;
}

/**
  Executes a caller provided function on all enabled CPUs, including the BSP,
  in blocking mode.

  @param[in] Procedure            The function to run on all enabled CPUs.
  @param[in] TimeoutInMicroseconds Time limit for the APs. Zero means infinity.
  @param[in] ProcedureArgument    Argument passed to Procedure.

  @retval EFI_SUCCESS             All CPUs finished, or there are no enabled APs.
  @retval EFI_INVALID_PARAMETER   Procedure is NULL.
  @retval EFI_TIMEOUT             The timeout expired before all APs finished.
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

  // The BSP participates in the work, the APs are blocked until they finish
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
    // No enabled APs, the BSP has already completed the work
    Status = EFI_SUCCESS;
  }

  return Status;
}


