$!===========================================================
$! GIT_AUTH_SETUP.COM - Install OVMS Agent Git credential helper
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
$! Remove the flaky credential-store helper and reset any inherited
$! helper list before installing the bounded OVMS Agent helper.
$ git config --global --unset-all credential.helper
$ git config --global --add credential.helper ""
$ IF .NOT. $STATUS THEN GOTO FAIL
$ git config --global --add credential.helper -
"!MCR SYS$LOGIN:OVMS_AGENT_GH_ASKPASS.EXE"
$ IF .NOT. $STATUS THEN GOTO FAIL
$ git config --global credential.useHttpPath true
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ MCR SYS$LOGIN:OVMS_AGENT_GH_ASKPASS.EXE --check
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ WRITE SYS$OUTPUT "Git credential helper installed successfully."
$ WRITE SYS$OUTPUT -
"Normal GitHub git commands should now use the active OVMS Agent profile."
$ IF F$SEARCH("SYS$LOGIN:.git-credentials;*") .NES. ""
$ THEN
$     WRITE SYS$OUTPUT -
"Note: a legacy .git-credentials file exists but is no longer configured."
$     WRITE SYS$OUTPUT "Do not TYPE that file; it may contain credentials."
$ ENDIF
$ GOTO FINISH
$
$ FAIL:
$ FINAL_STATUS = $STATUS
$ WRITE SYS$ERROR "Unable to install OVMS Agent Git credential helper."
$
$ FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
