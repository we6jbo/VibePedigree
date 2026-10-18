# October 18, 2026 publication gates

- [ ] Confirm that `~/Projects/VibePedigree` is the sole canonical Qt project, without deleting the historical Test2 companion.
- [ ] Review Git history, staged files, filenames, and content for private data, hardcoded addresses, passwords and personal records.
- [ ] Check ownership and copied-code attribution. Do not apply the project's MIT license to third-party material.
- [ ] Make `.gitignore` effective; untrack private files that are already committed separately. Consider history cleanup if private data previously leaked.
- [ ] Confirm source still compiles from clean build and the bot is installed from repository source rather than relying on `~/.Diablo/bot`.
- [ ] Implement JSON merge behavior; current `VibePedigree_Main` writer overwrites unrelated keys.
- [ ] Verify local runtime and one Task Orchestrator registration `vibepedigree` point to the approved executable.
- [ ] Commit and push from the T14; verify the actual commit timestamp and remote HEAD. Never falsify authorship or time metadata.
- [ ] Build and test Flatpak manifest and sandbox permissions; package Snap separately and review Snap Store requirements.
- [ ] Review and publish factual blog; insert its actual URL in social drafts; publish posts and video only with approval.
