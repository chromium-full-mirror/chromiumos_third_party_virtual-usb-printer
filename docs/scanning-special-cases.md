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

or:
-   `input_source`: `Feeder`
-   `color_mode`: `kRGB`
-   `DPI`: `100`


For scan jobs matching the above combinations, two images will be generated for
the scan job instead of one.

## Delayed Response

This behavior is triggered by the combination of:
-   `input_source`: `Platen`
-   `color_mode`: `kRGB`
-   `DPI`: `100`

For scan jobs matching the above combination, retrieving the image for the scan
job will be delayed by 45 seconds.

## ADF Out of Documents

This behavior is triggered by the combination of:
-   `input_source`: `Feeder`
-   `color_mode`: `kGrayscale`
-   `DPI`: `600`

For scan jobs matching the above combination, posting the scan job will fail
with a 500 Internal Server Error, and the next GetScannerStatus request will
return an AdfState of ScannerAdfEmpty.
