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

$! M278: make the shipped GitHub service bridge available to a normal
$! @OVMS_AGENT launch.  Preserve any explicit caller/system override.
$ IF F$TRNLNM("OVMS_AGENT_GITHUB_BRIDGE") .EQS. ""
$ THEN
$     BRIDGE_FILE = PROJECT_DIR + "GITHUB_BRIDGE.COM"
$     IF F$SEARCH(BRIDGE_FILE) .NES. ""
$     THEN
$         BRIDGE_COMMAND = "@" + BRIDGE_FILE
$         DEFINE/USER OVMS_AGENT_GITHUB_BRIDGE "''BRIDGE_COMMAND'"
$     ENDIF
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

$! M273 live SSE transport requires stream-oriented C RTL pipes.  Scope
$! the feature to this image activation only; do not change the caller's
$! process/system C RTL feature environment.
$ DEFINE/USER DECC$STREAM_PIPE ENABLE
$ RUN 'IMAGE'
$ EXIT $STATUS
