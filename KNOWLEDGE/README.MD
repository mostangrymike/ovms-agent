# Platform knowledge packs

`KNOWLEDGE/` contains bounded, human-readable OpenVMS platform/tool guidance that OVMS Agent may inject into model instructions when strong prompt evidence identifies the topic.

Current packs:

- `OPENVMS_GIT.MD` — Git/GitHub/OpenVMS Git-port behavior and evidence discipline.
- `OPENVMS_RMS.MD` — RMS record/file-version behavior and RMS-sensitive content handling.
- `OPENVMS_DCL.MD` — DCL status, logical/symbol, path, pipe, token, and file-version lessons.

The normal launcher exposes this directory through `OVMS_AGENT_KNOWLEDGE_DIR` while preserving an explicit caller/system override. Each pack is limited to 8 KiB and the total injected platform knowledge is limited to 16 KiB.

Platform packs are guidance only. They cannot override sandbox, safety, evidence, approval, write, network, or tool policy, and they are never evidence that a current ref, file, status, logical name, symbol, or system state actually exists.

Detection is deliberately conservative. Git-specific signatures load the Git pack; RMS/record-format signatures load the RMS pack; DCL/status/logical/command-procedure signatures load the DCL pack. Unrelated source-edit prompts do not receive platform knowledge.
