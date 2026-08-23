$!===========================================================
$! BUILD_M273_STREAM.COM - M273 SSE/popen regressions
$!===========================================================
$ SET NOON
$ FINAL_STATUS = 1
$
$! Compile private copies so this regression never depends on stale
$! canonical objects or changes their later build ordering.
$ CC/OBJECT=[.BUILD]M273_JSON_PARSE.OBJ -
     /STANDARD=RELAXED_ANSI89 -
     /PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
     /INCLUDE_DIRECTORY=([.SRC]) -
     [.SRC]LLM_JSON_PARSE.C
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ CC/OBJECT=[.BUILD]M273_LLM_SSE.OBJ -
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
     [.BUILD]M273_LLM_SSE.OBJ, -
     [.BUILD]M273_JSON_PARSE.OBJ
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ RUN [.BUILD]M273_STREAM_TEST
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ WRITE SYS$OUTPUT "M273 SSE/popen regressions passed."
$ EXIT 1
$
$ FAIL:
$ FINAL_STATUS = $STATUS
$ WRITE SYS$ERROR "M273 SSE/popen regression failed."
$ EXIT 'FINAL_STATUS'
