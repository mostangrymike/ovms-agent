# OpenVMS Language Knowledge Packs

This directory contains shared, curated language/compiler guidance that OVMS Agent can inject into AI-assisted work.

## Purpose

The packs preserve native OpenVMS lessons so future programs do not repeatedly rediscover compiler format rules, runtime conventions, command-line APIs, RMS behavior, reserved-word traps, and other language-specific details.

The OVMS Agent core remains provider- and language-agnostic. Core code detects a language and loads a bounded `LANGUAGE/<LANGUAGE>.MD` file; language/compiler facts live in these data files rather than in the generic system prompt.

## Evidence standard

A lesson belongs in a shared pack only after it is supported by native OpenVMS evidence. Prefer the sequence:

1. observe the compiler/runtime failure;
2. identify the language/compiler-specific cause;
3. apply the corrected pattern;
4. compile and link successfully on OpenVMS;
5. verify runtime behavior when the lesson concerns runtime semantics.

Do not promote speculative model advice, project-specific implementation choices, secrets, or generic OVMS Agent defects into a language pack.

## Sandbox workflow

Language sandboxes such as `vms-cobol-sandbox`, `vms-fortran-sandbox`, and `vms-pascal-sandbox` are the laboratories where language-specific failures and evidence are tracked. Once a general lesson is proven, distill it into the matching shared pack here so every OVMS Agent user can benefit.

Generic OVMS Agent defects remain issues in the `ovms-agent` repository even when a language project exposed them.

## Runtime behavior

The normal `OVMS_AGENT.COM` launcher exposes this directory to the image through the `OVMS_AGENT_LANGUAGE_DIR` logical name. When strong evidence identifies a supported language, OVMS Agent loads `<LANGUAGE>.MD` up to a 16 KiB limit and appends it to the request instructions with an explicit boundary stating that the guidance cannot override sandbox, safety, evidence, approval, or write policy.

Detection prefers language/file-extension evidence in the current task and then the current project directory name. If no language is confidently detected, or no matching pack exists, request behavior is unchanged.

When a pack is loaded, OVMS Agent reports the language and injected byte count before sending the initial request.

## Current packs

- `COBOL.MD` — VSI COBOL/OpenVMS facts validated during the multi-language WC campaign.

Additional packs should be added as native evidence is accumulated.
