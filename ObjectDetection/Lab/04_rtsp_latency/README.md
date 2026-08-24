this lab test the whole pipeline latency

when setting rtspsrc latency = 200, latency ~ 260 (ms)
when setting rtspsrc latency = 500, latency ~ 520 (ms)
when setting rtspsrc latency = 50, latency ~ 250 (ms)
=> latency floor = 200(ms)

measure latency floor, try one by one, case A - C

Case A: base (before decoder)
    latency = 20 (ms)

Case B: add decoder back
    latency = 100 - 20 = 80 (ms)

Case C: add videoconvert back (no queue)
    backlog latency! frame stuck, so age become higher and higher