/** @file
  DXE driver that tests the DXE instance of MpInitLib.

  Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/MpInitLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include "MpInitTestCommon.h"

/**
  Initializes MpInitLib and runs the MP test sequence against it.

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The test ran. Its result is in the log.
**/
EFI_STATUS
EFIAPI
MpInitLibDxeTestEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = MpInitLibInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MpInitLibDxeTest: FAIL MpInitLibInitialize %r\n", Status));
    return EFI_SUCCESS;
  }

  RunMpServicesTest (GetMpInitLibTestOps (), "MpInitLibDxeTest");
  return EFI_SUCCESS;
}
