/** @file
  Install the ARM MP Services protocol.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>

#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/MpInitLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/MpService.h>

STATIC
EFI_STATUS
EFIAPI
ArmPsciGetNumberOfProcessors (
  IN  EFI_MP_SERVICES_PROTOCOL  *This,
  OUT UINTN                     *NumberOfProcessors,
  OUT UINTN                     *NumberOfEnabledProcessors
  )
{
  return MpInitLibGetNumberOfProcessors (
           NumberOfProcessors,
           NumberOfEnabledProcessors
           );
}

STATIC
EFI_STATUS
EFIAPI
ArmPsciGetProcessorInfo (
  IN  EFI_MP_SERVICES_PROTOCOL   *This,
  IN  UINTN                      ProcessorNumber,
  OUT EFI_PROCESSOR_INFORMATION  *ProcessorInfoBuffer
  )
{
  return MpInitLibGetProcessorInfo (
           ProcessorNumber,
           ProcessorInfoBuffer,
           NULL
           );
}

STATIC
EFI_STATUS
EFIAPI
ArmPsciStartupAllAPs (
  IN  EFI_MP_SERVICES_PROTOCOL  *This,
  IN  EFI_AP_PROCEDURE          Procedure,
  IN  BOOLEAN                   SingleThread,
  IN  EFI_EVENT                 WaitEvent               OPTIONAL,
  IN  UINTN                     TimeoutInMicroseconds,
  IN  VOID                      *ProcedureArgument      OPTIONAL,
  OUT UINTN                     **FailedCpuList         OPTIONAL
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

STATIC
EFI_STATUS
EFIAPI
ArmPsciStartupThisAP (
  IN  EFI_MP_SERVICES_PROTOCOL  *This,
  IN  EFI_AP_PROCEDURE          Procedure,
  IN  UINTN                     ProcessorNumber,
  IN  EFI_EVENT                 WaitEvent               OPTIONAL,
  IN  UINTN                     TimeoutInMicroseconds,
  IN  VOID                      *ProcedureArgument      OPTIONAL,
  OUT BOOLEAN                   *Finished               OPTIONAL
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

STATIC
EFI_STATUS
EFIAPI
ArmPsciSwitchBSP (
  IN EFI_MP_SERVICES_PROTOCOL  *This,
  IN UINTN                     ProcessorNumber,
  IN BOOLEAN                   EnableOldBSP
  )
{
  return MpInitLibSwitchBSP (ProcessorNumber, EnableOldBSP);
}

STATIC
EFI_STATUS
EFIAPI
ArmPsciEnableDisableAP (
  IN EFI_MP_SERVICES_PROTOCOL  *This,
  IN UINTN                     ProcessorNumber,
  IN BOOLEAN                   EnableAP,
  IN UINT32                    *HealthFlag OPTIONAL
  )
{
  return MpInitLibEnableDisableAP (ProcessorNumber, EnableAP, HealthFlag);
}

STATIC
EFI_STATUS
EFIAPI
ArmPsciWhoAmI (
  IN  EFI_MP_SERVICES_PROTOCOL  *This,
  OUT UINTN                     *ProcessorNumber
  )
{
  return MpInitLibWhoAmI (ProcessorNumber);
}

STATIC EFI_MP_SERVICES_PROTOCOL  mMpServicesProtocol = {
  ArmPsciGetNumberOfProcessors,
  ArmPsciGetProcessorInfo,
  ArmPsciStartupAllAPs,
  ArmPsciStartupThisAP,
  ArmPsciSwitchBSP,
  ArmPsciEnableDisableAP,
  ArmPsciWhoAmI
};

EFI_STATUS
EFIAPI
ArmPsciMpServicesDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                 Status;
  EFI_HANDLE                 Handle;
  EFI_LOADED_IMAGE_PROTOCOL  *Image;

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&Image
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // AP entry code can run with the MMU disabled, so make the driver image
  // visible to all processors before starting any AP.
  WriteBackDataCacheRange (Image->ImageBase, Image->ImageSize);

  Status = MpInitLibInitialize ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Handle = NULL;
  return gBS->InstallMultipleProtocolInterfaces (
                &Handle,
                &gEfiMpServiceProtocolGuid,
                &mMpServicesProtocol,
                NULL
                );
}
