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
$! Git launches GIT_ASKPASS through the OpenVMS C RTL execvp() path.
$! On OpenVMS execvp() searches VAXC$PATH when only an image name is given.
$! Keep Git's own executable directory in the search list as well.
$ DEFINE/PROCESS VAXC$PATH SYS$LOGIN:,GIT$ROOT:[GIT_CORE]
$ DEFINE/PROCESS GIT_ASKPASS OVMS_AGENT_GH_ASKPASS.EXE
$ DEFINE/PROCESS GIT_TERMINAL_PROMPT "0"
$
$! Explicitly suppress credential-helper use for authenticated network calls.
$! Username/password prompts are answered by GIT_ASKPASS instead.
$ GITAUTH :== 'GIT' "-c" "credential.helper="
$
$ EXIT %X00000001
