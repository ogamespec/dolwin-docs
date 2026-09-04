# gamecube-specs — Specifications

This folder holds the **final specifications** for the GameCube hardware, written
as Markdown. They are the reference source that is later converted into the
user-friendly `docs/` folder served on GitHub Pages.

## How it is organised

- `architecture/` — the top-level architectural overview of the retail **HW2**
  (Dolphin) board, split into the major subsystems. This is the entry point.

| Topic | File |
|---|---|
| Architecture overview index | [architecture/README.md](architecture/README.md) |
| Motherboard, connectors & components | [architecture/motherboard.md](architecture/motherboard.md) |
| IBM Gekko processor | [architecture/gekko.md](architecture/gekko.md) |
| Flipper chipset | [architecture/flipper.md](architecture/flipper.md) |
| Disk Drive | [architecture/disk-drive.md](architecture/disk-drive.md) |
| Peripheral devices (EXI, SI) | [architecture/peripherals.md](architecture/peripherals.md) |

More detailed, register-level specifications for each subsystem will be added
here as they are refined, and the older `HW`/`RE`/`WEB` source folders will
eventually be retired in favour of this `specs/` tree and the rendered `docs/`.
