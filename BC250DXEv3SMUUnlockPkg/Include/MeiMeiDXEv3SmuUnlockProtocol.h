/** @file
  MeiMeiDXEv3SmuUnlockProtocol.h

  Protocol published by the BC-250 DXEv3 SMU secure-access unlock driver.

  Consumers (the SMU patch driver and the SMU core-unlock driver) list this
  protocol GUID in their [Depex] so the DXE dispatcher will not load them
  before the unlock driver has executed.  On its entry the unlock driver
  performs the unlock only when the shared SmuUnlock configuration variable is
  enabled; consumers keep their own runtime probes to decide whether the gated
  SMU interface is actually usable.

  In addition, the protocol exposes Grant(), which performs the unlock on
  demand.  It is intended for one-shot factory-learning paths that need
  privileged SMU access even when the SmuUnlock variable is disabled; outside
  learning, callers continue to follow the normal gating.

  Copyright (C) 2026 RescueMei
  SPDX-License-Identifier: MIT
**/

#ifndef __MEIMEIDXEV3_SMU_UNLOCK_PROTOCOL_H__
#define __MEIMEIDXEV3_SMU_UNLOCK_PROTOCOL_H__

#include <Uefi.h>

typedef struct _MEIMEIDXEV3_SMU_UNLOCK_PROTOCOL  MEIMEIDXEV3_SMU_UNLOCK_PROTOCOL;

/**
  Grant privileged SMU access for this boot.

  Idempotent and unconditional: it performs the secure-access unlock sequence if
  it has not already run, and returns EFI_SUCCESS immediately when the gated
  cluster is already open.  It deliberately does NOT consult the SmuUnlock
  configuration variable, so any caller may request access regardless of that
  setting (e.g. a one-shot factory-learning path).  Normal feature operation
  still decides what to do based on the variable.

  @param[in]  This  Protocol instance pointer.

  @retval EFI_SUCCESS  Privileged SMU access is available.
  @retval others       The unlock sequence failed.
**/
typedef
EFI_STATUS
(EFIAPI *MEIMEIDXEV3_SMU_UNLOCK_GRANT)(
  IN MEIMEIDXEV3_SMU_UNLOCK_PROTOCOL  *This
  );

//
// Protocol instance.  Revision identifies the unlock sequence version; Grant
// performs the unlock on demand.
//
struct _MEIMEIDXEV3_SMU_UNLOCK_PROTOCOL {
  UINT32                        Revision;
  MEIMEIDXEV3_SMU_UNLOCK_GRANT  Grant;
};

#endif // __MEIMEIDXEV3_SMU_UNLOCK_PROTOCOL_H__
