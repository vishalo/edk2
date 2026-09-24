/** @file
  ARM MP Services implementation shared by PI and DXE users.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/MpInitLib.h>

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

EFI_STATUS
EFIAPI
ArmPsciSwitchBSP (
  IN UINTN    ProcessorNumber,
  IN BOOLEAN  EnableOldBSP
  )
{
  return MpInitLibSwitchBSP (ProcessorNumber, EnableOldBSP);
}

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

EFI_STATUS
EFIAPI
ArmPsciWhoAmI (
  OUT UINTN  *ProcessorNumber
  )
{
  return MpInitLibWhoAmI (ProcessorNumber);
}
