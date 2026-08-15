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
$ M208_LOG = "[.BUILD]M255_M208.LOG"
$ CHECK_FILE = ""
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
$! Capture M208 exactly as the validated M254 driver captures one-shot output:
$! clean all old versions, redirect only SYS$OUTPUT for the next image, then
$! resolve the actual created filespec once and reuse it for every assertion.
$ GOSUB CLEAN_M208
$ DEFINE/USER/NOLOG SYS$OUTPUT 'M208_LOG'
$ RUN [.BUILD]M208_REPAIR_RETRY_TEST
$ M208_STATUS = $STATUS
$ CHECK_FILE = F$SEARCH(M208_LOG)
$
$ IF .NOT. M208_STATUS
$ THEN
$     FINAL_STATUS = M208_STATUS
$     WRITE SYS$OUTPUT "M255 M208 bounded retry evidence failed."
$     GOTO SHOW_M208
$ ENDIF
$
$ IF CHECK_FILE .EQS. ""
$ THEN
$     FINAL_STATUS = %X10000002
$     WRITE SYS$OUTPUT "M255 M208 output capture is missing."
$     GOTO SHOW_M208
$ ENDIF
$
$ CHECK_NEEDLE = "Repair execution summary:"
$ GOSUB CHECK_TEXT
$ IF .NOT. CHECK_FOUND
$ THEN
$     FINAL_STATUS = %X10000002
$     WRITE SYS$OUTPUT "M255 repair execution summary evidence missing."
$     GOTO SHOW_M208
$ ENDIF
$
$ CHECK_NEEDLE = "Outcome:  committed"
$ GOSUB CHECK_TEXT
$ IF .NOT. CHECK_FOUND
$ THEN
$     FINAL_STATUS = %X10000002
$     WRITE SYS$OUTPUT "M255 committed terminal outcome evidence missing."
$     GOTO SHOW_M208
$ ENDIF
$
$ CHECK_NEEDLE = "Outcome:  attempt-limit"
$ GOSUB CHECK_TEXT
$ IF .NOT. CHECK_FOUND
$ THEN
$     FINAL_STATUS = %X10000002
$     WRITE SYS$OUTPUT "M255 attempt-limit terminal outcome evidence missing."
$     GOTO SHOW_M208
$ ENDIF
$
$ CHECK_NEEDLE = "Final diff:"
$ GOSUB CHECK_TEXT
$ IF .NOT. CHECK_FOUND
$ THEN
$     FINAL_STATUS = %X10000002
$     WRITE SYS$OUTPUT "M255 final diff evidence missing."
$     GOTO SHOW_M208
$ ENDIF
$
$ WRITE SYS$OUTPUT "M255 M208 two-attempt bound/report evidence passed."
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
$ GOTO FINISH
$
$SHOW_M208:
$ IF CHECK_FILE .EQS. ""
$ THEN
$     WRITE SYS$OUTPUT "M255 captured M208 output file is missing."
$ ELSE
$     WRITE SYS$OUTPUT "M255 captured M208 output:"
$     TYPE 'CHECK_FILE'
$ ENDIF
$ GOTO FINISH
$
$CHECK_TEXT:
$ CHECK_FOUND = 0
$ IF CHECK_FILE .EQS. "" THEN RETURN
$ OPEN/READ CHECK_INPUT 'CHECK_FILE'
$CHECK_TEXT_LOOP:
$ READ/END=CHECK_TEXT_DONE CHECK_INPUT CHECK_LINE
$ IF F$LOCATE(CHECK_NEEDLE,CHECK_LINE) .LT. F$LENGTH(CHECK_LINE)
$ THEN
$     CHECK_FOUND = 1
$     GOTO CHECK_TEXT_DONE
$ ENDIF
$ GOTO CHECK_TEXT_LOOP
$CHECK_TEXT_DONE:
$ CLOSE CHECK_INPUT
$ RETURN
$
$CLEAN_M208:
$ CLEAN_FILE = F$SEARCH(M208_LOG+";*")
$ IF CLEAN_FILE .EQS. "" THEN RETURN
$ DELETE/NOLOG/NOCONFIRM 'CLEAN_FILE'
$ GOTO CLEAN_M208
$
$FINISH:
$ GOSUB CLEAN_M208
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
