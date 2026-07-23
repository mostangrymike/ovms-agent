$ OPEN/WRITE OUT [.SRC]MAIN.C
$ WRITE OUT "/* OVMS Agent source */"
$ WRITE OUT "#include <stdio.h>"
$ WRITE OUT "#include <stdlib.h>"
$ WRITE OUT ""
$ WRITE OUT "int main(void)"
$ WRITE OUT "{"
$ WRITE OUT "    (void)puts(""OVMS Agent"");"
$ WRITE OUT "    return EXIT_SUCCESS;"
$ WRITE OUT "}"
$ CLOSE OUT
$ WRITE SYS$OUTPUT "Created [.SRC]MAIN.C"
