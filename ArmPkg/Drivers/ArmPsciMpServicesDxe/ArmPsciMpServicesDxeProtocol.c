/** @file
  Implement the ARM MP Services protocol interface.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Ppi/SecPlatformInformation.h>

#include <Protocol/MpService.h>

extern EFI_STATUS EFIAPI ArmPsciGetNumberOfProcessors (
  OUT UINTN  *NumberOfProcessors        OPTIONAL,
  OUT UINTN  *NumberOfEnabledProcessors OPTIONAL
  );
extern EFI_STATUS EFIAPI ArmPsciGetProcessorInfo (
  IN  UINTN                     ProcessorNumber,
  OUT EFI_PROCESSOR_INFORMATION  *ProcessorInfoBuffer,
  OUT EFI_HEALTH_FLAGS           *HealthData  OPTIONAL
  );
extern EFI_STATUS EFIAPI ArmPsciStartupAllAPs (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  BOOLEAN            SingleThread,
  IN  EFI_EVENT          WaitEvent               OPTIONAL,
  IN  UINTN              TimeoutInMicroseconds,
  IN  VOID               *ProcedureArgument      OPTIONAL,
  OUT UINTN              **FailedCpuList         OPTIONAL
  );
extern EFI_STATUS EFIAPI ArmPsciStartupThisAP (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  UINTN              ProcessorNumber,
  IN  EFI_EVENT          WaitEvent               OPTIONAL,
  IN  UINTN              TimeoutInMicroseconds,
  IN  VOID               *ProcedureArgument      OPTIONAL,
  OUT BOOLEAN            *Finished               OPTIONAL
  );
extern EFI_STATUS EFIAPI ArmPsciSwitchBSP (
  IN UINTN    ProcessorNumber,
  IN BOOLEAN  EnableOldBSP
  );
extern EFI_STATUS EFIAPI ArmPsciEnableDisableAP (
  IN UINTN   ProcessorNumber,
  IN BOOLEAN EnableAP,
  IN UINT32  *HealthFlag OPTIONAL
  );
extern EFI_STATUS EFIAPI ArmPsciWhoAmI (OUT UINTN *ProcessorNumber);

STATIC EFI_STATUS EFIAPI
ArmPsciMpServicesGetNumberOfProcessors (
  IN EFI_MP_SERVICES_PROTOCOL  *This,
  OUT UINTN                    *NumberOfProcessors,
  OUT UINTN                    *NumberOfEnabledProcessors
  )
{
  return ArmPsciGetNumberOfProcessors (
           NumberOfProcessors,
           NumberOfEnabledProcessors
           );
}

STATIC EFI_STATUS EFIAPI
ArmPsciMpServicesGetProcessorInfo (
  IN EFI_MP_SERVICES_PROTOCOL   *This,
  IN UINTN                      ProcessorNumber,
  OUT EFI_PROCESSOR_INFORMATION  *ProcessorInfoBuffer
  )
{
  return ArmPsciGetProcessorInfo (ProcessorNumber, ProcessorInfoBuffer, NULL);
}

STATIC EFI_STATUS EFIAPI
ArmPsciMpServicesStartupAllAPs (
  IN EFI_MP_SERVICES_PROTOCOL  *This,
  IN EFI_AP_PROCEDURE          Procedure,
  IN BOOLEAN                   SingleThread,
  IN EFI_EVENT                 WaitEvent OPTIONAL,
  IN UINTN                     TimeoutInMicroseconds,
  IN VOID                      *ProcedureArgument OPTIONAL,
  OUT UINTN                    **FailedCpuList OPTIONAL
  )
{
  return ArmPsciStartupAllAPs (Procedure, SingleThread, WaitEvent,
                               TimeoutInMicroseconds, ProcedureArgument,
                               FailedCpuList);
}

STATIC EFI_STATUS EFIAPI
ArmPsciMpServicesStartupThisAP (
  IN EFI_MP_SERVICES_PROTOCOL  *This,
  IN EFI_AP_PROCEDURE          Procedure,
  IN UINTN                     ProcessorNumber,
  IN EFI_EVENT                 WaitEvent OPTIONAL,
  IN UINTN                     TimeoutInMicroseconds,
  IN VOID                      *ProcedureArgument OPTIONAL,
  OUT BOOLEAN                  *Finished OPTIONAL
  )
{
  return ArmPsciStartupThisAP (Procedure, ProcessorNumber, WaitEvent,
                               TimeoutInMicroseconds, ProcedureArgument,
                               Finished);
}

STATIC EFI_STATUS EFIAPI
ArmPsciMpServicesSwitchBSP (
  IN EFI_MP_SERVICES_PROTOCOL  *This,
  IN UINTN                     ProcessorNumber,
  IN BOOLEAN                   EnableOldBSP
  )
{
  return ArmPsciSwitchBSP (ProcessorNumber, EnableOldBSP);
}

STATIC EFI_STATUS EFIAPI
ArmPsciMpServicesEnableDisableAP (
  IN EFI_MP_SERVICES_PROTOCOL  *This,
  IN UINTN                     ProcessorNumber,
  IN BOOLEAN                   EnableAP,
  IN UINT32                    *HealthFlag OPTIONAL
  )
{
  return ArmPsciEnableDisableAP (ProcessorNumber, EnableAP, HealthFlag);
}

STATIC EFI_STATUS EFIAPI
ArmPsciMpServicesWhoAmI (
  IN EFI_MP_SERVICES_PROTOCOL  *This,
  OUT UINTN                    *ProcessorNumber
  )
{
  return ArmPsciWhoAmI (ProcessorNumber);
}

/** The ARM MP Services protocol instance installed by the driver. */
EFI_MP_SERVICES_PROTOCOL  gArmPsciMpServicesProtocol = {
  ArmPsciMpServicesGetNumberOfProcessors,
  ArmPsciMpServicesGetProcessorInfo,
  ArmPsciMpServicesStartupAllAPs,
  ArmPsciMpServicesStartupThisAP,
  ArmPsciMpServicesSwitchBSP,
  ArmPsciMpServicesEnableDisableAP,
  ArmPsciMpServicesWhoAmI
};
