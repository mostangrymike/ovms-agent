$!===========================================================
$! BUILD_M259.COM - Phase 8 image-input evidence
$!===========================================================
$ SET NOON
$
$ SAVED_DEFAULT = F$ENVIRONMENT("DEFAULT")
$ PROCEDURE = F$ENVIRONMENT("PROCEDURE")
$ PROJECT_DIR = F$PARSE(PROCEDURE,,,"DEVICE") + -
                F$PARSE(PROCEDURE,,,"DIRECTORY")
$ SET DEFAULT 'PROJECT_DIR'
$ FINAL_STATUS = 1
$
$ WRITE SYS$OUTPUT "Running M259 image-input regressions..."
$
$ IF F$SEARCH("[.BUILD]OPENAI_REQUEST_BASIC.OBJ;*") .EQS. "" .OR. -
     F$SEARCH("[.BUILD]OPENAI_JSON_WRITE.OBJ;*") .EQS. "" .OR. -
     F$SEARCH("[.BUILD]OPENAI_IMAGE.OBJ;*") .EQS. ""
$ THEN
$     WRITE SYS$OUTPUT "M259 requires a successful @BUILD first."
$     FINAL_STATUS = %X10000002
$     GOTO FINISH
$ ENDIF
$
$!-----------------------------------------------------------
$! M259.1 preserve the mature text-only request baseline.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]M259_REQUEST_BASE_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M259_REQUEST_BASE_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M259_REQUEST_BASE_TEST.EXE -
[.BUILD]M259_REQUEST_BASE_TEST.OBJ, -
[.BUILD]OPENAI_REQUEST_BASIC.OBJ, -
[.BUILD]OPENAI_IMAGE.OBJ, -
[.BUILD]OPENAI_JSON_WRITE.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "Link failed for M259_REQUEST_BASE_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M259_REQUEST_BASE_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M259 text-only request baseline evidence failed."
$     GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "M259 text-only request baseline evidence passed."
$
$!-----------------------------------------------------------
$! M259.2 guarded local binary ingestion and base64 preparation.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]M259_IMAGE_INGEST_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M259_IMAGE_INGEST_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M259_IMAGE_INGEST_TEST.EXE -
[.BUILD]M259_IMAGE_INGEST_TEST.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "Link failed for M259_IMAGE_INGEST_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M259_IMAGE_INGEST_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M259 guarded image-ingestion evidence failed."
$     GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "M259 guarded image-ingestion evidence passed."
$
$!-----------------------------------------------------------
$! M259.3 multimodal Responses request serialization.
$!-----------------------------------------------------------
$ CC/OBJECT=[.BUILD]M259_REQUEST_IMAGE_TEST.OBJ -
/STANDARD=RELAXED_ANSI89 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]M259_REQUEST_IMAGE_TEST.C
$ IF .NOT. $STATUS THEN GOTO COMPILE_ERROR
$
$ LINK/EXECUTABLE=[.BUILD]M259_REQUEST_IMAGE_TEST.EXE -
[.BUILD]M259_REQUEST_IMAGE_TEST.OBJ, -
[.BUILD]OPENAI_REQUEST_BASIC.OBJ, -
[.BUILD]OPENAI_IMAGE.OBJ, -
[.BUILD]OPENAI_JSON_WRITE.OBJ
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "Link failed for M259_REQUEST_IMAGE_TEST.EXE."
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M259_REQUEST_IMAGE_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M259 multimodal request serialization evidence failed."
$     GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "M259 multimodal request serialization evidence passed."
$
$ WRITE SYS$OUTPUT "M259 command integration/metadata logging evidence remains pending."
$ WRITE SYS$OUTPUT "All implemented M259 image-input regressions passed."
$ GOTO FINISH
$
$COMPILE_ERROR:
$ FINAL_STATUS = $STATUS
$ WRITE SYS$OUTPUT "M259 focused regression compilation failed."
$
$FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
