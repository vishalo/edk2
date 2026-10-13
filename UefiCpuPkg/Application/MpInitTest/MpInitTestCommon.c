/** @file
  Shared test sequence for the MpInitLib library and the EFI_MP_SERVICES_PROTOCOL.

  Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MpInitLib.h>
#include <Library/SynchronizationLib.h>
#include <Protocol/MpService.h>

#include "MpInitTestCommon.h"

#define MP_TEST_TIMEOUT_US  1000000

/**
  AP procedure that increments the counter pointed to by Context.

  @param[in,out] Context  A pointer to a UINT32 counter.
**/
STATIC
VOID
EFIAPI
IncrementCounter (
  IN OUT VOID  *Context
  )
{
  InterlockedIncrement ((volatile UINT32 *)Context);
}

STATIC
EFI_STATUS
EFIAPI
MpInitLibGetNumberOfProcessorsTest (
  OUT UINTN  *NumberOfProcessors,
  OUT UINTN  *NumberOfEnabledProcessors
  )
{
  return MpInitLibGetNumberOfProcessors (NumberOfProcessors, NumberOfEnabledProcessors);
}

STATIC
EFI_STATUS
EFIAPI
MpInitLibGetProcessorInfoTest (
  IN  UINTN                      ProcessorNumber,
  OUT EFI_PROCESSOR_INFORMATION  *ProcessorInfo
  )
{
  return MpInitLibGetProcessorInfo (ProcessorNumber, ProcessorInfo, NULL);
}

STATIC
EFI_STATUS
EFIAPI
MpInitLibWhoAmITest (
  OUT UINTN  *ProcessorNumber
  )
{
  return MpInitLibWhoAmI (ProcessorNumber);
}

STATIC
EFI_STATUS
EFIAPI
MpInitLibStartupAllAPsTest (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  VOID              *ProcedureArgument
  )
{
  return MpInitLibStartupAllAPs (Procedure, FALSE, NULL, MP_TEST_TIMEOUT_US, ProcedureArgument, NULL);
}

STATIC
EFI_STATUS
EFIAPI
MpInitLibStartupThisAPTest (
  IN  EFI_AP_PROCEDURE  Procedure,
  IN  UINTN             ProcessorNumber,
  IN  VOID              *ProcedureArgument
  )
{
  return MpInitLibStartupThisAP (Procedure, ProcessorNumber, NULL, MP_TEST_TIMEOUT_US, ProcedureArgument, NULL);
}

STATIC CONST MP_SERVICES_TEST_OPS  mMpInitLibTestOps = {
  MpInitLibGetNumberOfProcessorsTest,
  MpInitLibGetProcessorInfoTest,
  MpInitLibWhoAmITest,
  MpInitLibStartupAllAPsTest,
  MpInitLibStartupThisAPTest
};

/**
  Returns the operations that call MpInitLib directly.

  @return A pointer to the MpInitLib operations table.
**/
CONST MP_SERVICES_TEST_OPS *
GetMpInitLibTestOps (
  VOID
  )
{
  return &mMpInitLibTestOps;
}

/**
  Runs the MP test sequence against the given operations.

  @param[in] Ops   The operations to test.
  @param[in] Name  The name of the test, used in the log.

  @retval EFI_SUCCESS   Every check passed.
  @retval EFI_ABORTED   A check failed; the log names the check.
**/
EFI_STATUS
RunMpServicesTest (
  IN CONST MP_SERVICES_TEST_OPS  *Ops,
  IN CONST CHAR8                 *Name
  )
{
  EFI_STATUS                 Status;
  UINTN                      NumberOfProcessors;
  UINTN                      NumberOfEnabledProcessors;
  UINTN                      Index;
  UINTN                      BspIndex;
  UINTN                      BspCount;
  UINTN                      WhoAmIIndex;
  UINT32                     Count;
  EFI_PROCESSOR_INFORMATION  ProcessorInfo;

  DEBUG ((DEBUG_INFO, "%a: start\n", Name));

  Status = Ops->GetNumberOfProcessors (&NumberOfProcessors, &NumberOfEnabledProcessors);
  if (EFI_ERROR (Status) || (NumberOfProcessors < 1) || (NumberOfEnabledProcessors < 1) ||
      (NumberOfEnabledProcessors > NumberOfProcessors))
  {
    DEBUG ((DEBUG_ERROR, "%a: FAIL GetNumberOfProcessors %r total=%d enabled=%d\n", Name, Status, NumberOfProcessors, NumberOfEnabledProcessors));
    return EFI_ABORTED;
  }

  DEBUG ((DEBUG_INFO, "%a: processors total=%d enabled=%d\n", Name, NumberOfProcessors, NumberOfEnabledProcessors));

  BspCount = 0;
  BspIndex = 0;
  for (Index = 0; Index < NumberOfProcessors; Index++) {
    Status = Ops->GetProcessorInfo (Index, &ProcessorInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: FAIL GetProcessorInfo(%d) %r\n", Name, Index, Status));
      return EFI_ABORTED;
    }

    if ((ProcessorInfo.StatusFlag & PROCESSOR_AS_BSP_BIT) != 0) {
      BspIndex = Index;
      BspCount++;
    }
  }

  if (BspCount != 1) {
    DEBUG ((DEBUG_ERROR, "%a: FAIL expected one BSP, found %d\n", Name, BspCount));
    return EFI_ABORTED;
  }

  Status = Ops->WhoAmI (&WhoAmIIndex);
  if (EFI_ERROR (Status) || (WhoAmIIndex != BspIndex)) {
    DEBUG ((DEBUG_ERROR, "%a: FAIL WhoAmI %r index=%d expected=%d\n", Name, Status, WhoAmIIndex, BspIndex));
    return EFI_ABORTED;
  }

  if (NumberOfProcessors == 1) {
    DEBUG ((DEBUG_WARN, "%a: only the BSP is present, AP checks skipped\n", Name));
    DEBUG ((DEBUG_INFO, "%a: PASS\n", Name));
    return EFI_SUCCESS;
  }

  Count  = 0;
  Status = Ops->StartupAllAPs (IncrementCounter, &Count);
  if (EFI_ERROR (Status) || (Count != NumberOfProcessors - 1)) {
    DEBUG ((DEBUG_ERROR, "%a: FAIL StartupAllAPs %r ran=%d expected=%d\n", Name, Status, Count, NumberOfProcessors - 1));
    return EFI_ABORTED;
  }

  for (Index = 0; Index < NumberOfProcessors; Index++) {
    if (Index == BspIndex) {
      continue;
    }

    Count  = 0;
    Status = Ops->StartupThisAP (IncrementCounter, Index, &Count);
    if (EFI_ERROR (Status) || (Count != 1)) {
      DEBUG ((DEBUG_ERROR, "%a: FAIL StartupThisAP(%d) %r ran=%d\n", Name, Index, Status, Count));
      return EFI_ABORTED;
    }
  }

  DEBUG ((DEBUG_INFO, "%a: PASS\n", Name));
  return EFI_SUCCESS;
}
