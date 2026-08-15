$! BUILD_M258.COM - Phase 7 network/external-tool evidence
$ SET NOON
$
$ SAVED_DEFAULT = F$ENVIRONMENT("DEFAULT")
$ PROCEDURE = F$ENVIRONMENT("PROCEDURE")
$ PROJECT_DIR = F$PARSE(PROCEDURE,,,"DEVICE") + -
                F$PARSE(PROCEDURE,,,"DIRECTORY")
$ SET DEFAULT 'PROJECT_DIR'
$
$ FINAL_STATUS = 1
$ WRITE SYS$OUTPUT "Running M258 network/external-tool regressions..."
$
$ IF F$SEARCH("[.BUILD]M241_MCP_CATALOG_TEST.EXE;*") .EQS. ""
$ THEN
$     WRITE SYS$OUTPUT "M258 requires a successful @BUILD first."
$     FINAL_STATUS = %X10000002
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M241_MCP_CATALOG_TEST
$ IF .NOT. $STATUS THEN GOTO TEST_ERROR
$ WRITE SYS$OUTPUT "M258 M241 MCP catalog evidence passed."
$
$ RUN [.BUILD]M242_MCP_CALL_TEST
$ IF .NOT. $STATUS THEN GOTO TEST_ERROR
$ WRITE SYS$OUTPUT "M258 M242 guarded-call evidence passed."
$
$ RUN [.BUILD]M243_MCP_EXEC_TEST
$ IF .NOT. $STATUS THEN GOTO TEST_ERROR
$ WRITE SYS$OUTPUT "M258 M243 stdio execution evidence passed."
$
$ RUN [.BUILD]M244_MCP_HTTP_TEST
$ IF .NOT. $STATUS THEN GOTO TEST_ERROR
$ WRITE SYS$OUTPUT "M258 M244 HTTP transport evidence passed."
$
$ RUN [.BUILD]M245_MCP_SSE_TEST
$ IF .NOT. $STATUS THEN GOTO TEST_ERROR
$ WRITE SYS$OUTPUT "M258 M245 SSE transport evidence passed."
$
$ RUN [.BUILD]M246_MCP_RESULT_TEST
$ IF .NOT. $STATUS THEN GOTO TEST_ERROR
$ WRITE SYS$OUTPUT "M258 M246 normalized-result evidence passed."
$
$ RUN [.BUILD]M247_MCP_FEEDBACK_TEST
$ IF .NOT. $STATUS THEN GOTO TEST_ERROR
$ WRITE SYS$OUTPUT "M258 M247 agent-feedback evidence passed."
$
$!-----------------------------------------------------------
$! M258.2 default-deny network policy engine.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]M258_NET_POLICY_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M258_NET_POLICY_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M258_NET_POLICY_TEST.EXE -
[.BUILD]M258_NET_POLICY_TEST.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "Link failed for M258_NET_POLICY_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M258_NET_POLICY_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M258 network-policy evidence failed."
$     GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "M258 default-deny network-policy evidence passed."
$
$!-----------------------------------------------------------
$! M258.3 enforce policy in the real MCP HTTP/SSE entry points.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]M258_MCP_NET_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M258_MCP_NET_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M258_MCP_NET_TEST.EXE -
[.BUILD]M258_MCP_NET_TEST.OBJ, -
[.BUILD]AGENT.OBJ, -
[.BUILD]COMMAND_MODULES.OPT/OPTIONS, -
[.BUILD]PROJECT.OBJ, -
[.BUILD]OPENAI_MODULES.OPT/OPTIONS, -
[.BUILD]EDIT.OBJ, -
[.BUILD]UTIL.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "Link failed for M258_MCP_NET_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M258_MCP_NET_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M258 MCP network-policy integration evidence failed."
$     GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "M258 MCP network-policy integration evidence passed."
$
$ WRITE SYS$OUTPUT "M258 lifecycle/timeout/logging evidence remains pending."
$ WRITE SYS$OUTPUT "All implemented M258 network/external-tool regressions passed."
$ GOTO FINISH
$
$COMPILE_ERROR:
$ FINAL_STATUS = $STATUS
$ WRITE SYS$OUTPUT "M258 focused regression compilation failed."
$ GOTO FINISH
$
$TEST_ERROR:
$ FINAL_STATUS = $STATUS
$ WRITE SYS$OUTPUT "M258 existing external-tool regression failed."
$
$FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
