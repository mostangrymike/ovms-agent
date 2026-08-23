$!===========================================================
$! GIT_AUTH.COM - Enable OVMS Agent authenticated Git wrapper
$!===========================================================
$ SET NOON
$
$ IF F$TYPE(GIT) .EQS. ""
$ THEN
$     WRITE SYS$ERROR "GIT symbol is not defined. Run the Git setup first."
$     EXIT %X00000002
$ ENDIF
$
$ IF F$SEARCH("SYS$LOGIN:OVMS_AGENT_GIT_AUTH.EXE;*") .EQS. ""
$ THEN
$     WRITE SYS$ERROR "OVMS Agent authenticated Git wrapper is not installed."
$     EXIT %X00000002
$ ENDIF
$
$! The wrapper creates the same short-lived repository-scoped extraHeader
$! authentication config used by guarded AGENT/GITHUB network operations.
$! No credential value is placed in this DCL symbol or Git command line.
$ GITAUTH :== $SYS$LOGIN:OVMS_AGENT_GIT_AUTH.EXE
$
$ EXIT %X00000001
