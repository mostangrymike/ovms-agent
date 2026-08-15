$! BUILD_M257.COM - Phase 6 project-instruction evidence
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
$ WRITE SYS$OUTPUT "Running M257 project-instruction regressions..."
$
$ IF F$SEARCH("[.BUILD]M232_INSTR_TEST.EXE;*") .EQS. ""
$ THEN
$     WRITE SYS$OUTPUT "M257 requires a successful @BUILD first."
$     FINAL_STATUS = %X10000002
$     GOTO FINISH
$ ENDIF
$
$ RUN [.BUILD]M232_INSTR_TEST
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M257 M232 root-instruction foundation evidence failed."
$     GOTO FINISH
$ ENDIF
$
$ WRITE SYS$OUTPUT "M257 M232 root-instruction foundation evidence passed."
$ WRITE SYS$OUTPUT "M257 directory-scope/precedence evidence remains pending."
$ WRITE SYS$OUTPUT "All implemented M257 foundation regressions passed."
$
$FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
