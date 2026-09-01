# OpenVMS RMS knowledge

Use these as validated operating lessons, not as proof of any current file's contents or attributes. Inspect the live RMS file when correctness depends on its record structure.

- OpenVMS text files can be sequential variable-record files with carriage-control attributes rather than Unix byte streams. Preserve record boundaries and attributes when editing command procedures and other RMS-sensitive text.
- Text that looks correct after a byte-oriented rewrite can still be unusable if RMS record boundaries were damaged. Native execution or RMS-visible inspection is authoritative for command-procedure integrity.
- A new OpenVMS file version does not erase older physical versions. Repository deletion or replacement is not proof that all stale versions are gone; inspect or purge versions deliberately when acceptance depends on removal.
- For content identity when direct Git working-file reads fail, `PIPE TYPE file | GIT "hash-object" "--stdin"` provides a stream representation that can be compared with the committed blob SHA.
- Direct Git `hash-object` or status can report `short read while indexing` for valid RMS variable-record files. Do not infer corruption from that diagnostic alone when RMS inspection and stream hashing prove the content healthy.
- RMS-aware write/restore paths should preserve expected record structure. Do not substitute arbitrary byte-stream rewrites for established project helpers when the file's RMS format matters.
- A failed compile can leave an older `.OBJ` version available to LINK. Remove or independently verify stale objects when native acceptance requires a freshly compiled image.
- Treat physical file content, RMS attributes, Git object identity, and Git metadata as separate evidence surfaces.
