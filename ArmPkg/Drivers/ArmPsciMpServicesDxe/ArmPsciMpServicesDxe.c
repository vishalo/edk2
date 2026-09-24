/** @file
  Install the ARM MP Services protocol.

  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>

#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/MpInitLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/MpService.h>

extern EFI_MP_SERVICES_PROTOCOL  gArmPsciMpServicesProtocol;

/**
  Initialize the ARM MP Services protocol driver.

  @param[in] ImageHandle  The image handle of this driver.
  @param[in] SystemTable  The UEFI system table.

  @retval EFI_SUCCESS           The MP Services protocol was installed.
  @retval EFI_INVALID_PARAMETER A required parameter was invalid.
  @retval EFI_NOT_FOUND         The loaded-image protocol was not found.
  @retval EFI_OUT_OF_RESOURCES  MP Services initialization failed.
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
  EFI_LOADED_IMAGE_PROTOCOL  *Image;

  (VOID)SystemTable;

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&Image
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // AP entry code can run with the MMU disabled, so make the driver image
  // visible to all processors before starting any AP.
  WriteBackDataCacheRange (Image->ImageBase, Image->ImageSize);

  Status = MpInitLibInitialize ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Handle = NULL;
  return gBS->InstallMultipleProtocolInterfaces (
                &Handle,
                &gEfiMpServiceProtocolGuid,
                &gArmPsciMpServicesProtocol,
                NULL
                );
}
