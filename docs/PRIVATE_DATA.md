# Private data workflow

Staff rosters, reconstructed teacher assignments, reference schedules containing
names, and generated schedules containing names must not be committed to public
branches.

Keep private inputs outside this repository, for example:

```text
D:/SchedMesh-private/
  reconstructed-school.project.json
  reference-schedule.json
  outputs/
```

Branches named `local/*` or `private/*` are private. The tracked pre-push hook rejects
those branches and runs `tools/check_public_tree.py` before other pushes.

Enable the hook in this clone with:

```powershell
git config core.hooksPath .githooks
```

Do not use `git push --all` or mirror pushes from a clone containing private branch
history. Deleting an accidentally published branch does not remove its Git objects;
the repository history must be rewritten if that happens.
