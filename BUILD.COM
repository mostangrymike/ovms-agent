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
