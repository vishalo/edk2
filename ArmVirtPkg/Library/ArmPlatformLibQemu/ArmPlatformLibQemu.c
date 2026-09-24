/** @file

  Copyright (c) 2011-2012, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/ArmLib.h>
#include <Library/ArmPlatformLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Ppi/ArmMpCoreInfo.h>

STATIC ARM_CORE_INFO  *mArmPlatformQemuMpCoreInfoTable;
STATIC UINTN          mArmPlatformQemuMpCoreCount;

/**
  Return the MP core information used by the ARM PEI core.

  @param[out] CoreCount     Number of entries in ArmCoreTable.
  @param[out] ArmCoreTable  Table describing the available cores.

  @retval EFI_SUCCESS       The MP core information was returned.
  @retval EFI_UNSUPPORTED   The platform is running on a single core.
**/
STATIC
EFI_STATUS
EFIAPI
QemuGetMpCoreInfo (
  OUT UINTN          *CoreCount,
  OUT ARM_CORE_INFO  **ArmCoreTable
  )
{
  UINT32  CpuCount;
  UINTN   Index;

  if ((CoreCount == NULL) || (ArmCoreTable == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (!ArmIsMpCore ()) {
    return EFI_UNSUPPORTED;
  }

  if (mArmPlatformQemuMpCoreInfoTable == NULL) {
    if (!QemuFwCfgIsAvailable ()) {
      return EFI_UNSUPPORTED;
    }

    QemuFwCfgSelectItem (QemuFwCfgItemSmpCpuCount);
    CpuCount = QemuFwCfgRead32 ();
    if (CpuCount == 0) {
      return EFI_DEVICE_ERROR;
    }

    mArmPlatformQemuMpCoreInfoTable = AllocateZeroPool (
                                         (UINTN)CpuCount *
                                         sizeof (ARM_CORE_INFO)
                                         );
    if (mArmPlatformQemuMpCoreInfoTable == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    for (Index = 0; Index < CpuCount; Index++) {
      mArmPlatformQemuMpCoreInfoTable[Index].Mpidr = Index;
      mArmPlatformQemuMpCoreInfoTable[Index].MailboxClearValue = MAX_UINT64;
    }

    mArmPlatformQemuMpCoreCount = CpuCount;
  }

  *CoreCount    = mArmPlatformQemuMpCoreCount;
  *ArmCoreTable = mArmPlatformQemuMpCoreInfoTable;
  return EFI_SUCCESS;
}

STATIC ARM_MP_CORE_INFO_PPI  mQemuMpCoreInfoPpi = { QemuGetMpCoreInfo };

STATIC EFI_PEI_PPI_DESCRIPTOR  mQemuPlatformPpiList[] = {
  {
    EFI_PEI_PPI_DESCRIPTOR_PPI,
    &gArmMpCoreInfoPpiGuid,
    &mQemuMpCoreInfoPpi
  }
};

/**
  Return the current Boot Mode.

  This function returns the boot reason on the platform

  @return   Return the current Boot Mode of the platform

**/
EFI_BOOT_MODE
ArmPlatformGetBootMode (
  VOID
  )
{
  return BOOT_WITH_FULL_CONFIGURATION;
}

/**
  Initialize controllers that must setup in the normal world.

  This function is called by the ArmPlatformPkg/PrePi or
  ArmPlatformPkg/PlatformPei in the PEI phase.

  @param[in]     MpId               ID of the calling CPU

  @return        RETURN_SUCCESS unless the operation failed
**/
RETURN_STATUS
ArmPlatformInitialize (
  IN  UINTN  MpId
  )
{
  return RETURN_SUCCESS;
}

/**
  Return the Platform specific PPIs.

  This function exposes the Platform Specific PPIs. They can be used by any
  PrePi modules or passed to the PeiCore by PrePeiCore.

  @param[out]   PpiListSize         Size in Bytes of the Platform PPI List
  @param[out]   PpiList             Platform PPI List

**/
VOID
ArmPlatformGetPlatformPpiList (
  OUT UINTN                   *PpiListSize,
  OUT EFI_PEI_PPI_DESCRIPTOR  **PpiList
  )
{
  if (ArmIsMpCore ()) {
    *PpiListSize = sizeof (mQemuPlatformPpiList);
    *PpiList     = mQemuPlatformPpiList;
  } else {
    *PpiListSize = 0;
    *PpiList     = NULL;
  }
}
