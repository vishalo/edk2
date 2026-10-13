/** @file
  PEIM that tests the PEI instance of MpInitLib.

  Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

#include <Library/DebugLib.h>
#include <Library/MpInitLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/PeiServicesLib.h>

#include "MpInitTestCommon.h"

/**
  Initializes MpInitLib and runs the MP test sequence against it.

  @param[in] FileHandle    Handle of the file being invoked.
  @param[in] PeiServices  Describes the list of possible PEI Services.

  @retval EFI_SUCCESS   Every check passed.
  @retval EFI_ABORTED   A check failed.
**/
EFI_STATUS
EFIAPI
MpInitLibPeiTestEntryPoint (
  IN EFI_PEI_FILE_HANDLE      FileHandle,
  IN CONST EFI_PEI_SERVICES  **PeiServices
  )
{
  EFI_STATUS  Status;

  //
  // MpInitLib keeps its state in writable globals, which cannot be used while
  // this PEIM executes in place from flash. Register for shadowing so the
  // entry point is invoked again from RAM, and only run the test from there.
  //
  Status = PeiServicesRegisterForShadow (FileHandle);
  if (!EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  if (Status != EFI_ALREADY_STARTED) {
    return Status;
  }

  Status = MpInitLibInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MpInitLibPeiTest: FAIL MpInitLibInitialize %r\n", Status));
    return Status;
  }

  return RunMpServicesTest (GetMpInitLibTestOps (), "MpInitLibPeiTest");
}
