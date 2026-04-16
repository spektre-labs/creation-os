# GDA archive (exploratory, not load-bearing)

This directory stores **archived duplicates** and exploratory fragments.

Policy:

- `gda/` is the **canonical** location for any GDA fragments kept in-tree.
- Exact duplicates found elsewhere (for example `gda 2/`) are moved here by
  `tools/archive_gda_duplicates.py`.
- Nothing under `gda/_archive/` is part of the merge gate, CI evidence chain,
  or “required for correctness”.

If you are reading for correctness or review, start with:

- `docs/WHICH_FILE_TO_READ.md`
- `make reviewer`

