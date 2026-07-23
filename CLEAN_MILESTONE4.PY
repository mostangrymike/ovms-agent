filename = "APPLY_MILESTONE4.PY"

with open(filename, "r", encoding="utf-8", errors="replace") as source:
lines = source.readlines()

cleaned = []
removed = 0

for line in lines:
if line.strip() in {"`", "`python"}:
removed += 1
continue

```
cleaned.append(line)
```

with open(filename, "w", encoding="utf-8", newline="\n") as destination:
destination.writelines(cleaned)

print(f"Removed {removed} Markdown fence line(s).")
