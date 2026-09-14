# Slippi safety policy observation

Date: 2026-09-14. The installed Slippi executable path was supplied to `GuiController.launch` with a temporary profile containing a `gameplay-changing` plugin and `mode=slippi`. Profile loading rejected the combination before emulator discovery or process creation:

```text
ManifestError profile validation failed (... profile.mode ... unsafe_combination ... offline-only content cannot be launched in slippi mode)
```

A process check found no Slippi or Dolphin process. This directly verifies the fail-closed pre-launch block against the installed Slippi path. It does not verify online login, tournament, or network behavior for a safe profile; that smoke remains unavailable.
