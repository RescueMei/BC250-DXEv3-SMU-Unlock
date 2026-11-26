# BC250-DXEv3-SMU-Unlock

DXE driver for the BC-250 that opens the SMU's gated Q3 secure-access cluster, so
later DXE drivers can perform arbitrary SMU-local reads and writes. It is a C
port of rw-r-r-0644's Python proof of concept and targets **BIOS 3.00 / Robin 3**
only; BIOS 5 firmware uses different offsets and is not supported.

> **Firmware warning:** a bad SMU write can leave the SMU unresponsive and
> require a cold power cycle. Keep a recovery path before enabling this driver.

## At a glance

| Item | Value |
| --- | --- |
| Package / module | `BC250DXEv3SMUUnlockPkg` -> `MeiMeiDXEv3_SMU_Unlock.efi` |
| Depex | `gEfiVariableArchProtocolGuid` |
| Produces | `gMeiMeiDXEv3SmuUnlockProtocolGuid` (`Grant`) |
| Consumes | `MeiMeiDXEv3SmuUnlockVar`, shared config ABI |
| Configuration | `MeiMeiDXEv3SmuUnlockVar`, 1 byte |
| SMU SRAM | none (the exploit targets firmware-internal state, not a shared lane) |
| Compliance | [COMPLIANCE.md](COMPLIANCE.md) |

## Dependencies

Only variable services are required for dispatch. The drivers that need
privileged SMU access (`SMU Patch`, `Core Unlock`, `CU Unlock`, `SMU Governor`)
list `gMeiMeiDXEv3SmuUnlockProtocolGuid` in their own `[Depex]`, so they cannot
run before this driver has installed the protocol.

## How it works

The unlock exploits a bug in SMU Queue 2 message `0x23` (subqueue-ring append).
The overflow redirects the SMU's transfer-table pointer so a fake transfer table
can be staged in SMU-local SRAM. That table then drives the ungated Q2 message
`0x0A` transfer-engine DMA interface to zero the debug-disable guard word at
SMU-local `0x7B3C`. With the guard removed, the gated Q3 messages become usable:

- `0x28` / `0x29` — arbitrary SMU-local SRAM read/write
- `0x2A` / `0x2B` / `0x2C` — arbitrary SMN read/write through the mem64 window

At dispatch time the driver:

1. Reads `MeiMeiDXEv3SmuUnlockVar` through the shared configuration ABI.
2. If the variable is missing, undersized, wrong-attribute, or has
   `SmuUnlock == 0`, it logs and returns without touching the SMU.
3. Otherwise it verifies the SMU is alive, probes the gate, and returns early if
   the cluster is already open.
4. It locates a zero-filled dead zone, stages the fake transfer table in DRAM,
   DMA's it into SMU SRAM, re-arms the table, and clears the guard word.
5. It verifies the gate is open and the SMU is still alive.
6. It restores the SMU state so only the guard word differs from its boot value.

The driver is fire-and-forget: every failure is logged and the normal boot path
continues.

## Configuration

The driver consumes the shared `SMU_UNLOCK_CONFIG` record:

- Header: `BC250-DXEv3-Shared/MeiMeiDXEv3SharedPkg/Include/Config/MeiMeiDXEv3Config.h`
- Variable: `MEIMEIDXEV3_SMU_UNLOCK_VAR_NAME` (`MeiMeiDXEv3SmuUnlockVar`)
- Attributes: `MEIMEIDXEV3_CONFIG_VAR_ATTRIBUTES`
  (boot service + runtime + non-volatile)
- Field: `SmuUnlock` at offset 0

Missing, short, unreadable, or wrong-attribute variables all leave the unlock
disabled; the boot continues.

## Protocols

Also publishes `gMeiMeiDXEv3DriverInfoProtocolGuid` with its name, version, module GUID, config schema, and payload version for the `BC250-DXEv3-Driver-Info` telemetry driver.

`gMeiMeiDXEv3SmuUnlockProtocolGuid`
(`A7C3D5E1-94B2-4C6D-8F1A-3B5D7E9C2F68`), header
`<MeiMeiDXEv3SmuUnlockProtocol.h>`.

The single method `Grant()` is idempotent and unconditional: it performs the
unlock sequence even when the configuration variable is disabled, which is what
the one-shot factory-learning paths need. Normal feature operation still gates
on the variable. The protocol is installed before any early return.

```c
#include <Library/UefiBootServicesTableLib.h>
#include <MeiMeiDXEv3SmuUnlockProtocol.h>

MEIMEIDXEV3_SMU_UNLOCK_PROTOCOL  *Unlock;

Status = gBS->LocateProtocol (&gMeiMeiDXEv3SmuUnlockProtocolGuid, NULL,
                              (VOID **)&Unlock);
if (!EFI_ERROR (Status)) {
  Status = Unlock->Grant (Unlock);
}
```

## SMU SRAM placement

This driver owns no lane in the shared map. The guard word and transfer-table
state it touches are firmware-internal fields, not scratch space. See
[SMU_MEMORY_MAP.md](https://github.com/RescueMei/BC250-DXEv3-Shared/blob/main/docs/SMU_MEMORY_MAP.md).

## Build

```bash
./build_ffs.sh                                  # container build
NO_CONTAINER=1 EDK2_DIR=/path/to/edk2 ./build_ffs.sh   # host build
```

Output:

- `Build/Output/MeiMeiDXEv3_SMU_Unlock.efi`
- `Build/Output/MeiMeiDXEv3_SMU_Unlock.ffs`

The `.ffs` can be inserted into an AMI DXE firmware volume with UEFITool.

## Validation

2026-11-24, eight-driver stack, configured ceiling 2100 MHz: `DEPLOY_OK`,
actual clock 2100 MHz sustained, rail voltage <= 1018 mV, 193 W GPU peak, 82 C
peak, 0 ring timeouts, 0 C2P timeouts, board reachable after 60 s Valley + 60 s
Superposition. See [COMPLIANCE.md](COMPLIANCE.md) for the checklist.

## Credits

- [rw-r-r-0644](https://github.com/rw-r-r-0644) for the original SMU unlock
  implementation and research.
