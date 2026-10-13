/** @file
  MP Initialize Library helpers shared by the AArch64 DXE and PEI instances.

  The PSCI based bring-up of secondary cores, the AP entry procedure and the
  processor information setup are common to both phases. The DXE instance
  layers EFI events on top of these helpers, and the PEI instance performs
  blocking calls only.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2011, Apple Inc. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/ArmLib.h>
#include <Library/ArmMmuLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/CpuExceptionHandlerLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MpInitLib.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Guid/ArmMpCoreInfo.h>

#include "MpLib.h"

CPU_MP_DATA  mCpuMpData;
UINT64       *gApStacksBase;
UINT64       *gProcessorIDs;
CONST UINT64 gApStackSize = AP_STACK_SIZE;
VOID         *gTtbr0;
UINTN        gTcr;
UINTN        gMair;
UINTN        gHcr;

/** Turns on the specified core using PSCI and executes the user-supplied
    function that's been configured via a previous call to SetApProcedure.

   @param ProcessorIndex The index of the core to turn on.

   @retval EFI_SUCCESS      Success.
   @retval EFI_NOT_READY    The processor is already on.
   @retval EFI_DEVICE_ERROR The processor could not be turned on.

**/
EFI_STATUS
EFIAPI
DispatchCpu (
  IN UINTN  ProcessorIndex
  )
{
  ARM_SMC_ARGS  Args;
  EFI_STATUS    Status;

  Status = EFI_SUCCESS;

  mCpuMpData.CpuData[ProcessorIndex].State = CpuStateBusy;

  /* Turn the AP on */
  if (sizeof (Args.Arg0) == sizeof (UINT32)) {
    Args.Arg0 = ARM_SMC_ID_PSCI_CPU_ON_AARCH32;
  } else {
    Args.Arg0 = ARM_SMC_ID_PSCI_CPU_ON_AARCH64;
  }

  Args.Arg1 = gProcessorIDs[ProcessorIndex];
  Args.Arg2 = (UINTN)ApEntryPoint;

  ArmCallSmc (&Args);

  if (Args.Arg0 == ARM_SMC_PSCI_RET_ALREADY_ON) {
    Status = EFI_NOT_READY;
  } else if (Args.Arg0 != ARM_SMC_PSCI_RET_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "PSCI_CPU_ON call failed: %d\n", Args.Arg0));
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}


/**
  This function is called by all processors (both BSP and AP) once and collects
  MP related data.

  @param BSP            TRUE if the processor is the BSP.
  @param Mpidr          The MPIDR for the specified processor. This should be
                        the full MPIDR and not only the affinity bits.
  @param ProcessorIndex The index of the processor.

  @return EFI_SUCCESS if the data for the processor collected and filled in.

**/
STATIC
EFI_STATUS
FillInProcessorInformation (
  IN BOOLEAN  BSP,
  IN UINTN    Mpidr,
  IN UINTN    ProcessorIndex
  )
{
  EFI_PROCESSOR_INFORMATION  *CpuInfo;

  CpuInfo = &mCpuMpData.CpuData[ProcessorIndex].Info;

  CpuInfo->ProcessorId = GET_MPIDR_AFFINITY_BITS (Mpidr);
  CpuInfo->StatusFlag  = PROCESSOR_ENABLED_BIT | PROCESSOR_HEALTH_STATUS_BIT;

  if (BSP) {
    CpuInfo->StatusFlag |= PROCESSOR_AS_BSP_BIT;
  }

  if ((Mpidr & MPIDR_MT_BIT) > 0) {
    CpuInfo->Location.Package = GET_MPIDR_AFF2 (Mpidr);
    CpuInfo->Location.Core    = GET_MPIDR_AFF1 (Mpidr);
    CpuInfo->Location.Thread  = GET_MPIDR_AFF0 (Mpidr);

    CpuInfo->ExtendedInformation.Location2.Package = GET_MPIDR_AFF3 (Mpidr);
    CpuInfo->ExtendedInformation.Location2.Die     = GET_MPIDR_AFF2 (Mpidr);
    CpuInfo->ExtendedInformation.Location2.Core    = GET_MPIDR_AFF1 (Mpidr);
    CpuInfo->ExtendedInformation.Location2.Thread  = GET_MPIDR_AFF0 (Mpidr);
  } else {
    CpuInfo->Location.Package = GET_MPIDR_AFF1 (Mpidr);
    CpuInfo->Location.Core    = GET_MPIDR_AFF0 (Mpidr);
    CpuInfo->Location.Thread  = 0;

    CpuInfo->ExtendedInformation.Location2.Package = GET_MPIDR_AFF2 (Mpidr);
    CpuInfo->ExtendedInformation.Location2.Die     = GET_MPIDR_AFF1 (Mpidr);
    CpuInfo->ExtendedInformation.Location2.Core    = GET_MPIDR_AFF0 (Mpidr);
    CpuInfo->ExtendedInformation.Location2.Thread  = 0;
  }

  mCpuMpData.CpuData[ProcessorIndex].State = BSP ? CpuStateBusy : CpuStateIdle;

  mCpuMpData.CpuData[ProcessorIndex].Procedure = NULL;
  mCpuMpData.CpuData[ProcessorIndex].Parameter = NULL;

  return EFI_SUCCESS;
}

