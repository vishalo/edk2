/** @file
  DXE driver that tests the EFI_MP_SERVICES_PROTOCOL instance.

  Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Protocol/MpService.h>

#include "MpInitTestCommon.h"

STATIC EFI_MP_SERVICES_PROTOCOL  *mMpServices;

STATIC
EFI_STATUS
EFIAPI
ProtocolGetNumberOfProcessors (
  OUT UINTN  *NumberOfProcessors,
  OUT UINTN  *NumberOfEnabledProcessors
  )
{
  return mMpServices->GetNumberOfProcessors (mMpServices, NumberOfProcessors, NumberOfEnabledProcessors);
}

STATIC
EFI_STATUS
EFIAPI
ProtocolGetProcessorInfo (
  IN  UINTN                      ProcessorNumber,
  OUT EFI_PROCESSOR_INFORMATION  *ProcessorInfo
  )
{
  return mMpServices->GetProcessorInfo (mMpServices, ProcessorNumber, ProcessorInfo);
}

STATIC
EFI_STATUS
EFIAPI
ProtocolWhoAmI (
  OUT UINTN  *ProcessorNumber
  )
{
  return mMpServices->WhoAmI (mMpServices, ProcessorNumber);
}

STATIC
EFI_STATUS
EFIAPI
ProtocolStartupAllAPs (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  VOID              *ProcedureArgument
  )
{
  return mMpServices->StartupAllAPs (mMpServices, Procedure, FALSE, NULL, 1000000, ProcedureArgument, NULL);
}

STATIC
EFI_STATUS
EFIAPI
ProtocolStartupThisAP (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  UINTN             ProcessorNumber,
  IN  VOID              *ProcedureArgument
  )
{
  return mMpServices->StartupThisAP (mMpServices, Procedure, ProcessorNumber, NULL, 1000000, ProcedureArgument, NULL);
}

STATIC CONST MP_SERVICES_TEST_OPS  mProtocolTestOps = {
  ProtocolGetNumberOfProcessors,
  ProtocolGetProcessorInfo,
  ProtocolWhoAmI,
  ProtocolStartupAllAPs,
  ProtocolStartupThisAP
};

/**
  Locates EFI_MP_SERVICES_PROTOCOL and runs the MP test sequence against it.

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The test ran. Its result is in the log.
**/
EFI_STATUS
EFIAPI
MpServiceProtocolTestEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (&gEfiMpServiceProtocolGuid, NULL, (VOID **)&mMpServices);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MpServiceProtocolTest: FAIL LocateProtocol %r\n", Status));
    return EFI_SUCCESS;
  }

  RunMpServicesTest (&mProtocolTestOps, "MpServiceProtocolTest");
  return EFI_SUCCESS;
}
