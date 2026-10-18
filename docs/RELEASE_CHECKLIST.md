# October 18, 2026 publication gates

- [ ] Confirm that `~/Projects/VibePedigree` is the sole canonical Qt project, without deleting the historical Test2 companion.
- [ ] Review Git history, staged files, filenames, and content for private data, hardcoded addresses, passwords and personal records.
- [ ] Check ownership and copied-code attribution. Do not apply the project's MIT license to third-party material.
- [ ] Make `.gitignore` effective; untrack private files that are already committed separately. Consider history cleanup if private data previously leaked.
- [ ] Confirm source still compiles from clean build and the bot is installed from repository source rather than relying on `~/.Diablo/bot`.
- [x] Verify native JSON merge behavior: `VibePedigree_Main` preserves `VibePedigree_sh`.
- [x] Verify native runtime and synchronize the `vibepedigree` Task Orchestrator registry and event paths.
- [ ] Commit and push from the T14; verify the actual commit timestamp and remote HEAD. Never falsify authorship or time metadata.
- [ ] Finish Flatpak host integration, package Snap separately, and review store requirements.
- [ ] Stage 5.5: Launch using Task Orchestrator Execute; verify shared host `/tmp/pedigree_status.json`, Bot startup, Bot shutdown, and Bot restart.
- [ ] Review and publish factual blog; insert its actual URL in social drafts; publish posts and video only with approval.
