$!===========================================================
$! GIT_AUTH_SETUP.COM - Install authenticated Git wrapper
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
$ WRITE SYS$OUTPUT "Installing OVMS Agent authenticated Git wrapper..."
$
$ IF F$SEARCH("[]BUILD.DIR;*") .EQS. ""
$ THEN
$     CREATE/DIRECTORY [.BUILD]
$     IF .NOT. $STATUS THEN GOTO FAIL
$ ENDIF
$
$! Use the same DEC C default mode as the proven GH_AUTH/GH_CONFIG build.
$! /STANDARD=C99 hides the OpenVMS fgetname() prototype on this VAX compiler.
$ CC/OBJECT=[.BUILD]GH_GIT_WRAP.OBJ -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]GH_GIT_WRAP.C
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ CC/OBJECT=[.BUILD]GH_CONFIG_WRAP.OBJ -
/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES -
/INCLUDE_DIRECTORY=([.SRC]) -
[.SRC]GH_CONFIG.C
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ LINK/EXECUTABLE=[.BUILD]OVMS_AGENT_GIT_AUTH.EXE -
[.BUILD]GH_GIT_WRAP.OBJ, -
[.BUILD]GH_CONFIG_WRAP.OBJ
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$! Installation validates local profile/config plumbing without requiring
$! network availability. Operator-facing GITAUTH --check performs the live
$! current-origin write-authorization preflight after installation.
$ MCR [.BUILD]OVMS_AGENT_GIT_AUTH.EXE --check-config
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ COPY/NOLOG [.BUILD]OVMS_AGENT_GIT_AUTH.EXE -
SYS$LOGIN:OVMS_AGENT_GIT_AUTH.EXE
$ IF .NOT. $STATUS THEN GOTO FAIL
$ PURGE/NOLOG/KEEP=1 SYS$LOGIN:OVMS_AGENT_GIT_AUTH.EXE
$
$ COPY/NOLOG []GIT_AUTH.COM SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM
$ IF .NOT. $STATUS THEN GOTO FAIL
$ PURGE/NOLOG/KEEP=1 SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM
$
$! Remove the earlier global credential-helper experiment.  GITAUTH carries
$! authentication only through its short-lived repository-scoped config.
$ git config --global --unset-all credential.helper
$ git config --global --unset credential.useHttpPath
$
$! OpenVMS does not provide a useful Unix executable-bit distinction for
$! ordinary repository files. Keep mode-only RMS materialization drift from
$! making an otherwise content-identical checkout appear modified.
$ git config --local core.filemode false
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ @SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ MCR SYS$LOGIN:OVMS_AGENT_GIT_AUTH.EXE --check-config
$ IF .NOT. $STATUS THEN GOTO FAIL
$
$ WRITE SYS$OUTPUT "Authenticated Git wrapper installed successfully."
$ WRITE SYS$OUTPUT "DCL wrapper command defined: GITAUTH"
$ WRITE SYS$OUTPUT -
"Use GITAUTH for GitHub network commands that need the active saved profile."
$ WRITE SYS$OUTPUT -
"Run GITAUTH --check in a repository to verify current-origin write authorization."
$ WRITE SYS$OUTPUT -
"For future logins, run @SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM after Git setup."
$ GOTO FINISH
$
$ FAIL:
$ FINAL_STATUS = $STATUS
$ WRITE SYS$ERROR "Unable to install OVMS Agent authenticated Git wrapper."
$
$ FINISH:
$ SET DEFAULT 'SAVED_DEFAULT'
$ EXIT 'FINAL_STATUS'
