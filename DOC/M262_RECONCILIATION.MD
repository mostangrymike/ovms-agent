# M262 Provider and GitHub Configuration Reconciliation

M262 reconciles the previously validated provider-neutral, Requesty, persistent-provider, and saved-GitHub-profile work onto canonical M261 without replacing the newer M252-M261 implementation.

## Accepted AI provider behavior

- Named AI provider profiles are stored outside the repository in `SYS$LOGIN:OVMS_AGENT_CONFIG.DAT`.
- `PROVIDER LIST`, `SHOW`, `USE`, `ADD`, and `DELETE` manage profiles.
- `MODEL` reads and updates the active profile model.
- Service access keys are entered without terminal echo and masked in normal output.
- The configured service address can point directly to an AI provider or to an intermediary such as Requesty.
- Responses requests are built from the configured service base address.
- ASK, CHAT, AGENT, build assistance, file creation, and other AI-backed paths use the active provider.
- Multi-turn AGENT continuation is carried locally rather than depending on a provider-held previous response identifier.

OpenVMS acceptance on 2026-08-18:

- `@BUILD` passed with all regression tests.
- Startup recognized the saved Requesty profile without `OPENAI_API_KEY`.
- `ASK "What is 2 + 2?"` returned 4 through Requesty.
- Read-only AGENT completed multiple tool turns and final synthesis through Requesty.

## Accepted GitHub profile behavior

- Named GitHub profiles are stored outside the repository in `SYS$LOGIN:OVMS_AGENT_GITHUB.DAT`.
- `GITHUB LIST`, `SHOW`, `USE`, `ADD`, and `DELETE` manage profiles.
- Tokens are entered without terminal echo and masked in normal output.
- Guarded FETCH, PULL, PUSH, and CLONE use short-lived repository-scoped authentication data.
- Interactive Git credential prompting is disabled during guarded operations.
- Saved credentials are verified with a readable-reference probe before the requested operation runs.
- Temporary authentication, probe, and run files are removed across OpenVMS file versions.
- FETCH, PULL, and PUSH refuse a saved profile whose repository does not match the current checkout origin.
- Existing approval controls remain authoritative; PUSH still requires FULL approval.

OpenVMS acceptance on 2026-08-18:

- `@BUILD` passed with all regression tests after the GitHub reconciliation.
- A saved profile for another repository was refused in the OVMS Agent checkout because the checkout origin did not match the profile repository.
- In a matching external checkout, `AGENT/GITHUB/CHECK` passed and `AGENT/GITHUB/FETCH` completed successfully without an interactive credential prompt.

## Canonical preservation

- M262 is based directly on canonical M261.
- No canonical M252-M261 files are deleted by the reconciliation.
- Mature M261 workflow bodies that needed provider routing are preserved under small compatibility wrappers rather than replaced with the divergent historical tree.
- `BUILD.COM` remains authoritative.
- Newly introduced external linker-visible names remain within the 31-character DEC C/VAX limit.

README and `features.md` describe the final user-visible behavior in plain technical language.
