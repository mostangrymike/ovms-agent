$!===========================================================
$! M251_BUILD_DIAG_TEST.COM
$! Verify BUILD.COM failure diagnostics without modifying sources.
$!===========================================================
$ SET NOON
$ SAVED_DEFAULT = F$ENVIRONMENT("DEFAULT")
$ PROCEDURE = F$ENVIRONMENT("PROCEDURE")
$ PROJECT_DIR = F$PARSE(PROCEDURE,,,"DEVICE") + -
  F$PARSE(PROCEDURE,,,"DIRECTORY")
$ SET DEFAULT 'PROJECT_DIR'
$
$ @BUILD M251_DIAG_FAIL
$ BUILD_STATUS = $STATUS
$
$ IF BUILD_STATUS
$ THEN
$     WRITE SYS$OUTPUT -
          "M251.10 regression failed: intentional build failure returned success."
$     SET DEFAULT 'SAVED_DEFAULT'
$     EXIT %X10000002
$ ENDIF
$
$ SEARCH/NOOUTPUT [.BUILD]OVMS_BUILD.LOG -
  "M251.10 intentional build failure diagnostic."
$ IF .NOT. $STATUS
$ THEN
$     WRITE SYS$OUTPUT -
          "M251.10 regression failed: failure diagnostic was not captured."
$     SET DEFAULT 'SAVED_DEFAULT'
$     EXIT %X10000002
$ ENDIF
$
$ WRITE SYS$OUTPUT "M251.10 build failure diagnostic regression passed."
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 1
