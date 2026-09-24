/** @file
  ARM MP Services implementation shared by PI and DXE users.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/MpInitLib.h>

/**
  Return the number of ARM processors.

  @param[out] NumberOfProcessors        Total number of processors, if requested.
  @param[out] NumberOfEnabledProcessors Number of enabled processors, if requested.

  @retval EFI_SUCCESS           The processor counts were returned.
  @retval EFI_INVALID_PARAMETER A required parameter was invalid.
**/
EFI_STATUS
EFIAPI
ArmPsciGetNumberOfProcessors (
  OUT UINTN  *NumberOfProcessors        OPTIONAL,
  OUT UINTN  *NumberOfEnabledProcessors OPTIONAL
  )
{
  return MpInitLibGetNumberOfProcessors (
           NumberOfProcessors,
           NumberOfEnabledProcessors
           );
}

/**
  Return information about an ARM processor.

  @param[in]  ProcessorNumber      The processor number.
  @param[out] ProcessorInfoBuffer  The processor information.
  @param[out] HealthData           The processor health information, if requested.

  @retval EFI_SUCCESS              The processor information was returned.
  @retval EFI_INVALID_PARAMETER    A required parameter was NULL or invalid.
  @retval EFI_NOT_FOUND            ProcessorNumber does not identify a processor.
**/
EFI_STATUS
EFIAPI
ArmPsciGetProcessorInfo (
  IN  UINTN                     ProcessorNumber,
  OUT EFI_PROCESSOR_INFORMATION  *ProcessorInfoBuffer,
  OUT EFI_HEALTH_FLAGS           *HealthData  OPTIONAL
  )
{
  return MpInitLibGetProcessorInfo (
           ProcessorNumber,
           ProcessorInfoBuffer,
           HealthData
           );
}

/**
  Dispatch a procedure on all enabled application processors.

  @param[in]  Procedure             The procedure to run on each AP.
  @param[in]  SingleThread          Run the procedure on one AP at a time.
  @param[in]  WaitEvent             Event signaled when dispatch is complete.
  @param[in]  TimeoutInMicroseconds Maximum time to wait for completion.
  @param[in]  ProcedureArgument     Argument passed to Procedure.
  @param[out] FailedCpuList         List of processors that failed to complete.

  @retval EFI_SUCCESS              The procedure was dispatched successfully.
  @retval EFI_INVALID_PARAMETER    A required parameter was NULL or invalid.
  @retval EFI_TIMEOUT              One or more processors did not complete.
**/
EFI_STATUS
EFIAPI
ArmPsciStartupAllAPs (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  BOOLEAN            SingleThread,
  IN  EFI_EVENT          WaitEvent               OPTIONAL,
  IN  UINTN              TimeoutInMicroseconds,
  IN  VOID               *ProcedureArgument      OPTIONAL,
  OUT UINTN              **FailedCpuList         OPTIONAL
  )
{
  return MpInitLibStartupAllAPs (
           Procedure,
           SingleThread,
           WaitEvent,
           TimeoutInMicroseconds,
           ProcedureArgument,
           FailedCpuList
           );
}

/**
  Dispatch a procedure on a selected application processor.

  @param[in]  Procedure             The procedure to run on the AP.
  @param[in]  ProcessorNumber       The processor number.
  @param[in]  WaitEvent             Event signaled when dispatch is complete.
  @param[in]  TimeoutInMicroseconds Maximum time to wait for completion.
  @param[in]  ProcedureArgument     Argument passed to Procedure.
  @param[out] Finished              Indicates whether the procedure completed.

  @retval EFI_SUCCESS              The procedure was dispatched successfully.
  @retval EFI_INVALID_PARAMETER    A required parameter was NULL or invalid.
  @retval EFI_NOT_FOUND            ProcessorNumber does not identify a processor.
  @retval EFI_TIMEOUT              The processor did not complete in time.
**/
EFI_STATUS
EFIAPI
ArmPsciStartupThisAP (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  UINTN              ProcessorNumber,
  IN  EFI_EVENT          WaitEvent               OPTIONAL,
  IN  UINTN              TimeoutInMicroseconds,
  IN  VOID               *ProcedureArgument      OPTIONAL,
  OUT BOOLEAN            *Finished               OPTIONAL
  )
{
  return MpInitLibStartupThisAP (
           Procedure,
           ProcessorNumber,
           WaitEvent,
           TimeoutInMicroseconds,
           ProcedureArgument,
           Finished
           );
}

/**
  Switch the BSP to another enabled processor.

  @param[in] ProcessorNumber  The processor to make the BSP.
  @param[in] EnableOldBSP     Re-enable the current BSP after the switch.

  @retval EFI_SUCCESS              The BSP was switched.
  @retval EFI_INVALID_PARAMETER    ProcessorNumber is invalid.
  @retval EFI_UNSUPPORTED          BSP switching is not supported.
**/
EFI_STATUS
EFIAPI
ArmPsciSwitchBSP (
  IN UINTN    ProcessorNumber,
  IN BOOLEAN  EnableOldBSP
  )
{
  return MpInitLibSwitchBSP (ProcessorNumber, EnableOldBSP);
}

/**
  Enable or disable an application processor.

  @param[in]  ProcessorNumber  The processor number.
  @param[in]  EnableAP         TRUE to enable the processor, FALSE to disable it.
  @param[out] HealthFlag       The resulting processor health flag, if requested.

  @retval EFI_SUCCESS              The processor state was changed.
  @retval EFI_INVALID_PARAMETER    ProcessorNumber is invalid.
  @retval EFI_UNSUPPORTED          The requested operation is not supported.
**/
EFI_STATUS
EFIAPI
ArmPsciEnableDisableAP (
  IN  UINTN    ProcessorNumber,
  IN  BOOLEAN  EnableAP,
  IN  UINT32   *HealthFlag OPTIONAL
  )
{
  return MpInitLibEnableDisableAP (ProcessorNumber, EnableAP, HealthFlag);
}

/**
  Return the processor number of the calling processor.

  @param[out] ProcessorNumber  The processor number.

  @retval EFI_SUCCESS           The processor number was returned.
  @retval EFI_INVALID_PARAMETER ProcessorNumber is NULL.
**/
EFI_STATUS
EFIAPI
ArmPsciWhoAmI (
  OUT UINTN  *ProcessorNumber
  )
{
  return MpInitLibWhoAmI (ProcessorNumber);
}
