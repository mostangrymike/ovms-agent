$!===========================================================
$! BUILD_M260.COM - Phase 9 final practical parity validation
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
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "OVMS Agent Phase 9 final parity validation"
$ WRITE SYS$OUTPUT "Validation time: ''F$TIME()'"
$ WRITE SYS$OUTPUT "OpenVMS version: ''F$GETSYI("VERSION")'"
$ WRITE SYS$OUTPUT "Architecture: ''F$GETSYI("ARCH_NAME")'"
$ WRITE SYS$OUTPUT "Node: ''F$GETSYI("NODENAME")'"
$ WRITE SYS$OUTPUT "Operator: ''F$EDIT(F$GETJPI("","USERNAME"),"TRIM")'"
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "Compiler version:"
$ CC/VERSION
$ IF .NOT. $STATUS
$ THEN
$     FINAL_STATUS = $STATUS
$     WRITE SYS$OUTPUT "M260 unable to report compiler version."
$     GOTO FINISH
$ ENDIF
$
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "[1/9] Running canonical BUILD.COM..."
$ @BUILD
$ IF .NOT. $STATUS THEN GOTO FAILED
$
$ WRITE SYS$OUTPUT "[2/9] Running M252 guarded file-operation evidence..."
$ @BUILD_M252
$ IF .NOT. $STATUS THEN GOTO FAILED
$
$ WRITE SYS$OUTPUT "[3/9] Running M253 parity/evidence hardening..."
$ @BUILD_M253
$ IF .NOT. $STATUS THEN GOTO FAILED
$
$ WRITE SYS$OUTPUT "[4/9] Running M254 execution-mode evidence..."
$ @BUILD_M254
$ IF .NOT. $STATUS THEN GOTO FAILED
$
$ WRITE SYS$OUTPUT "[5/9] Running M255 iterative-repair evidence..."
$ @BUILD_M255
$ IF .NOT. $STATUS THEN GOTO FAILED
$
$ WRITE SYS$OUTPUT "[6/9] Running M256 session-resumption evidence..."
$ @BUILD_M256
$ IF .NOT. $STATUS THEN GOTO FAILED
$
$ WRITE SYS$OUTPUT "[7/9] Running M257 project-instruction evidence..."
$ @BUILD_M257
$ IF .NOT. $STATUS THEN GOTO FAILED
$
$ WRITE SYS$OUTPUT "[8/9] Running M258 network/external-tool evidence..."
$ @BUILD_M258
$ IF .NOT. $STATUS THEN GOTO FAILED
$
$ WRITE SYS$OUTPUT "[9/9] Running M259 image-input evidence..."
$ @BUILD_M259
$ IF .NOT. $STATUS THEN GOTO FAILED
$
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "M260 Phase 9 automated parity suite passed."
$ WRITE SYS$OUTPUT "CP-001 through CP-020 are mapped to VERIFIED evidence in doc/codex_parity.md."
$ WRITE SYS$OUTPUT "Live M259 image acceptance is recorded in doc/parity_audit_m259.md."
$ WRITE SYS$OUTPUT "Final status is an OpenVMS odd success condition."
$ GOTO FINISH
$
$FAILED:
$ FINAL_STATUS = $STATUS
$ WRITE SYS$OUTPUT ""
$ WRITE SYS$OUTPUT "M260 Phase 9 validation stopped on a failing component."
$
$FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
