$!===========================================================
$! GIT_AUTH.COM - Enable OVMS Agent Git askpass authentication
$!===========================================================
$ SET NOON
$
$ IF F$TYPE(GIT) .EQS. ""
$ THEN
$     WRITE SYS$ERROR "GIT symbol is not defined. Run the Git setup first."
$     EXIT %X00000002
$ ENDIF
$
$ IF F$SEARCH("SYS$LOGIN:OVMS_AGENT_GH_ASKPASS.EXE;*") .EQS. ""
$ THEN
$     WRITE SYS$ERROR "OVMS Agent Git askpass executable is not installed."
$     EXIT %X00000002
$ ENDIF
$
$! Git performs its own PATH lookup before execve(), so VAXC$PATH is not
$! consulted here. Give Git a path containing '/' so it skips that lookup.
$! The OpenVMS C RTL translates sys$login/FILE to SYS$LOGIN:FILE.
$ DEFINE/PROCESS GIT_ASKPASS "sys$login/OVMS_AGENT_GH_ASKPASS.EXE"
$ DEFINE/PROCESS GIT_TERMINAL_PROMPT "0"
$
$! Explicitly suppress credential-helper use for authenticated network calls.
$! Username/password prompts are answered by GIT_ASKPASS instead.
$ GITAUTH :== 'GIT' "-c" "credential.helper="
$
$ EXIT %X00000001
