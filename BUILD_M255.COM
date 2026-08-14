$! BUILD_M255.COM - Phase 4 iterative repair evidence
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
$ WRITE SYS$OUTPUT "Running M255 iterative-repair regressions..."
$
$ IF F$SEARCH("[.BUILD]M207_REPAIR_E2E_TEST.EXE;*") .EQS. ""
$ THEN
$     WRITE SYS$OUTPUT "M255 requires a successful @BUILD first."
$     FINAL_STATUS = %X10000002
$     GOTO FINISH
$ ENDIF
$
$ IF F$SEARCH("[.BUILD]M208_REPAIR_RETRY_TEST.EXE;*") .EQS. ""
$ THEN
$     WRITE SYS$OUTPUT "M255 requires M208 regression image from @BUILD."
$     FINAL_STATUS = %X10000002
$     GOTO FINISH
$ ENDIF
$
$ IF F$SEARCH("[.BUILD]M209_REPAIR_CONTEXT_TEST.EXE;*") .EQS. ""
$ THEN
$     WRITE SYS$OUTPUT "M255 requires M209 regression image from @BUILD."
$     FINAL_STATUS = %X10000002
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M207_REPAIR_E2E_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M255 M207 end-to-end repair evidence failed."
$     GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "M255 M207 commit/rollback evidence passed."
$
$ RUN [.BUILD]M208_REPAIR_RETRY_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M255 M208 bounded retry evidence failed."
$     GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "M255 M208 two-attempt bound evidence passed."
$
$ RUN [.BUILD]M209_REPAIR_CONTEXT_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M255 M209 retry-context evidence failed."
$     GOTO FINISH
$ ENDIF
$ WRITE SYS$OUTPUT "M255 M209 context-aware retry evidence passed."
$
$ WRITE SYS$OUTPUT "All M255 iterative-repair regressions passed."
$
$FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