/**
  Allocates and initializes the MP data shared by the DXE and PEI instances.

  @param[in] NumberOfProcessors  The number of processors described by CoreInfo.
  @param[in] CoreInfo            The MPIDR of each processor, from the ARM MP core info HOB.

  @retval EFI_SUCCESS            The MP data was initialized.
  @retval EFI_OUT_OF_RESOURCES   Memory for the MP data could not be allocated.

**/
EFI_STATUS
InitializeMpCommonData (
  IN UINTN                NumberOfProcessors,
  IN CONST ARM_CORE_INFO  *CoreInfo
  )
{
  UINTN    Index;
  BOOLEAN  IsBsp;

  //
  // Clear the data structure area first.
  //
  ZeroMem (&mCpuMpData, sizeof (CPU_MP_DATA));
  //
  // First BSP fills and inits all known values, including its own records.
  //
  mCpuMpData.NumberOfProcessors        = NumberOfProcessors;
  mCpuMpData.NumberOfEnabledProcessors = NumberOfProcessors;

  mCpuMpData.CpuData = AllocateZeroPool (
                         mCpuMpData.NumberOfProcessors * sizeof (CPU_AP_DATA)
                         );

  if (mCpuMpData.CpuData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  /* Allocate one extra for the sentinel entry at the end */
  gProcessorIDs = AllocateZeroPool ((mCpuMpData.NumberOfProcessors + 1) * sizeof (UINT64));
  ASSERT (gProcessorIDs != NULL);

  gApStacksBase = AllocatePages (
                    EFI_SIZE_TO_PAGES (
                      mCpuMpData.NumberOfProcessors *
                      gApStackSize
                      )
                    );
  ASSERT (gApStacksBase != NULL);

  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    if (GET_MPIDR_AFFINITY_BITS (ArmReadMpidr ()) == GET_MPIDR_AFFINITY_BITS (CoreInfo[Index].Mpidr)) {
      IsBsp = TRUE;
    } else {
      IsBsp = FALSE;
    }

    FillInProcessorInformation (IsBsp, CoreInfo[Index].Mpidr, Index);

    gProcessorIDs[Index] = mCpuMpData.CpuData[Index].Info.ProcessorId;
  }

  gProcessorIDs[Index] = MAX_UINT64;

  gTcr   = ArmGetTCR ();
  gMair  = ArmGetMAIR ();
  gTtbr0 = ArmGetTTBR0BaseAddress ();
  if (ArmReadCurrentEL () == AARCH64_EL2) {
    gHcr = ArmReadHcr ();
  }

  //
  // The global pointer variables as well as the gProcessorIDs array contents
  // are accessed by the other cores so we must clean them to the PoC
  //
  WriteBackDataCacheRange (&gProcessorIDs, sizeof (UINT64 *));
  WriteBackDataCacheRange (&gApStacksBase, sizeof (UINT64 *));
  WriteBackDataCacheRange (&gHcr, sizeof (gHcr));

  WriteBackDataCacheRange (
    gProcessorIDs,
    (mCpuMpData.NumberOfProcessors + 1) * sizeof (UINT64)
    );

  return EFI_SUCCESS;
}

