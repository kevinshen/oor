﻿Android
--------

Open Overlay Router includes support for Android devices operating as 
LISP mobile nodes (LISP-MN). There are two different editions of the Open Overlay 
Router Android application, for rooted devices and for non-rooted devices. It is 
expected that in the future the root version will provide features beyond those 
available on the non-root version, however on the current version there is just one 
root-only feature, support for IPv6 RLOCs. In both editions, there is a limit of
one IPv4 EID and one IPv6 EID mapped to one or more RLOC interfaces. Even though
 several interfaces can be managed by Open Overlay Router at the same time, they 
can only be used in an active-backup fashion (no more than one interface used at 
once). 
Given that NAT traversal is not yet available for this version, the usage of Open 
Overlay Router on Android is limited to devices with a public address. 

Installation
------------

The two different editions of the Open Overlay Router application have different 
requirements. Open Overlay Router for rooted devices requires root access and Android 
version 2.3.6 or higher, while Open Overlay Router for non-rooted devices requires 
Android 4.0 or higher. Please note that due to a bug on Android 4.4.0 and onwards, 
the non-rooted version of OOR will not work on Android 4.4.0, 4.4.1, 4.4.2 or 
4.4.3. The bug was fixed on Android 4.4.4. (for IPv4 EIDs). You can download a precompiled
APK package file from the Open Overlay Router website or compile the app from sources 
yourself. In any case, if you choose to install Open Overlay Router without using 
Google Play, the device must be configured to allow the installation of packages from 
"unknown sources" (System Settings -> Security -> Device Administration). 

Building the code from source is supported on Linux and Mac OS X. To build 
Open Overlay Router for Android from source code you require some extra packages beside 
those specified in the main README.md file.

  * Android SDK: [http://developer.android.com/sdk/]
  * Android NDK: [http://developer.android.com/tools/sdk/ndk/]
  * Apache Ant

To get the latest version of the OOR source from Github:

    git clone git://github.com/OpenOverlayRouter/oor.git
    cd oor
            
To build the code, go to the `android/` directory located in the top-level 
directory and modify the `local.properties` file with the correct path to your 
Android SDK and Android NDK.  In the Android SDK Manager you should either have
installed Android 4.2.2 (API 17), or update the `project.properties` file to
specify your currently installed API. Please note that regardless of the target 
API, Open Overlay Router for rooted devices should still work on all Android 
releases from API 9 (Android 2.3 Gingerbread) and above, and the non-rooted 
version should work on releases from API 14 (Android 4.0 Ice Cream Sandwich) 
and above.

Compile the code with:

    cd android
    ./select_appl.bash
    ant debug

This command generates an APK file called `openoverlayrouter-debug.apk` in the folder
`android/bin/`. To install it, copy the file to the device and install it using
the android application manager, or run:

    adb install [-r] bin/openoverlayrouter-debug.apk


Running Open Overlay Router
---------------------------

Open Overlay Router Android application allows you to start and stop the oor daemon 
and edit the most important parameters of the configuration file. To access the full
list of configurable parameters you have to edit manually the configuration file
located in /sdcard/oor.conf.  Manually edited parameters not present in the 
graphical configuration interface will be overwritten when using the graphical 
configuration editor. 

Due to the large amount of data generated by the oor daemon, it is recommended 
to set "log level" to 0 when not debugging.
