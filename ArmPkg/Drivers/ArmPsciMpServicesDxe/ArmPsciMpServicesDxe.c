/** @file
  ARM MP services protocol driver entry point.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>

#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/MpService.h>
#include <Ppi/ArmMpCoreInfo.h>

extern EFI_MP_SERVICES_PROTOCOL  mMpServicesProtocol;
extern EFI_STATUS
MpServicesInitialize (
  IN UINTN                 NumberOfProcessors,
  IN CONST ARM_CORE_INFO  *CoreInfo
  );

/** Initialize multi-processor support.

  @param ImageHandle  Image handle.
  @param SystemTable  System table.

  @return EFI_SUCCESS on success, or an error code.

**/
EFI_STATUS
EFIAPI
ArmPsciMpServicesDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                 Status;
  EFI_HANDLE                 Handle;
  UINTN                      MaxCpus;
  EFI_LOADED_IMAGE_PROTOCOL  *Image;
  EFI_HOB_GENERIC_HEADER     *Hob;
  VOID                       *HobData;
  UINTN                      HobDataSize;
  CONST ARM_CORE_INFO        *CoreInfo;

  MaxCpus = 1;

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&Image
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Parts of the code in this driver may be executed by other cores running
  // with the MMU off so we need to ensure that everything is clean to the
  // point of coherency (PoC)
  //
  WriteBackDataCacheRange (Image->ImageBase, Image->ImageSize);

  Hob = GetFirstGuidHob (&gArmMpCoreInfoGuid);
  if (Hob != NULL) {
    HobData     = GET_GUID_HOB_DATA (Hob);
    HobDataSize = GET_GUID_HOB_DATA_SIZE (Hob);
    CoreInfo    = (ARM_CORE_INFO *)HobData;
    MaxCpus     = HobDataSize / sizeof (ARM_CORE_INFO);
  }

  if (MaxCpus == 1) {
    DEBUG ((DEBUG_WARN, "Trying to use EFI_MP_SERVICES_PROTOCOL on a UP system"));
    // We are not MP so nothing to do
    return EFI_NOT_FOUND;
  }

  Status = MpServicesInitialize (MaxCpus, CoreInfo);
  if (Status != EFI_SUCCESS) {
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  //
  // Now install the MP services protocol.
  //
  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiMpServiceProtocolGuid,
                  &mMpServicesProtocol,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
