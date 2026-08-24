$!===========================================================
$! BUILD_M273_STREAM.COM - M273 SSE/popen regressions
$!===========================================================
$ SET NOON
$ FINAL_STATUS = 1
$
$! Compile a private JSON parser so this regression never depends on
$! a stale canonical object. LLM_SSE.OBJ is canonical and remains for
$! the final OVMS Agent link after this focused regression succeeds.
$ CC/OBJECT=[.BUILD]M273_JSON_PARSE.OBJ -
     /STANDARD=RELAXED_ANSI89 -
     /PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
     /INCLUDE_DIRECTORY=([.SRC]) -
     [.SRC]LLM_JSON_PARSE.C
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ CC/OBJECT=[.BUILD]LLM_SSE.OBJ -
     /STANDARD=RELAXED_ANSI89 -
     /PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
     /INCLUDE_DIRECTORY=([.SRC]) -
     [.SRC]LLM_SSE.C
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$! Default DEC C mode is intentional here: popen()/pclose() are
$! OpenVMS C RTL extensions and must retain their native prototypes.
$ CC/OBJECT=[.BUILD]M273_STREAM_TEST.OBJ -
     /PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
     /INCLUDE_DIRECTORY=([.SRC]) -
     [.SRC]M273_STREAM_TEST.C
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ LINK/EXECUTABLE=[.BUILD]M273_STREAM_TEST.EXE -
     [.BUILD]M273_STREAM_TEST.OBJ, -
     [.BUILD]LLM_SSE.OBJ, -
     [.BUILD]M273_JSON_PARSE.OBJ
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$! M273_STREAM_TEST now proves three parser framings: ordinary SSE,
$! missing blank separators, and a worst-case fixture with a record
$! boundary after every SSE character. The last case covers the live
$! VAX evidence where event names, data: markers, and JSON were split
$! across C RTL pipe records.
$
$! Build a child command procedure for an actual incremental-delivery
$! check. It emits one line, waits, creates a sentinel, then emits a
$! second line. The parent test must receive the first line before the
$! sentinel exists; eventual delivery after child completion is a fail.
$ IF F$SEARCH("[.BUILD]M273_STREAM_CHILD.COM;*") .NES. "" -
     THEN DELETE/NOLOG [.BUILD]M273_STREAM_CHILD.COM;*
$ IF F$SEARCH("[.BUILD]M273_STREAM_CHILD.DONE;*") .NES. "" -
     THEN DELETE/NOLOG [.BUILD]M273_STREAM_CHILD.DONE;*
$ OPEN/WRITE M273_CHILD [.BUILD]M273_STREAM_CHILD.COM
$ IF .NOT. $STATUS THEN GOTO FAIL
$ WRITE M273_CHILD "$ WRITE SYS$OUTPUT ""M273_PIPE_FIRST"""
$ WRITE M273_CHILD "$ WAIT 00:00:03"
$ WRITE M273_CHILD "$ OPEN/WRITE DONE [.BUILD]M273_STREAM_CHILD.DONE"
$ WRITE M273_CHILD "$ WRITE DONE ""DONE"""
$ WRITE M273_CHILD "$ CLOSE DONE"
$ WRITE M273_CHILD "$ WRITE SYS$OUTPUT ""M273_PIPE_SECOND"""
$ WRITE M273_CHILD "$ EXIT 1"
$ CLOSE M273_CHILD
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$! The live OVMS Agent launcher scopes the same feature to its image.
$! DECC$STREAM_PIPE selects UNIX-compatible stream I/O instead of the
$! legacy OpenVMS record-mode pipe path. Keep it /USER so this focused
$! regression does not alter the caller's process/system environment.
$ DEFINE/USER DECC$STREAM_PIPE ENABLE
$ RUN [.BUILD]M273_STREAM_TEST
$ M273_TEST_STATUS = $STATUS
$
$ IF F$SEARCH("[.BUILD]M273_STREAM_CHILD.COM;*") .NES. "" -
     THEN DELETE/NOLOG [.BUILD]M273_STREAM_CHILD.COM;*
$ IF F$SEARCH("[.BUILD]M273_STREAM_CHILD.DONE;*") .NES. "" -
     THEN DELETE/NOLOG [.BUILD]M273_STREAM_CHILD.DONE;*
$
$ IF .NOT. M273_TEST_STATUS
$ THEN
$     FINAL_STATUS = M273_TEST_STATUS
$     GOTO FAIL_DONE
$ ENDIF
$
$ WRITE SYS$OUTPUT "M273 fragmented-SSE/incremental-popen regressions passed."
$ EXIT 1
$
$ FAIL:
$ FINAL_STATUS = $STATUS
$ FAIL_DONE:
$ WRITE SYS$ERROR "M273 SSE/popen regression failed."
$ EXIT 'FINAL_STATUS'
