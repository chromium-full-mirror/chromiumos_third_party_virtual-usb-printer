# Scanning Special Cases

This guide details each of the special cases that the virtual USB printer's eSCL
manager supports. The default behavior of the eSCL manager is to generate a
single image for every scan job and respond immediately. Specific combinations
of ScanSettings in received scan jobs can trigger special cases, which are
documented here.

[TOC]

## Multi-page ADF Scans

This behavior is triggered by the combination of:
-   `input_source`: `Feeder`
-   `color_mode`: `kGrayscale`
-   `DPI`: `300`

For scan jobs matching the above combination, two images will be generated for
the scan job instead of one.

## Delayed Response

This behavior is triggered by the combination of:
-   `input_source`: `Platen`
-   `color_mode`: `kRGB`
-   `DPI`: `100`

For scan jobs matching the above combination, retrieving the image for the scan
job will be delayed by 45 seconds.
