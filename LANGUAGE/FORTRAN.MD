# OVMS Agent Language Knowledge — VSI Fortran on OpenVMS

This file contains concise language/compiler facts verified during native OpenVMS work. OVMS Agent may inject it into coding context when Fortran is detected.

## Environment validated

- OpenVMS V9.2-3 x86-64.
- VSI Fortran x86-64 V8.7-001.
- A single free-form `.F90` source compiled successfully with `FORTRAN file.F90`.
- Canonical link command after a successful compile: `LINK file`.
- A successful compile was observed with status `%X186A0001`; successful LINK with `%X10000001`.

## OpenVMS command line

A validated way to obtain the complete foreign-command text is `LIB$GET_FOREIGN`.

The accepted WC program used the equivalent of:

```fortran
integer*2 :: command_length
integer :: foreign_status
integer, external :: lib$get_foreign
character(len=1024) :: command

foreign_status = lib$get_foreign(command, , command_length)
if (iand(foreign_status, 1) == 0) then
   ! handle failure
end if
```

OpenVMS command arguments for an image normally require foreign-command invocation (for example a DCL symbol of the form `WC :== $device:[dir]WC.EXE`). Do not assume Unix `argv` behavior without compiler/runtime evidence.

## Sequential formatted RMS text

A validated record-reading pattern is:

```fortran
open(unit=unit, file=name, access='sequential', form='formatted', &
     status='old', action='read', iostat=status)
...
read(unit, '(Q,A)', iostat=read_status) record_length, record
```

For the WC contract, `Q` supplied the record-data length. The logical POSIX-style byte count was implemented as `record_length + 1` for each OpenVMS text record, treating the record boundary as one LF byte.

`SYS$INPUT` worked as the no-file input name for the accepted program.

## Word boundaries across RMS records

A record boundary is whitespace in the logical text stream. Do not carry an `in_word` state from the final non-whitespace character of one RMS record into the first character of the next record.

Native testing exposed this exact bug. The accepted implementation resets word state at the end of every record after scanning its record data.

For ASCII-oriented whitespace, the accepted WC implementation treated character codes 9, 10, 13, and 32 as whitespace while scanning record data; the record boundary itself was handled separately by resetting word state.

## Record buffer

`character(len=32767)` compiled and was used successfully as the sequential text-record buffer in the accepted WC implementation. Do not transfer COBOL record-size restrictions to Fortran without native evidence; compiler limits are language-specific.

## Evidence discipline

These notes describe native compiler/runtime behavior verified on OpenVMS. They are guidance for generation, not proof that a particular source file currently contains the corresponding constructs. Inspect live source before editing it.

When new native evidence contradicts a rule here, update this file rather than preserving obsolete advice.
