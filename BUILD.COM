$!===========================================================
$! BUILD.COM - Build OVMS Agent
$!===========================================================
$ SET NOON
$
$ SAVED_DEFAULT = F$ENVIRONMENT("DEFAULT")
$ PROCEDURE = F$ENVIRONMENT("PROCEDURE")
$ PROJECT_DIR = F$PARSE(PROCEDURE,,,"DEVICE") + -
F$PARSE(PROCEDURE,,,"DIRECTORY")
$
$ SET DEFAULT 'PROJECT_DIR'
$ FINAL_STATUS = 1
$
$ WRITE SYS$OUTPUT "Building OVMS Agent Version 2..."
$
$ IF F$SEARCH("[]BUILD.DIR;*") .EQS. ""
$ THEN
$     CREATE/DIRECTORY [.BUILD]
$     IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$ ENDIF
$
$!-----------------------------------------------------------
$! Build modular OpenAI subsystem.
$!
$! RMS_WRITE.OBJ and EDIT_TXN.OBJ are built by this procedure
$! and listed in OPENAI_MODULES.OPT. Do not compile or link
$! them separately in this file.
$!-----------------------------------------------------------
$ @BUILD_OPENAI_MODULES
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$!-----------------------------------------------------------
$! Build core modules.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]EDIT.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]EDIT.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ CC/OBJECT=[.BUILD]MAIN.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]MAIN.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ CC/OBJECT=[.BUILD]AGENT.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]AGENT.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$!-----------------------------------------------------------
$! Build command subsystem.
$!-----------------------------------------------------------
$ @BUILD_COMMAND_MODULES
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$!-----------------------------------------------------------
$! Build remaining project modules.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]PROJECT.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]PROJECT.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ CC/OBJECT=[.BUILD]UTIL.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]UTIL.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$!-----------------------------------------------------------
$! Copy linker options files into BUILD.
$!-----------------------------------------------------------
$ COPY/NOLOG COMMAND_MODULES.OPT [.BUILD]COMMAND_MODULES.OPT
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ COPY/NOLOG OPENAI_MODULES.OPT [.BUILD]OPENAI_MODULES.OPT
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$!-----------------------------------------------------------
$! Link final executable.
$!
$! RMS_WRITE.OBJ and EDIT_TXN.OBJ are supplied exactly once
$! through OPENAI_MODULES.OPT.
$!-----------------------------------------------------------
$ LINK/EXECUTABLE=[.BUILD]OVMS_AGENT.EXE -
[.BUILD]MAIN.OBJ, -
[.BUILD]AGENT.OBJ, -
[.BUILD]COMMAND_MODULES.OPT/OPTIONS, -
[.BUILD]PROJECT.OBJ, -
[.BUILD]OPENAI_MODULES.OPT/OPTIONS, -
[.BUILD]EDIT.OBJ, -
[.BUILD]UTIL.OBJ
$
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Build EDIT_TXN_CREATE_ROLLBACK_TEST (do not run).
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]EDIT_TXN_CREATE_ROLLBACK_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]EDIT_TXN_CREATE_ROLLBACK_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]EDIT_TXN_CREATE_ROLLBACK_TEST.EXE -
[.BUILD]EDIT_TXN_CREATE_ROLLBACK_TEST.OBJ, -
[.BUILD]EDIT_TXN.OBJ, -
[.BUILD]RMS_WRITE.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed for EDIT_TXN_CREATE_ROLLBACK_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Run regression test EDIT_TXN_CREATE_ROLLBACK_TEST.
$!-----------------------------------------------------------
$ RUN [.BUILD]EDIT_TXN_CREATE_ROLLBACK_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Regression test EDIT_TXN_CREATE_ROLLBACK_TEST.EXE failed."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Build EDIT_TXN_MULTI_ROLLBACK_TEST.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]EDIT_TXN_MULTI_ROLLBACK_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]EDIT_TXN_MULTI_ROLLBACK_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]EDIT_TXN_MULTI_ROLLBACK_TEST.EXE -
[.BUILD]EDIT_TXN_MULTI_ROLLBACK_TEST.OBJ, -
[.BUILD]EDIT_TXN.OBJ, -
[.BUILD]RMS_WRITE.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed for EDIT_TXN_MULTI_ROLLBACK_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]EDIT_TXN_MULTI_ROLLBACK_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Regression test EDIT_TXN_MULTI_ROLLBACK_TEST.EXE failed."
$     GOTO FINISH
$ ENDIF
$
$
$!-----------------------------------------------------------
$! Build M205 controlled repair transaction test.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]M205_REPAIR_PATH_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M205_REPAIR_PATH_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M205_REPAIR_PATH_TEST.EXE -
[.BUILD]M205_REPAIR_PATH_TEST.OBJ, -
[.BUILD]EDIT_TXN.OBJ, -
[.BUILD]RMS_WRITE.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed for M205_REPAIR_PATH_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M205_REPAIR_PATH_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Regression test M205_REPAIR_PATH_TEST.EXE failed."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Build M206 failed-build diagnostics and repair-plan test.
$! First create a deliberately broken source and capture its
$! compiler diagnostic without failing the production build.
$!-----------------------------------------------------------
$ OPEN/WRITE M206_BAD [.BUILD]M206_BAD.C
$ WRITE M206_BAD "int main(void)"
$ WRITE M206_BAD "{"
$ WRITE M206_BAD "    return MISSING_SYMBOL;"
$ WRITE M206_BAD "}"
$ CLOSE M206_BAD
$
$ IF F$SEARCH("[.BUILD]M206_DIAGNOSTIC.TXT;*") .NES. "" -
THEN DELETE [.BUILD]M206_DIAGNOSTIC.TXT;*
$ DEFINE/USER SYS$OUTPUT [.BUILD]M206_DIAGNOSTIC.TXT
$ DEFINE/USER SYS$ERROR [.BUILD]M206_DIAGNOSTIC.TXT
$ CC/OBJECT=[.BUILD]M206_BAD.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
[.BUILD]M206_BAD.C
$ M206_BAD_STATUS = $STATUS
$ IF M206_BAD_STATUS
$ THEN
$     FINAL_STATUS = 2
$     WRITE SYS$ERROR "M206 fixture unexpectedly compiled successfully."
$     GOTO FINISH
$ ENDIF
$
$ SEARCH/OUTPUT=NL: [.BUILD]M206_DIAGNOSTIC.TXT "MISSING_SYMBOL"
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "M206 compiler diagnostic capture failed."
$     GOTO FINISH
$ ENDIF
$
$ CC/OBJECT=[.BUILD]M206_DIAGNOSTICS_PLAN_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M206_DIAGNOSTICS_PLAN_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M206_DIAGNOSTICS_PLAN_TEST.EXE -
[.BUILD]M206_DIAGNOSTICS_PLAN_TEST.OBJ, -
[.BUILD]AGENT.OBJ, -
[.BUILD]COMMAND_MODULES.OPT/OPTIONS, -
[.BUILD]PROJECT.OBJ, -
[.BUILD]OPENAI_MODULES.OPT/OPTIONS, -
[.BUILD]EDIT.OBJ, -
[.BUILD]UTIL.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed for M206_DIAGNOSTICS_PLAN_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M206_DIAGNOSTICS_PLAN_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Regression test M206_DIAGNOSTICS_PLAN_TEST.EXE failed."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Build M207 deterministic end-to-end AGENT/REPAIR test.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]M207_REPAIR_E2E_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M207_REPAIR_E2E_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M207_REPAIR_E2E_TEST.EXE -
[.BUILD]M207_REPAIR_E2E_TEST.OBJ, -
[.BUILD]AGENT.OBJ, -
[.BUILD]COMMAND_MODULES.OPT/OPTIONS, -
[.BUILD]PROJECT.OBJ, -
[.BUILD]OPENAI_MODULES.OPT/OPTIONS, -
[.BUILD]EDIT.OBJ, -
[.BUILD]UTIL.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed for M207_REPAIR_E2E_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M207_REPAIR_E2E_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Regression test M207_REPAIR_E2E_TEST.EXE failed."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Build M208 bounded two-attempt AGENT/REPAIR test.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]M208_REPAIR_RETRY_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M208_REPAIR_RETRY_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M208_REPAIR_RETRY_TEST.EXE -
[.BUILD]M208_REPAIR_RETRY_TEST.OBJ, -
[.BUILD]AGENT.OBJ, -
[.BUILD]COMMAND_MODULES.OPT/OPTIONS, -
[.BUILD]PROJECT.OBJ, -
[.BUILD]OPENAI_MODULES.OPT/OPTIONS, -
[.BUILD]EDIT.OBJ, -
[.BUILD]UTIL.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed for M208_REPAIR_RETRY_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M208_REPAIR_RETRY_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Regression test M208_REPAIR_RETRY_TEST.EXE failed."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Build M209 context-aware second-attempt AGENT/REPAIR test.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]M209_REPAIR_CONTEXT_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M209_REPAIR_CONTEXT_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M209_REPAIR_CONTEXT_TEST.EXE -
[.BUILD]M209_REPAIR_CONTEXT_TEST.OBJ, -
[.BUILD]AGENT.OBJ, -
[.BUILD]COMMAND_MODULES.OPT/OPTIONS, -
[.BUILD]PROJECT.OBJ, -
[.BUILD]OPENAI_MODULES.OPT/OPTIONS, -
[.BUILD]EDIT.OBJ, -
[.BUILD]UTIL.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed for M209_REPAIR_CONTEXT_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M209_REPAIR_CONTEXT_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Regression test M209_REPAIR_CONTEXT_TEST.EXE failed."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Build PROJECT_GREP_TEST.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]PROJECT_GREP_TEST.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]PROJECT_GREP_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]PROJECT_GREP_TEST.EXE -
[.BUILD]PROJECT_GREP_TEST.OBJ, -
[.BUILD]PROJECT.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed for PROJECT_GREP_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Run PROJECT_GREP_TEST.
$!-----------------------------------------------------------
$ RUN [.BUILD]PROJECT_GREP_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Regression test PROJECT_GREP_TEST.EXE failed."
$     GOTO FINISH
$ ENDIF
$!-----------------------------------------------------------
$! Build COMMAND_GREP_TEST.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]COMMAND_GREP_TEST.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]COMMAND_GREP_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]COMMAND_GREP_TEST.EXE -
[.BUILD]COMMAND_GREP_TEST.OBJ, -
[.BUILD]COMMAND_PROJECT.OBJ, -
[.BUILD]COMMAND.OBJ, -
[.BUILD]EDIT_TXN.OBJ, -
[.BUILD]RMS_WRITE.OBJ, -
[.BUILD]UTIL.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Link failed for COMMAND_GREP_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! Run COMMAND_GREP_TEST.
$!-----------------------------------------------------------
$ RUN [.BUILD]COMMAND_GREP_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$ERROR "Regression test COMMAND_GREP_TEST.EXE failed."
$     GOTO FINISH
$ ENDIF
$
$ WRITE SYS$OUTPUT "Build completed successfully."
$ WRITE SYS$OUTPUT "Run with: $ @OVMS_AGENT"
$ GOTO FINISH
$
$ COMPILE_ERROR:
$ FINAL_STATUS = $STATUS
$ WRITE SYS$ERROR "Compilation failed."
$
$ FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
