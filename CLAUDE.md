# Snapper

Read `ARCHITECTURE.md` before changing code: it says which module and
manager a change belongs in.

## Build and test

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`ctest` also runs the Snawy's Law linter and the layer check, so a
green run means the code builds, passes its tests, follows the rules
and respects the module order.

## Rules for every change

- Put logic in the manager that owns the concern. A widget never decides
  anything a test should check.
- Change the project only through a manager and `HistoryManager`.
- Reuse the shared pieces: one key type, one sampler, one renderer, one
  edit path. If something looks like a copy of existing code, it is.
- New logic leaves a test behind in `tests/<module>/`.
- Keep `ARCHITECTURE.md` true: a new manager or module goes in it in the
  same commit.

## UX language

- Keys hold (step) unless told to ease.
- Every drag is one undo step; Escape cancels it.
- A control that cannot act is greyed out with a tooltip saying why.
- Undo and redo name what they undo ("Undo Rotate head").
- New things appear where they cause no jump.
- Selected things get an orange outline; draggable handles are yellow.
- Handles look small and grab big.
- Right click changes a node's mode; double click on empty space adds
  one; Delete removes the picked ones; Shift adds to the pick.
