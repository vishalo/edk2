/** @file
  Basic runtime test for the ARM MP Services implementation.

  Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Pi/PiMultiPhase.h>
#include <Library/BaseLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/MpService.h>

STATIC volatile UINT32  mApCallbackCount;

STATIC
VOID
EFIAPI
MpServicesTestApProcedure (
  IN VOID  *Buffer
  )
{
  (VOID)Buffer;
  InterlockedIncrement ((UINT32 *)&mApCallbackCount);
}

/**
  Run the ARM MP Services runtime test.

  @param[in] ImageHandle  The image handle of this application.
  @param[in] SystemTable  The UEFI system table.

  @retval EFI_SUCCESS  All MP Services checks passed.
  @retval other        An MP Services operation or test check failed.
**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS              Status;
  EFI_MP_SERVICES_PROTOCOL  *MpServices;
  EFI_EVENT                  CompletionEvent;
  UINTN                      NumberOfProcessors;
  UINTN                      NumberOfEnabledProcessors;
  UINTN                      EventIndex;
  UINT32                     ExpectedCallbacks;

  (VOID)ImageHandle;
  (VOID)SystemTable;

  Status = gBS->LocateProtocol (
                  &gEfiMpServiceProtocolGuid,
                  NULL,
                  (VOID **)&MpServices
                  );
  if (EFI_ERROR (Status)) {
    Print (L"MP Services test: LocateProtocol failed: %r\n", Status);
    return Status;
  }

  Status = MpServices->GetNumberOfProcessors (
                         MpServices,
                         &NumberOfProcessors,
                         &NumberOfEnabledProcessors
                         );
  if (EFI_ERROR (Status)) {
    Print (L"MP Services test: GetNumberOfProcessors failed: %r\n", Status);
    return Status;
  }

  Print (
    L"MP Services test: processors=%u enabled=%u\n",
    NumberOfProcessors,
    NumberOfEnabledProcessors
    );

  if (NumberOfEnabledProcessors == 0) {
    Print (L"MP Services test: no enabled processors\n");
    return EFI_DEVICE_ERROR;
  }

  ExpectedCallbacks = NumberOfEnabledProcessors - 1;
  mApCallbackCount  = 0;
  Status = MpServices->StartupAllAPs (
                         MpServices,
                         MpServicesTestApProcedure,
                         FALSE,
                         NULL,
                         5000000,
                         NULL,
                         NULL
                         );
  if (EFI_ERROR (Status)) {
    Print (L"MP Services test: StartupAllAPs failed: %r\n", Status);
    return Status;
  }

  Print (
    L"MP Services test: AP callbacks=%u expected=%u\n",
    mApCallbackCount,
    ExpectedCallbacks
    );
  if (mApCallbackCount != ExpectedCallbacks) {
    Print (L"MP Services test: FAIL\n");
    return EFI_DEVICE_ERROR;
  }

  Status = gBS->CreateEvent (
                  0,
                  0,
                  NULL,
                  NULL,
                  &CompletionEvent
                  );
  if (EFI_ERROR (Status)) {
    Print (L"MP Services test: CreateEvent failed: %r\n", Status);
    return Status;
  }

  mApCallbackCount = 0;
  Status = MpServices->StartupAllAPs (
                         MpServices,
                         MpServicesTestApProcedure,
                         TRUE,
                         CompletionEvent,
                         5000000,
                         NULL,
                         NULL
                         );
  if (Status == EFI_UNSUPPORTED) {
    //
    // Non-blocking dispatch is unavailable after ReadyToBoot.  A UEFI Shell
    // application normally runs after that point, so fall back to blocking
    // mode while still exercising single-thread AP dispatch.
    //
    gBS->CloseEvent (CompletionEvent);
    Print (L"MP Services test: non-blocking dispatch unavailable; using blocking mode\n");

    mApCallbackCount = 0;
    Status = MpServices->StartupAllAPs (
                           MpServices,
                           MpServicesTestApProcedure,
                           TRUE,
                           NULL,
                           5000000,
                           NULL,
                           NULL
                           );
    if (EFI_ERROR (Status) || (mApCallbackCount != ExpectedCallbacks)) {
      Print (
        L"MP Services test: blocking single-thread callbacks=%u expected=%u status=%r\n",
        mApCallbackCount,
        ExpectedCallbacks,
        Status
        );
      return EFI_DEVICE_ERROR;
    }

    Print (L"MP Services test: PASS\n");
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    Print (L"MP Services test: single-thread dispatch failed: %r\n", Status);
    gBS->CloseEvent (CompletionEvent);
    return Status;
  }

  Status = gBS->WaitForEvent (1, &CompletionEvent, &EventIndex);
  gBS->CloseEvent (CompletionEvent);
  if (EFI_ERROR (Status) || (mApCallbackCount != ExpectedCallbacks)) {
    Print (
      L"MP Services test: single-thread callbacks=%u expected=%u status=%r\n",
      mApCallbackCount,
      ExpectedCallbacks,
      Status
      );
    return EFI_DEVICE_ERROR;
  }

  Print (L"MP Services test: PASS\n");
  return EFI_SUCCESS;
}
