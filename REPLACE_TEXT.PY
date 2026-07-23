import os
import sys

if len(sys.argv) != 4:
    print("Usage: python replace_text.py file old-text new-text")
    raise SystemExit(2)

filename = sys.argv[1]
old_text = sys.argv[2]
new_text = sys.argv[3]

try:
    with open(filename, "r", encoding="utf-8", errors="replace") as source:
        contents = source.read()
except OSError as error:
    print(f"Unable to read {filename}: {error}")
    raise SystemExit(1)

matches = contents.count(old_text)

if matches == 0:
    print(f"Text not found in {filename}")
    raise SystemExit(1)

if matches > 1:
    print(f"Refusing replacement: text occurs {matches} times")
    raise SystemExit(1)

updated = contents.replace(old_text, new_text, 1)

try:
    with open(filename, "w", encoding="utf-8", newline="\n") as destination:
        destination.write(updated)
except OSError as error:
    print(f"Unable to write {filename}: {error}")
    raise SystemExit(1)

print(f"Updated {filename}")
