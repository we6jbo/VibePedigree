# VibePedigree

**A local-first family-history workspace built with Qt 6 and C++17.**

VibePedigree brings together a read-only family-tree viewer, a genealogy book ledger, story writing, optional local game-companion workflows, and deliberate handoffs to a terminal-based development assistant. It aims to make records easier to explore while keeping private family material under the owner's control.

Developed in **San Diego, California**, on a **Lenovo ThinkPad T14**. This repository documents the project's state as prepared for the **October 18, 2026** release review; it does not claim that every planned feature is finished.

## What you can explore

- **Family:** Browse TG/AKA identifier records and distinguish confirmed book drops from records not yet observed. Full inventory, sale and dungeon-position telemetry remain future work.
- **Story:** Write locally stored stories associated with identifiers, with a separate real-life classification. Marking a story real is **not** consent to publish it.
- **Local Bot:** Qt can manage an optional lightweight local watcher and display green/red process status. Game-action interpretation requires additional event instrumentation.
- **Recommendations:** Light-blue navigation highlights indicate tabs that may deserve attention.
- **Terminal handoff:** Qt writes a local shell handoff and copies a terminal command; it does not start Mistral Vibe itself.
- **Restore:** Preview and instruction workflows exist. Do not assume automatic live restoration has been validated.
- **Publishing, Dragon Pipe and Web:** These are evolving workflows and not fully enabled integrations.

The application preserves older capabilities behind hidden navigation entries. A separate historic `VibePedigree-Test2` experiment is not the current application and is not required to build this repository.

## Build locally

Requirements: a C++17 compiler, CMake 3.21+, Qt 6 Widgets and Qt 6 Sql development components.

```bash
cmake -S . -B build-phase2 -DCMAKE_BUILD_TYPE=Release
cmake --build build-phase2 -j2
./build-phase2/VibePedigree
```

Do not assume the project currently works without the original author's external local paths. **Portability is unfinished**: some features reference `~/.Diablo`, protected local KinMap data, and a private Raspberry Pi. Missing sources should degrade safely, and no original personal data or proprietary game files are supplied by this repository.

## Repository boundaries and privacy

The repository contains application source, public documentation and distributable templates only. It **must not** contain private genealogy snapshots, unapproved personal information, game saves, encrypted material, credentials, private stories, locally generated observations or game data archives. Review `git ls-files` and staged changes before publishing; `.gitignore` does not remove already tracked files.

The local status file `/tmp/pedigree_status.json` is a generated runtime record, not a checked-in data file. The intended design has each process update only its own key; native testing verified that VibePedigree_Main updates while preserving VibePedigree_sh.

## Game independence

VibePedigree is a companion application, not a distribution of Diablo or DevilutionX. It does not bundle Blizzard assets, the GOG installer, MPQ data, or a modified game executable. Game interaction is optional and requires the user's separately obtained, lawful installation. The original DevilutionX project's license and third-party copyright notices govern any separate engine derivatives. Do not relicense their code as VibePedigree's. See [Third-party notices](docs/THIRD_PARTY.md).

## Verified native installation

On October 18, 2026, the native Manjaro build was tested with 113 KinMap people, 114 relationships, a running Bot, a manual Mistral handoff, shared status timestamps, and synchronized Task Orchestrator paths. This is not yet a portable public installation.

The local genealogy privacy rule protects exactly four birth years: 1981, 1960, 1926, and 1928. Approved historical records may be published; private source databases and unapproved personal details must remain excluded.

## Roadmap

1. Relocate application-owned Bot, templates and generated records out of `~/.Diablo`, without moving the protected custom game or damaging existing records.
2. Fix atomic JSON merging for `VibePedigree_Main` and `VibePedigree_sh`.
3. Package the local bot script and workflow templates as installable application resources.
4. Add reliable, opt-in inventory/pickup/drop/sale/location events to the companion ledger.
5. Verify persistence, privacy boundaries and automated tests.
6. Produce a reproducible Flatpak build, then a Snap package; validate sandbox permissions and store compliance independently.

## AI-assisted development

Developers and coding assistants can read this README and the public source to understand architecture, implementation status, and boundaries. Human review is required before changing private records, external integrations, live game files, or any publication. The MIT license permits use, modification, and redistribution of **original project-owned material** under its terms; it does not grant ownership of external code or personal information.

## Copyright and license

Copyright © 2026 Jeremiah O'Neal for original VibePedigree materials he owns. Original repository code is offered under the [MIT License](LICENSE), **subject to verifying contributor ownership and any reused source**. Third-party material retains its own copyrights and licensing; see [Third-party notices](docs/THIRD_PARTY.md). The copyright date is October 18, 2026 for this release preparation, not an assertion that every source file originated that day.

## Publishing status

A local Git commit, remote GitHub push, Flatpak installation, Snap Store listing, personal blog post and video are separate actions. They are not performed by downloading this documentation package.
