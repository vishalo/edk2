/** @file
*
*  Copyright (c) 2011, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#pragma once

typedef struct {
  UINT64                  Mpidr;

  // MP Core Mailbox
  EFI_PHYSICAL_ADDRESS    MailboxSetAddress;
  EFI_PHYSICAL_ADDRESS    MailboxGetAddress;
  EFI_PHYSICAL_ADDRESS    MailboxClearAddress;
  UINT64                  MailboxClearValue;
} ARM_CORE_INFO;

#define ARM_MP_CORE_INFO_GUID \
  { 0xa4ee0728, 0xe5d7, 0x4ac5,  {0xb2, 0x1e, 0x65, 0x8e, 0xd8, 0x57, 0xe8, 0x34} }

extern EFI_GUID  gArmMpCoreInfoGuid;
