# SDK demo equivalence coverage

The host tests in this directory are independently written behavioral
equivalents of the SDK demos. They use synthetic inputs and libPorpoise's
public API; SDK demo source and data are not incorporated into the tests.

| Sequence | SDK target | Host test | Status |
|---:|---|---|---|
| 1 | `arcdemo/arctest1` | `arc-archive-paths` | Passing |
| 2 | `arcdemo/arctest2` | `arc-external-storage` | Passing |
| 3 | `arcdemo/arctest3` | `arc-directory-traversal` | Passing |
| 4–22 | `axdemo/*` | — | Deferred: public API is not yet SDK-compatible |
| 23 | `cntdemo/cntdemo` | `sdk-023-cntdemo` | Passing |
| 24 | `cntdemo/datatitledemo` | `sdk-024-datatitledemo` | Passing |
| 25 | `cntdemo/strapcntdemo` | `sdk-025-strapcntdemo` | Passing |
| 26 | `cxdemo/cx_uncompress` | `sdk-026-cx-uncompress` | Passing |
| 27 | `cxdemo/cx_uncompress_stream` | `sdk-027-cx-uncompress-stream` | Passing |
| 28 | `darchdemo/darchdemo` | `sdk-028-darchdemo` | Passing |
| 29 | `dvddemo/directory` | `sdk-029-dvd-directory` | Passing |
| 30 | `dvddemo/dvddemo1` | `sdk-030-dvddemo1` | Passing |

Phase boundary: sequence 30, `dvddemo/dvddemo1`.

Progress: 11 of 30 targets accepted for this phase. The 19 AX targets
remain deferred until their headers and implementation are source-compatible
with the SDK APIs used by the original demos.
