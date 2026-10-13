/** @file
  Shared test sequence for the MpInitLib library and the EFI_MP_SERVICES_PROTOCOL.

  Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MP_INIT_TEST_COMMON_H_
#define MP_INIT_TEST_COMMON_H_

#include <Base.h>
#include <Uefi/UefiBaseType.h>
#include <Protocol/MpService.h>

/**
  The MP operations a test sequence runs against. MpInitLib and the MP services
  protocol are both adapted to this table so that one sequence covers both.
**/
typedef struct {
  EFI_STATUS (EFIAPI *GetNumberOfProcessors)(
    OUT UINTN  *NumberOfProcessors,
    OUT UINTN  *NumberOfEnabledProcessors
    );
  EFI_STATUS (EFIAPI *GetProcessorInfo)(
    IN  UINTN                      ProcessorNumber,
    OUT EFI_PROCESSOR_INFORMATION  *ProcessorInfo
    );
  EFI_STATUS (EFIAPI *WhoAmI)(
    OUT UINTN  *ProcessorNumber
    );
  EFI_STATUS (EFIAPI *StartupAllAPs)(
    IN  EFI_AP_PROCEDURE  Procedure,
    IN  VOID              *ProcedureArgument
    );
  EFI_STATUS (EFIAPI *StartupThisAP)(
    IN  EFI_AP_PROCEDURE  Procedure,
    IN  UINTN             ProcessorNumber,
    IN  VOID              *ProcedureArgument
    );
} MP_SERVICES_TEST_OPS;

/**
  Returns the operations that call MpInitLib directly.

  @return A pointer to the MpInitLib operations table.
**/
CONST MP_SERVICES_TEST_OPS *
GetMpInitLibTestOps (
  VOID
  );

/**
  Runs the MP test sequence against the given operations.

  The sequence checks the processor count, that exactly one processor is the
  BSP and that WhoAmI reports it, that StartupAllAPs runs on every AP, and that
  StartupThisAP runs on each AP. Results are logged with DEBUG.

  @param[in] Ops   The operations to test.
  @param[in] Name  The name of the test, used in the log.

  @retval EFI_SUCCESS   Every check passed.
  @retval EFI_ABORTED   A check failed; the log names the check.
**/
EFI_STATUS
RunMpServicesTest (
  IN CONST MP_SERVICES_TEST_OPS  *Ops,
  IN CONST CHAR8                 *Name
  );

#endif
