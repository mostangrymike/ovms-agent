$!===========================================================
$! BUILD_M316_DIAG.COM
$! Focused M316 native compiler-diagnostic/status regression.
$!===========================================================
$ SET NOON
$ @BUILD_M289_NATIVE
$ M316_STATUS = $STATUS
$ IF .NOT. M316_STATUS THEN EXIT 'M316_STATUS'
$ WRITE SYS$OUTPUT "M316 focused compiler diagnostic regression passed."
$ EXIT 1
