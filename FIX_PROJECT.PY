filename = "SRC/PROJECT.C"

with open(filename, "r", encoding="utf-8", errors="replace") as source:
    lines = source.readlines()

updated = []
replaced = False

for line in lines:
    if "if (*path == '/'" in line:
        updated.append("    if (*path == '/') {\n")
        replaced = True
    else:
        updated.append(line)

if not replaced:
    print("Target line not found")
    raise SystemExit(1)

with open(filename, "w", encoding="utf-8", newline="\n") as destination:
    destination.writelines(updated)

print("Corrected unsafe leading-path test")
