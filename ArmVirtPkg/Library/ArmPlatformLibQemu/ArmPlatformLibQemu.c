/** @file

  Copyright (c) 2011-2012, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/ArmLib.h>
#include <Library/ArmPlatformLib.h>

#include <Ppi/ArmMpCoreInfo.h>

ARM_CORE_INFO  mQemuMpCoreInfoTable[] = {
  {
    // Cluster 0, Core 0
    0,

    // MP Core MailBox Set/Get/Clear Addresses and Clear Value
    (EFI_PHYSICAL_ADDRESS)0,
    (EFI_PHYSICAL_ADDRESS)0,
    (EFI_PHYSICAL_ADDRESS)0,
    (UINT64)0xFFFFFFFF
  },
  {
    // Cluster 0, Core 1
    1,

    // MP Core MailBox Set/Get/Clear Addresses and Clear Value
    (EFI_PHYSICAL_ADDRESS)0,
    (EFI_PHYSICAL_ADDRESS)0,
    (EFI_PHYSICAL_ADDRESS)0,
    (UINT64)0xFFFFFFFF
  },
  {
    // Cluster 0, Core 2
    2,

    // MP Core MailBox Set/Get/Clear Addresses and Clear Value
    (EFI_PHYSICAL_ADDRESS)0,
    (EFI_PHYSICAL_ADDRESS)0,
    (EFI_PHYSICAL_ADDRESS)0,
    (UINT64)0xFFFFFFFF
  },
  {
    // Cluster 0, Core 3
    3,

    // MP Core MailBox Set/Get/Clear Addresses and Clear Value
    (EFI_PHYSICAL_ADDRESS)0,
    (EFI_PHYSICAL_ADDRESS)0,
    (EFI_PHYSICAL_ADDRESS)0,
    (UINT64)0xFFFFFFFF
  },
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
  Returns the MP core information for the fixed QEMU topology.

  The table describes four cores (MPIDR 0 to 3) to match the -smp 4 test
  configuration. Booting with a different -smp count is not supported.

    ArmCoreCount  The number of entries in ArmCoreTable.
    ArmCoreTable  The MP core information table.

   EFI_SUCCESS  The table was returned.
**/
EFI_STATUS
PrePeiCoreGetMpCoreInfo (
  OUT UINTN          *ArmCoreCount,
  OUT ARM_CORE_INFO  **ArmCoreTable
  )
{
  *ArmCoreCount = sizeof (mQemuMpCoreInfoTable) / sizeof (ARM_CORE_INFO);
  *ArmCoreTable = mQemuMpCoreInfoTable;
  return EFI_SUCCESS;
}

ARM_MP_CORE_INFO_PPI  mMpCoreInfoPpi = { PrePeiCoreGetMpCoreInfo };

EFI_PEI_PPI_DESCRIPTOR  gPlatformPpiTable[] = {
  {
    EFI_PEI_PPI_DESCRIPTOR_PPI,
    &gArmMpCoreInfoPpiGuid,
    &mMpCoreInfoPpi
  }
};

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
  *PpiListSize = sizeof (gPlatformPpiTable);
  *PpiList     = gPlatformPpiTable;
}
