# OVMS Agent Language Knowledge — VSI COBOL on OpenVMS

This file contains concise language/compiler facts verified during native OpenVMS work. OVMS Agent may inject it into coding context when COBOL is detected.

## Environment validated

- OpenVMS V9.2-3 x86-64.
- VSI COBOL x86-64 V3.4-3.
- Canonical compile command for a single source file: `COBOL file.COB`.
- Canonical link command after a successful compile: `LINK file`.
- A successful VSI COBOL compile was observed with status `%X10820001`; successful LINK with `%X10000001`.
- Do not trust a successful LINK after a failed compile without proving a fresh object was produced; LINK can consume an older `.OBJ` version.

## Default source format

Plain `COBOL` uses OpenVMS terminal reference format (`/NOANSI_FORMAT`) by default.

- Area A begins in character 1.
- Area B begins in character 5.
- Division/section/paragraph headers and other Area A constructs must be positioned accordingly.
- Do not generate ANSI-format column placement unless the build explicitly uses `/ANSI_FORMAT`.
- Do not mix ANSI and terminal-reference assumptions in the same source.

## Reserved words

Do not casually use English-looking words as paragraph names without checking whether they are COBOL reserved words.

Verified failure: `REPORT.` is not a valid paragraph name on this compiler. A neutral name such as `PRINT-COUNTS.` compiled successfully.

## Runtime file names

Under the default OpenVMS COBOL rules, an unquoted COBOL word in `ASSIGN TO` is treated as an external/literal file specification, not as the value of a Working-Storage data item.

This is wrong for a runtime filename variable:

```cobol
    SELECT INFILE ASSIGN TO WS-NAME
```

It was observed to attempt to open `WS_NAME.DAT`.

A validated OpenVMS pattern is:

```cobol
    SELECT INFILE ASSIGN TO "INFILE"
        ORGANIZATION IS SEQUENTIAL
        ACCESS MODE IS SEQUENTIAL
        FILE STATUS IS WS-STATUS.
...
FD  INFILE
    VALUE OF ID IS WS-NAME
```

Here `WS-NAME` is the alphanumeric Working-Storage item containing the runtime file specification.

Do not assume the `DYNAMIC` keyword changes an `ASSIGN TO` word into a runtime data-name under these default OpenVMS rules.

## Command-line arguments

Validated `SPECIAL-NAMES` mapping:

```cobol
SPECIAL-NAMES.
    ARGUMENT-NUMBER IS POS-OF-COMMAND-LINE-ARGUMENT
    ARGUMENT-VALUE IS COMMAND-LINE-ARGUMENT.
```

`ACCEPT ... FROM COMMAND-LINE-ARGUMENT` retrieves one argument at a time; it does not return the whole command line. Do not accept one argument into a large buffer and then tokenize it as though it contained all arguments.

The most reliable loop validated in native testing is exception-terminated:

```cobol
    PERFORM UNTIL WS-ARG-DONE = "Y"
        MOVE SPACES TO WS-TOKEN
        ACCEPT WS-TOKEN FROM COMMAND-LINE-ARGUMENT
            ON EXCEPTION
                MOVE "Y" TO WS-ARG-DONE
            NOT ON EXCEPTION
                ... process exactly this argument ...
        END-ACCEPT
    END-PERFORM
```

Important:

- Never call argument-processing logic on the `ON EXCEPTION` end-of-arguments iteration.
- Do not use a position-zero sentinel; the argument position does not become zero at end of input.
- A count-driven loop produced an extra iteration in native testing; prefer the validated `ON EXCEPTION` termination pattern unless later compiler-specific evidence supersedes it.

## Record size

For the sequential text-file program validated here, `PIC X(32767)` exceeded the accepted maximum. `PIC X(32765)` compiled successfully and was used with:

```cobol
FD  INFILE
    VALUE OF ID IS WS-NAME
    RECORD IS VARYING IN SIZE FROM 0 TO 32765 CHARACTERS
    DEPENDING ON WS-RECLEN.
01  INREC PIC X(32765).
```

## Evidence discipline

These notes describe native compiler/runtime behavior verified on OpenVMS. They are guidance for generation, not proof that a particular source file currently contains the corresponding constructs. Inspect live source before editing it.

When new native evidence contradicts a rule here, update this file rather than preserving obsolete advice.
