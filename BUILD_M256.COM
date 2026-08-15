$! BUILD_M256.COM - Phase 5 session-resumption evidence
$ SET NOON
$
$ SAVED_DEFAULT = F$ENVIRONMENT("DEFAULT")
$ PROCEDURE = F$ENVIRONMENT("PROCEDURE")
$ PROJECT_DIR = F$PARSE(PROCEDURE,,,"DEVICE") + -
                F$PARSE(PROCEDURE,,,"DIRECTORY")
$ SET DEFAULT 'PROJECT_DIR'
$
$ FINAL_STATUS = 1
$
$ WRITE SYS$OUTPUT "Running M256 session-resumption regressions..."
$
$ IF F$SEARCH("[.BUILD]M228_SESSION_TEST.EXE;*") .EQS. ""
$ THEN
$     WRITE SYS$OUTPUT "M256 requires a successful @BUILD first."
$     FINAL_STATUS = %X10000002
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M228_SESSION_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M256 M228 persistent-session foundation evidence failed."
$     GOTO FINISH
$ ENDIF
$
$ WRITE SYS$OUTPUT "M256 M228 persistent-session foundation evidence passed."
$ WRITE SYS$OUTPUT "M256 active-plan/step/approval resume evidence remains pending."
$ WRITE SYS$OUTPUT "All implemented M256 foundation regressions passed."
$
$FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
