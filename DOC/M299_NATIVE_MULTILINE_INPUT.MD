# M299 Native Multiline Input

M299 adds native `.END`-terminated multiline entry for interactive commands whose remaining operand is a free-form LLM prompt or goal.

## Behavior

Normal one-line forms remain immediate. For example:

```text
ASK Explain this project.
AGENT/WRITE Fix MAIN.C.
```

For an eligible command, omitting the free-form tail starts multiline collection. OVMS Agent reads physical input records until `.END` appears by itself on a line, preserving embedded newlines before the terminator.

```text
ASK
Explain the first problem.
Then compare it with the second problem.
.END
```

The same rule applies after required fixed leading arguments are present, including:

```text
AGENT/IMAGE image-path
AGENT/SESSION/EXEC <id>
AGENT/EXEC/FORK <new-name> ::
```

Structured command grammars such as DCL, RUN, PATCH, MCP, GitHub operations, and history/query commands do not gain implicit multiline behavior.

The historical compatibility aliases `AGENT/MULTILINE`, `AGENT/CREATE/MULTILINE`, and `AGENT/REPAIR/MULTILINE` remain accepted but are no longer required.

## Implementation boundary

M299 changes only the REPL trigger-selection layer. The existing M251 multiline reader remains authoritative for exact `.END` termination, embedded-newline preservation, overflow draining, and EOF handling.

The trigger selector is implemented in `SRC/COMMAND_INPUT_M299.INC` and used by `SRC/MAIN.C`. Focused deterministic coverage remains in `SRC/m251_multiline_test.c` alongside the M251 reader tests.

## Native acceptance

Exact canonical OpenVMS V9.2-3 x86-64 candidate:

- commit `0827cb80ab25e7cf6e5336a5b25396fc8ebd3b81`
- tree `a861600d498abf4b910e793e8808c82e878ac4c5`

Acceptance evidence:

- full `@BUILD` reported `All regression tests passed.` and `Build completed successfully.` with `BUILD_STATUS = "%X00000001"`;
- focused M251/M299 regression passed;
- `OVMS_AGENT.HLP` loaded with `LIBRARY/HELP/CREATE=(KEYSIZE:64)` and the `OVMS_AGENT COMMANDS MULTILINE_INPUT` topic resolved successfully;
- a live blocking-transport `ASK` entered multiline mode, waited for `.END`, and returned the requested response.

Promotion PR #180 produced canonical main `18a78a656825f6eb0841c60e1f924fc027980659`, whose tree is exactly the accepted candidate tree. Permanent tag `m299` points to that promoted main commit.

A separate pre-existing streaming completion-status defect discovered during M299 validation is tracked as issue #179 and is not part of the multiline-input implementation.
