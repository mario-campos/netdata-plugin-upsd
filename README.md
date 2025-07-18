# netdata-plugin-upsd

[![C/C++ CI](https://github.com/mario-campos/netdata-plugin-upsd/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/mario-campos/netdata-plugin-upsd/actions/workflows/c-cpp.yml)

### About

upsd.plugin is a Netdata collector plugin for Network UPS Tools (NUT).

In particular, upsd.plugin is a lightweight alternative to Netdata's [upsd Go module](https://learn.netdata.cloud/docs/collecting-metrics/ups/ups-nut), whereas upsd.plugin is written in C to execute faster (less clock cyles) and use less memory. In fact, one notable aspect is that upsd.plugin&mdash;not including libupsclient&mdash;does not allocate any memory once initialized!

### Build

```shell
cmake -B build
cmake --build build --target upsd.plugin
```

### Dependencies

upsd.plugin needs libupsclient in order to look up the NUT UPSes. This package should be installed if NUT is installed.
