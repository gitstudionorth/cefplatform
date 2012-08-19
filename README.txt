Chromium Embedded Framework (CEF) Binary Distribution
-------------------------------------------------------------------------------

CEF Version:      3.1180.719
Chromium Verison: 21.0.1180.18

This distribution contains all components necessary to build and distribute an
application using CEF. Please see the LICENSING section of this document for
licensing terms and conditions.


CONTENTS
--------

cefclient   Contains the cefclient sample application configured to build
            using the files in this distribution.

Debug       Contains libcef.dll and other components required to run the debug
            version of CEF-based applications. Also acts as the build target for
            the Debug build of cefclient.

docs        Contains C++ API documentation generated from the CEF header files.

include     Contains all required CEF header files.

lib         Contains Debug and Release versions of the libcef.lib library file
            that all CEF-based applications must link against.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains libcef.dll and other components required to run the release
            version of CEF-based applications. Also acts as the build target for
            the Release build of cefclient.
