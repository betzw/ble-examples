URI-Beacons are handy when there is a need to advertise a small amount of
information (usually a URL) to any nearby device. They’re really easy to set
up: the code is fully available on the mbed website, so all you’ll need to do
is tell the beacon what to broadcast.

Technical details are better presented [here](https://developer.mbed.org/teams/Bluetooth-Low-Energy/code/BLE_URIBeacon/),
which happens to be the mbed-classic equivalent of this example. Please also refer to [Google's URIBeacon project](https://github.com/google/uribeacon).

What You’ll Need
================

To get this going, you’ll need:

- To see BLE devices and their advertisement or beacon information, get one of the following installed on your phone:

  - The `physical web` app. You can get that app for [iOS](https://itunes.apple.com/us/app/physical-web/id927653608?mt=8) and for [Android](https://play.google.com/store/apps/details?id=physical_web.org.physicalweb).

  - For Android, you can get [nRF Master Control Panel](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp).

  - For iPhone, you can get [LightBlue](https://itunes.apple.com/gb/app/lightblue-bluetooth-low-energy/id557428110?mt=8).

- One of the BLE platforms listed in the README.md of this repository, for example a
  Nordic DK board.

Build Instructions
==================

After cloning the parent repository, switch to the subfolder BLE_URIBeacon, and
execute the following:

```Shell
yotta target <an_appropriate_target_such_as_mkit-gcc>
yotta install
yotta build
```
Assuming that you're building for the nRF51 DK platform, available targets are
`nrf51dk-armcc` and `nrf51dk-gcc`. You can pick either.

The other targets you can use are described in the main README.md for this repository.

The resulting binaries would be under `build/<yotta_target_name>/source/`.

Under that folder, the file called `ble-uribeacon-combined.hex` is the one which
can be flashed to the target using mbed's DAP over USB; the parent README or the
documentation for your yotta target will explain how to choose between the available
binaries and hex files.

Checking for Success
====================

Your URI beacon should be detectable by BLE scanners (e.g. a smartphone) and by the
Google Physical Web app.

**Please note that the URIBeacon spec requires the URIBeacon app to remain in
config mode for the first 60 seconds before switching to being a beacon. So if
you're using a physical-web app, you'll only see the beacon after this period;
if you're using one of the generic apps for BLE scanning, you should see a
configurable beacon being advertised for the first 60 seconds.**

You'll find [links](https://github.com/google/uribeacon/tree/uribeacon-final#contents) on Google's project page to client apps to test URIBeacon. Here's a link that should get you an [Android App](https://github.com/google/uribeacon/releases/tag/v1.2); please browse to `uribeacon-sample-release.apk`. But you should begin with the links to android apps mentioned above.

