$!===========================================================
$! GIT_AUTH_SETUP.COM - Install OVMS Agent Git auth wrapper
$!===========================================================
$ SET NOON
$
$ SAVED_DEFAULT = F$ENVIRONMENT("DEFAULT")
$ PROCEDURE = F$ENVIRONMENT("PROCEDURE")
$ PROJECT_DIR = F$PARSE(PROCEDURE,,,"DEVICE") + -
F$PARSE(PROCEDURE,,,"DIRECTORY")
$ FINAL_STATUS = 1
$
$ SET DEFAULT 'PROJECT_DIR'
$ SET PROCESS/PARSE_STYLE=EXTENDED
$
$ WRITE SYS$OUTPUT "Installing OVMS Agent Git credential helper..."
$
$ IF F$SEARCH("[]BUILD.DIR;*") .EQS. ""
$ THEN
$     CREATE/DIRECTORY [.BUILD]
$     IF .NOT. $STATUS THEN GOTO FAIL
$ ENDIF
$
$ CC/OBJECT=[.BUILD]GH_ASKPASS.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]GH_ASKPASS.C
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ CC/OBJECT=[.BUILD]GH_CONFIG_HELPER.OBJ -
/STANDARD=C99 -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]GH_CONFIG.C
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ LINK/EXECUTABLE=[.BUILD]OVMS_AGENT_GH_ASKPASS.EXE -
[.BUILD]GH_ASKPASS.OBJ, -
[.BUILD]GH_CONFIG_HELPER.OBJ
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ MCR [.BUILD]OVMS_AGENT_GH_ASKPASS.EXE --check
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ COPY/NOLOG [.BUILD]OVMS_AGENT_GH_ASKPASS.EXE -
SYS$LOGIN:OVMS_AGENT_GH_ASKPASS.EXE
$ IF .NOT. $STATUS THEN GOTO FAIL
$ PURGE/NOLOG/KEEP=1 SYS$LOGIN:OVMS_AGENT_GH_ASKPASS.EXE
$
$! Global credential-helper behavior is unreliable on the VAX Git port.
$! Clear the earlier experiment. GITAUTH below supplies the helper explicitly
$! for each invocation, matching the previously proven authenticated path.
$ git config --global --unset-all credential.helper
$ git config --global --unset credential.useHttpPath
$
$ GITAUTH :== 'GIT' "-c" "credential.helper=" "-c" -
"credential.helper=!MCR SYS$LOGIN:OVMS_AGENT_GH_ASKPASS.EXE" "-c" -
"credential.useHttpPath=true"
$
$ MCR SYS$LOGIN:OVMS_AGENT_GH_ASKPASS.EXE --check
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ WRITE SYS$OUTPUT "Git credential helper installed successfully."
$ WRITE SYS$OUTPUT "DCL wrapper command defined: GITAUTH"
$ WRITE SYS$OUTPUT -
"Use GITAUTH instead of GIT for authenticated GitHub network commands."
$ WRITE SYS$OUTPUT -
"The wrapper supplies the active OVMS Agent GitHub profile automatically."
$ GOTO FINISH
$
$ FAIL:
$ FINAL_STATUS = $STATUS
$ WRITE SYS$ERROR "Unable to install OVMS Agent Git auth wrapper."
$
$ FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
