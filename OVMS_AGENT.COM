$ SET NOON
$ PROCEDURE = F$ENVIRONMENT("PROCEDURE")
$ PROJECT_DEVICE = F$PARSE(PROCEDURE,,,"DEVICE")
$ PROJECT_DIRECTORY = F$PARSE(PROCEDURE,,,"DIRECTORY")
$ PROJECT_DIR = PROJECT_DEVICE + PROJECT_DIRECTORY
$ BUILD_DIR = PROJECT_DEVICE + -
              F$EXTRACT(0,F$LENGTH(PROJECT_DIRECTORY)-1,PROJECT_DIRECTORY) + -
              ".BUILD]"
$ IMAGE = F$PARSE("OVMS_AGENT.EXE", BUILD_DIR)

$ IF F$SEARCH(IMAGE) .EQS. ""
$ THEN
$     WRITE SYS$OUTPUT "%OVMS-AGENT-E-NOIMAGE, executable not found"
$     WRITE SYS$OUTPUT "Expected image: ", IMAGE
$     WRITE SYS$OUTPUT "Run @BUILD first."
$     EXIT %X10000002
$ ENDIF

$ IF P1 .NES. ""
$ THEN
$     DEFINE/USER OVMS_AGENT_ONESHOT "''P1'"
$     DEFINE/USER OVMS_AGENT_QUIET "1"
$     IF P2 .NES. "" THEN DEFINE/USER OVMS_AGENT_RESULT_MODE "''P2'"
$     DEFINE/USER SYS$INPUT _NLA0:
$ ELSE
$     DEFINE/USER SYS$INPUT SYS$COMMAND
$ ENDIF

$! M273: OpenVMS popen() defaults to CR/LF record carriage control.
$! curl --no-buffer may write SSE in small mailbox records; record-mode
$! translation can then inject newlines between bytes and destroy event
$! framing.  Scope the documented UNIX-compatible popen behavior to only
$! this image activation so the surrounding process/system is unchanged.
$ DEFINE/USER DECC$POPEN_NO_CRLF_REC_ATTR "ENABLE"
$ RUN 'IMAGE'
$ EXIT $STATUS
