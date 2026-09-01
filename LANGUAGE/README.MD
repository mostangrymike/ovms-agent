# OpenVMS Language Knowledge and Build Profiles

The `LANGUAGE` directory contains two related but separate kinds of data used by OVMS Agent:

1. **knowledge packs** (`LANGUAGE/<LANGUAGE>.MD`) — bounded native-validated compiler/runtime guidance that may be injected into AI requests; and
2. **native build profiles** (`LANGUAGE/<LANGUAGE>.BUILD`) — bounded toolchain descriptions used by the guarded `build_source` tool.

A language may have a `.BUILD` profile without having a curated `.MD` knowledge pack.

## Knowledge-pack purpose

Knowledge packs preserve native OpenVMS lessons so future projects do not repeatedly rediscover compiler format rules, runtime conventions, command-line APIs, RMS behavior, reserved-word traps, and other language-specific details.

The core remains provider- and language-agnostic: detection loads a bounded matching `.MD` file rather than hard-coding COBOL/Fortran facts into generic prompts.

## Knowledge evidence standard

A lesson belongs in a shared pack only after native OpenVMS evidence supports it. Prefer:

1. observe the compiler/runtime failure;
2. identify the language/compiler-specific cause;
3. apply the corrected pattern;
4. compile/link successfully on OpenVMS; and
5. verify runtime behavior when the lesson concerns runtime semantics.

Do not promote speculative model advice, project-specific implementation choices, secrets, or generic OVMS Agent defects into a language pack.

Language sandboxes are evidence laboratories; generic OVMS Agent defects remain issues in the `ovms-agent` repository.

## Knowledge-pack runtime behavior

`OVMS_AGENT.COM` exposes this directory through `OVMS_AGENT_LANGUAGE_DIR`. When strong task/file evidence identifies a supported language and a matching `.MD` pack exists, OVMS Agent loads up to 16 KiB and appends it to request instructions behind an explicit boundary stating that guidance cannot override sandbox, safety, evidence, approval, or write policy.

Detection prefers task/file-extension evidence and may use the current project directory name as fallback. If no language is confidently detected or no matching pack exists, request behavior is unchanged.

When a pack is loaded, the initial request reports:

```text
Language knowledge: <language> (<bytes> bytes).
```

## Current knowledge packs

- `COBOL.MD` — VSI COBOL/OpenVMS facts validated during the multi-language WC campaign.
- `FORTRAN.MD` — VSI Fortran/OpenVMS command-line and RMS text-record facts validated during the same campaign.

Both currently record validation on OpenVMS V9.2-3 x86-64 with the corresponding VSI compiler versions.

## Native build_source profiles

M289 added guarded `build_source` execution using `.BUILD` profiles. The tool is available only when full approval, guarded writes, and DCL execution are all enabled.

Shipped profiles cover:

- MACRO32;
- Fortran;
- COBOL;
- Pascal;
- C++;
- BASIC;
- Java;
- Python;
- Perl; and
- DCL.

Profile execution classes include compiled/link, compile-only, interpreted, and validated project-relative DCL command-procedure execution. Native command status and bounded captured output are returned to the agent.

These profiles do **not** replace a project's authoritative build procedure. They provide a guarded one-source native tool path when the agent needs an appropriate compiler/interpreter bridge.

## Adding or changing language data

For a new/updated `.MD` pack, require native evidence for the guidance. For a `.BUILD` profile, validate parser safety plus the native compile/link/interpret/procedure behavior appropriate to that profile class.

Keep language-specific facts here; keep generic OpenVMS Git/RMS/DCL platform lessons in their own platform documentation/knowledge work rather than mixing them into a language pack.