/**
  Return the handle number for the calling processor.

  @param[out] ProcessorNumber  The handle number of the calling processor.

  @retval EFI_SUCCESS             The current processor handle number was returned
                                  in ProcessorNumber.
  @retval EFI_INVALID_PARAMETER   ProcessorNumber is NULL.

**/
EFI_STATUS
EFIAPI
MpInitLibWhoAmI (
  OUT UINTN  *ProcessorNumber
  )
{
  UINTN   Index;
  UINT64  ProcessorId;

  if (ProcessorNumber == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ProcessorId = GET_MPIDR_AFFINITY_BITS (ArmReadMpidr ());
  for (Index = 0; Index < mCpuMpData.NumberOfProcessors; Index++) {
    if (ProcessorId == gProcessorIDs[Index]) {
      *ProcessorNumber = Index;
      break;
    }
  }

  return EFI_SUCCESS;
}

/** Exception handler for APs. Marks the AP as finished and turns it off.

  @param InterruptType  Type of interrupt or exception.
  @param SystemContext  System context.

**/
STATIC
VOID
EFIAPI
ApExceptionHandler (
  IN CONST EFI_EXCEPTION_TYPE  InterruptType,
  IN CONST EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  ARM_SMC_ARGS  Args;
  UINT64        Mpidr;
  UINTN         Index;
  UINTN         ProcessorIndex;

  Mpidr = GET_MPIDR_AFFINITY_BITS (ArmReadMpidr ());

  Index          = 0;
  ProcessorIndex = MAX_UINT64;

  do {
    if (gProcessorIDs[Index] == Mpidr) {
      ProcessorIndex = Index;
      break;
    }

    Index++;
  } while (gProcessorIDs[Index] != MAX_UINT64);

  if (ProcessorIndex != MAX_UINT64) {
    mCpuMpData.CpuData[ProcessorIndex].State = CpuStateFinished;
    ArmDataMemoryBarrier ();
  }

  Args.Arg0 = ARM_SMC_ID_PSCI_CPU_OFF;
  ArmCallSmc (&Args);

  /* Should never be reached */
  ASSERT (FALSE);
  CpuDeadLoop ();
}

/** C entry-point for the AP.
    This function gets called from the assembly function ApEntryPoint.

**/
VOID
ApProcedure (
  VOID
  )
{
  ARM_SMC_ARGS      Args;
  EFI_AP_PROCEDURE  UserApProcedure;
  VOID              *UserApParameter;
  UINTN             ProcessorIndex;

  ProcessorIndex = 0;

  MpInitLibWhoAmI (&ProcessorIndex);

  /* Fetch the user-supplied procedure and parameter to execute */
  UserApProcedure = mCpuMpData.CpuData[ProcessorIndex].Procedure;
  UserApParameter = mCpuMpData.CpuData[ProcessorIndex].Parameter;

  InitializeCpuExceptionHandlers (NULL);
  RegisterCpuInterruptHandler (EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS, ApExceptionHandler);
  RegisterCpuInterruptHandler (EXCEPT_AARCH64_IRQ, ApExceptionHandler);
  RegisterCpuInterruptHandler (EXCEPT_AARCH64_FIQ, ApExceptionHandler);
  RegisterCpuInterruptHandler (EXCEPT_AARCH64_SERROR, ApExceptionHandler);

  UserApProcedure (UserApParameter);

  mCpuMpData.CpuData[ProcessorIndex].State = CpuStateFinished;

  ArmDataMemoryBarrier ();

  /* Since we're finished with this AP, turn it off */
  Args.Arg0 = ARM_SMC_ID_PSCI_CPU_OFF;
  ArmCallSmc (&Args);

  /* Should never be reached */
  ASSERT (FALSE);
  CpuDeadLoop ();
}

