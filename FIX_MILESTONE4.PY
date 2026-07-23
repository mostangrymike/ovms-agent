filename = "APPLY_MILESTONE4.PY"

with open(filename, "r", encoding="utf-8", errors="replace") as source:
    lines = source.readlines()

output = []
inside_tail = False

for line in lines:
    stripped = line.lstrip()

    if stripped.startswith("for filename, content in files.items():"):
        inside_tail = True
        output.append("for filename, content in files.items():\n")
        continue

    if inside_tail:
        if stripped.startswith("path = Path(filename)"):
            output.append("    path = Path(filename)\n")
        elif stripped.startswith("path.parent.mkdir("):
            output.append("    path.parent.mkdir(parents=True, exist_ok=True)\n")
        elif stripped.startswith("with path.open("):
            output.append(
                '    with path.open("w", encoding="utf-8", '
                'newline="\\n") as output_file:\n'
            )
        elif stripped.startswith("output.write("):
            output.append("        output_file.write(content.lstrip())\n")
        elif stripped.startswith('print(f"Created {filename}")'):
            output.append('    print(f"Created {filename}")\n')
        elif stripped.startswith(
            'print("Milestone 4 source files created.")'
        ):
            output.append('\nprint("Milestone 4 source files created.")\n')
            inside_tail = False
        else:
            output.append(line)
    else:
        output.append(line)

with open(filename, "w", encoding="utf-8", newline="\n") as destination:
    destination.writelines(output)

print("Corrected Milestone 4 indentation.")
