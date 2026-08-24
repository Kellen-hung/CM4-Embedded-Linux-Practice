this lab test the whole pipeline FPS

Supposed that Camera FPS = 15
If we don't put any queue the whole pipeline FPS = 8, so we need find the bottleneck of whole pipeline

try one by one, case A - E

Case A: base (before decoder)
    FPS = 15

Case B: add decoder back
    FPS = 15

Case C: add videoconvert back
    FPS = 8 => so videoconvert is bottleneck

Case D: add queue between decoder and videoconvert
    FPS = 15 => solve => because queue will create another thread to run downstream pipeline (decouple)
    CPU usage: before decoder = 90%, after videoconvert = 95%

Case E: test if put queue to other places
    FPS = 8

