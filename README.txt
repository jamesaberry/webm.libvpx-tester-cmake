vpx libvpx-tester v0.9.1
README - 08-03-2010

Note: For usage see libvpx-tester-manual.pdf
Note: For release notes see libvpx-release-notes.txt

Contents:

A) vpx libvpx-tester Build Procedure Windows 32Bit
B) vpx libvpx-tester Build Procedure Linux 32Bit
C) vpx libvpx-tester Build Procedure IMac 32Bit
D) vpx libvpx-tester Build Procedure Windows 64Bit
E) vpx libvpx-tester Build Procedure Linux 64Bit
F) vpx libvpx-tester Build Procedure IMac 64Bit


  Prerequisites - Inherited prerequisites from libvpx

    * All x86 targets require the Yasm[1] assembler be installed.
    * All Windows builds require that Cygwin[2] be installed.
    * Building the documentation requires PHP[3] and Doxygen[4]. If you do not
      have these packages, you must pass --disable-install-docs to the
      configure script.

    [1]: http://www.tortall.net/projects/yasm
    [2]: http://www.cygwin.com
    [3]: http://php.net
    [4]: http://www.doxygen.org


vpx libvpx-tester Build Procedure Windows 32Bit

Prerequisites:  Inherited prerequisites from libvpx (see libvpx read me for details) and Microsoft Visual Studio 2005

1)	Pull the libvpx from git://review.webmproject.org/libvpx.git that you wish to test
2)	Pull libvpx-tester from git://review.webmproject.org/libvpx-tester.git
3)	Check out an older revision of libvpx that you wish to test the new version against to \libvpx-old\ (optional)

Build and Collect libvpx Libraries - sample configurations are provided below, for more detailed information on vp8 sdk configurations see the README located in the libvpx directory

4)	Build New VP8 Release Libararies using cygwin
�	mkdir codec-sdk-build-VP8-Win32
	cd codec-sdk-build-VP8-Win32
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86-win32-vs8 --enable-static-msvcrt
	make
�	Open .sln file located in codec-sdk-build-VP8-Win32 and build in Release Mode.
�	libvpx.lib will be located in \codec-sdk-build-VP8-Win32\Win32\Release\

5)	Build New VP8 Debug Libraries using cygwin
�	mkdir codec-sdk-build-VP8-Win32-Debug
	cd codec-sdk-build-VP8-Win32-Debug
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86-win32-vs8 --enable-static-msvcrt --enable-mem-tracker
	make
�	Open .sln file located in codec-sdk-build-VP8-Win32-Debug and build in Debug Mode.
�	libvpx.lib will be located in \codec-sdk-build-VP8-Win32-Debug\Win32\Debug\

6)	Build Old VP8 Release library using cygwin (or locate old release library or executable)
�	mkdir codec-sdk-build-VP8-Win32-old
	cd codec-sdk-build-VP8-Win32-old
	chmod -R 777 ../libvpx-old
	../libvpx-old/configure --target=x86-win32-vs8 --enable-static-msvcrt
	make
�	Open .sln file located in codec-sdk-build-VP8-Win32-old and build in Release Mode.
�	libvpx.lib will be located in \codec-sdk-build-VP8-Win32-old\Win32\Release\

Build Main Test Executable from newest libvpx Libraries

7)	Place libvpx.lib  from \codec-sdk-build-VP8-Win32\Win32\Release\ in \libvpx-tester\MasterFile\lib\
8)  Place vpx_config.h from \codec-sdk-build-VP8-Win32\ in \libvpx-tester\MasterFile\include\release-32\
9)  Place vpx_version.h from \codec-sdk-build-VP8-Win32\ in \libvpx-tester\MasterFile\include\release-32\
10)	Open .sln file located in \libvpx-tester\MasterFile\
11)	Build in Release Mode.
12)	Rename executable produced in \libvpx-tester\MasterFile\Release\ from VP8_Tester_API.exe to VP8_Tester_API_32Bit.exe.

Build Supporting Debug Executable from newest libvpx Libraries

13)	Place libvpx.lib  from \codec-sdk-build-VP8-Win32-Debug\Win32\Debug\ in \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\lib
14) Place vpx_config.h from \codec-sdk-build-VP8-Win32-Debug\ in \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\include\debug-32\
15) Place vpx_version.h from \codec-sdk-build-VP8-Win32-Debug\ in \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\include\debug-32\
16)	Open .sln file located in \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\
17)	Build in Debug Mode.
18)	Rename executable produced in \libvpx-tester\VP8ReleasePlugIn\MasterFile\Release\ from VP8v--_ PlugIn_DLib_DMode.exe to VP8vNewest_PlugIn_DLib_DMode.exe.

Build Supporting Release Executable from newest libvpx Libraries

19)	Place libvpx.lib  from \codec-sdk-build-VP8-Win32\Win32\Release\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\lib\
20) Place vpx_config.h from \codec-sdk-build-VP8-Win32\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\include\release-new-32\
21) Place vpx_version.h from \codec-sdk-build-VP8-Win32\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\include\release-new-32\
22)	Open .sln file located in \libvpx-tester\VP8ReleasePlugIn\MasterFile\
23)	Build in Release Mode.
24)	Rename executable produced in \libvpx-tester\VP8ReleasePlugIn\MasterFile\Release\ from VP8v--_PlugIn_RLib_RMode.exe to VP8vNewest_PlugIn_RLib_RMode.exe.

Build Supporting Release Executable from old libvpx Libraries

25)	Place libvpx.lib  from \codec-sdk-build-VP8-Win32-old\Win32\Release\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\lib\
26) Place vpx_config.h from \codec-sdk-build-VP8-Win32-old\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\include\release-old-32\
27) Place vpx_version.h from \codec-sdk-build-VP8-Win32-old\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\include\release-old-32\
28)	Open .sln file located in \libvpx-tester\VP8ReleasePlugIn\MasterFile\
29)	Build in Release Mode.
30)	Rename executable produced in \libvpx-tester\VP8ReleasePlugIn\MasterFile\Release\ from VP8v--_PlugIn_RLib_RMode.exe to VP8vOldest_PlugIn_RLib_RMode.exe.

Gather Executables

31)	Create \libvpx-tester\TestFolder_32Bit\
32)	Place \libvpx-tester\MasterFile\Release\VP8_Tester_API_32Bit.exe in \libvpx-tester\TestFolder_32Bit\
33)	Place \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\Debug\VP8vNewest_PlugIn_DLib_DMode.exe in \libvpx-tester\TestFolder_32Bit\
34)	Place \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\Release\VP8vNewest_PlugIn_RLib_RMode.exe in \libvpx-tester\TestFolder_32Bit\
35)	Place \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\Release\VP8vOldest_PlugIn_RLib_RMode.exe in \libvpx-tester\TestFolder_32Bit\



vpx libvpx-tester Build Procedure Linux 32Bit

Prerequisites:  Inherited prerequisites from libvpx (see libvpx read me for details)

1)	Pull the libvpx from git://review.webmproject.org/libvpx.git that you wish to test
2)	Pull libvpx-tester from git://review.webmproject.org/libvpx-tester.git
3)	Check out an older revision of libvpx that you wish to test the new version against to \libvpx-old\ (optional)

Build and Collect libvpx Libraries - sample configurations are provided below, for more detailed information on vp8 sdk configurations see the README located in the libvpx directory

4)	Build New VP8 Release Libararies
�	mkdir codec-sdk-build-VP8-Lin32
	cd codec-sdk-build-VP8-Lin32
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86-linux-gcc
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-Lin32/

5)	Build New VP8 Debug Libraries
�	mkdir codec-sdk-build-VP8-Lin32-Debug
	cd codec-sdk-build-VP8-Lin32-Debug
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86-linux-gcc --enable-mem-tracker --enable-debug-libs
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-Lin32-Debug/

6)	Build Old VP8 Release library  (or locate old release library or executable)
�	mkdir codec-sdk-build-VP8-Lin32-old
	cd codec-sdk-build-VP8-Lin32-old
	chmod -R 777 ../libvpx-old
	../libvpx-old/configure --target=x86-linux-gcc
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-Lin32-old/

Copy and Rename VP8 Libraries

7)	Copy /codec-sdk-build-VP8-Lin32/libvpx.a to /libvpx-tester/MasterFile/lib/ renaming libvpx.a to libvpx_Lin32.a
8)	Copy /codec-sdk-build-VP8-Lin32-Debug/libvpx.a to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/lib/ renaming libvpx.a to libvpx_MemLin32.a
9)	Copy /codec-sdk-build-VP8-Lin32/libvpx.a to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/newlib/ renaming libvpx.a to libvpx_NewLin32.a
10)	Copy /codec-sdk-build-VP8-Lin32-old/libvpx.a to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/ renaming libvpx.a to oldlib/libvpx_OldLin32.a

11)	Copy /codec-sdk-build-VP8-Lin32/vpx_config.h to /libvpx-tester/MasterFile/include/release-32/
12)	Copy /codec-sdk-build-VP8-Lin32-Debug/vpx_config.h to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/debug-32/
13)	Copy /codec-sdk-build-VP8-Lin32/vpx_config.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-new-32/
14)	Copy /codec-sdk-build-VP8-Lin32-old/vpx_config.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-old-32/

15)	Copy /codec-sdk-build-VP8-Lin32/vpx_version.h to /libvpx-tester/MasterFile/include/release-32/
16)	Copy /codec-sdk-build-VP8-Lin32-Debug/vpx_version.h to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/debug-32/
17)	Copy /codec-sdk-build-VP8-Lin32/vpx_version.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-new-32/
18)	Copy /codec-sdk-build-VP8-Lin32-old/vpx_version.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-old-32/

Build Test Executables

19)	Run make_Linux_API_32Bit located in /libvpx-tester/ Test Executables will be located in /libvpx-tester/TestFolder_32Bit/



vpx libvpx-tester Build Procedure IMac 32Bit

Prerequisites:  Inherited prerequisites from libvpx (see libvpx read me for details)

1)	Pull the libvpx from git://review.webmproject.org/libvpx.git that you wish to test
2)	Pull libvpx-tester from git://review.webmproject.org/libvpx-tester.git
3)	Check out an older revision of libvpx that you wish to test the new version against to \libvpx-old\ (optional)

Build and Collect libvpx Libraries - sample configurations are provided below, for more detailed information on vp8 sdk configurations see the README located in the libvpx directory

4)	Build New VP8 Release Libararies
�	mkdir codec-sdk-build-VP8-IMac32
	cd codec-sdk-build-VP8-IMac32
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86-darwin9-gcc
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-IMac32/

5)	Build New VP8 Debug Libraries
�	mkdir codec-sdk-build-VP8-IMac32-Debug
	cd codec-sdk-build-VP8-IMac32-Debug
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86-darwin9-gcc --enable-mem-tracker --enable-debug-libs
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-IMac32-Debug/

6)	Build Old VP8 Release library  (or locate old release library or executable)
�	mkdir codec-sdk-build-VP8-IMac32-old
	cd codec-sdk-build-VP8-IMac32-old
	chmod -R 777 ../libvpx-old
	../libvpx-old/configure --target=x86-darwin9-gcc
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-IMac32-old/

Copy and Rename VP8 Libraries

7)	Copy /codec-sdk-build-VP8-IMac32/libvpx.a to /libvpx-tester/MasterFile/lib/ renaming libvpx.a to libvpx_IMac32.a
8)	Copy /codec-sdk-build-VP8-IMac32-Debug/libvpx.a to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/lib/ renaming libvpx.a to libvpx_MemIMac32.a
9)	Copy /codec-sdk-build-VP8-IMac32/libvpx.a to /libvpx-tester /SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/newlib/ renaming libvpx.a to libvpx_NewIMac32.a
10)	Copy /codec-sdk-build-VP8-IMac32-old/libvpx.a to /libvpx-tester /SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/oldlib/ renaming libvpx.a to libvpx_OldIMac32.a

11)	Copy /codec-sdk-build-VP8-IMac32/vpx_config.h to /libvpx-tester/MasterFile/include/release-32/
12)	Copy /codec-sdk-build-VP8-IMac32-Debug/vpx_config.h to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/debug-32/
13)	Copy /codec-sdk-build-VP8-IMac32/vpx_config.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-new-32/
14)	Copy /codec-sdk-build-VP8-IMac32-old/vpx_config.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-old-32/

15)	Copy /codec-sdk-build-VP8-IMac32/vpx_version.h to /libvpx-tester/MasterFile/include/release-32/
16)	Copy /codec-sdk-build-VP8-IMac32-Debug/vpx_version.h to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/debug-32/
17)	Copy /codec-sdk-build-VP8-IMac32/vpx_version.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-new-32/
18)	Copy /codec-sdk-build-VP8-IMac32-old/vpx_version.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-old-32/

Build Test Executables

19)	Run make_IMac_API_32Bit located in /libvpx-tester/ Test Executables will be located in /libvpx-tester/TestFolder_32Bit/



vpx libvpx-tester Build Procedure Windows 64Bit

Prerequisites:  Inherited prerequisites from libvpx (see libvpx read me for details) and Microsoft Visual Studio 2005

1)	Pull the libvpx from git://review.webmproject.org/libvpx.git that you wish to test
2)	Pull libvpx-tester from git://review.webmproject.org/libvpx-tester.git
3)	Check out an older revision of libvpx that you wish to test the new version against to \libvpx-old\ (optional)

Build and Collect libvpx Libraries - sample configurations are provided below, for more detailed information on vp8 sdk configurations see the README located in the libvpx directory

4)	Build New VP8 Release Libararies using cygwin
�	mkdir codec-sdk-build-VP8-Win64
	cd codec-sdk-build-VP8-Win64
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86_64-win64-vs8 --enable-static-msvcrt
	make
�	Open .sln file located in codec-sdk-build-VP8-Win64 and build in Release Mode.
�	libvpx.lib will be located in \codec-sdk-build-VP8-Win64\Win64\Release\

5)	Build New VP8 Debug Libraries using cygwin
�	mkdir codec-sdk-build-VP8-Win64-Debug
	cd codec-sdk-build-VP8-Win64-Debug
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86_64-win64-vs8 --enable-static-msvcrt --enable-mem-tracker
	make
�	Open .sln file located in codec-sdk-build-VP8-Win64-Debug and build in Debug Mode.
�	libvpx.lib will be located in \codec-sdk-build-VP8-Win64-Debug\Win64\Debug\

6)	Build Old VP8 Release library using cygwin (or locate old release library or executable)
�	mkdir codec-sdk-build-VP8-Win64-old
	cd codec-sdk-build-VP8-Win64-old
	chmod -R 777 ../libvpx-old
	../libvpx-old/configure --target=x86_64-win64-vs8 --enable-static-msvcrt
	make
�	Open .sln file located in codec-sdk-build-VP8-Win64-old and build in Release Mode.
�	libvpx.lib will be located in \codec-sdk-build-VP8-Win64-old\Win64\Release\

Build Main Test Executable from newest libvpx Libraries

7)	Place libvpx.lib  from \codec-sdk-build-VP8-Win64\Win64\Release\ in \libvpx-tester\MasterFile\lib\
8)  Place vpx_config.h from \codec-sdk-build-VP8-Win64\ in \libvpx-tester\MasterFile\include\release-64\
9)  Place vpx_version.h from \codec-sdk-build-VP8-Win64\ in \libvpx-tester\MasterFile\include\release-64\
10)	Open .sln file located in \libvpx-tester\MasterFile\
11)	Build in Release Mode.
12)	Rename executable produced in \libvpx-tester\MasterFile\Release\ from VP8_Tester_API.exe to VP8_Tester_API_64Bit.exe.

Build Supporting Debug Executable from newest libvpx Libraries

13)	Place libvpx.lib  from \codec-sdk-build-VP8-Win64-Debug\Win64\Debug\ in \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\lib
14) Place vpx_config.h from \codec-sdk-build-VP8-Win64-Debug\ in \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\include\debug-64\
15) Place vpx_version.h from \codec-sdk-build-VP8-Win64-Debug\ in \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\include\debug-64\
16)	Open .sln file located in \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\
17)	Build in Debug Mode.
18)	Rename executable produced in \libvpx-tester\VP8ReleasePlugIn\MasterFile\Release\ from VP8v--_ PlugIn_DLib_DMode.exe to VP8vNewest_PlugIn_DLib_DMode.exe.

Build Supporting Release Executable from newest libvpx Libraries

19)	Place libvpx.lib  from \codec-sdk-build-VP8-Win64\Win64\Release\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\lib\
20) Place vpx_config.h from \codec-sdk-build-VP8-Win64\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\include\release-new-64\
21) Place vpx_version.h from \codec-sdk-build-VP8-Win64\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\include\release-new-64\
22)	Open .sln file located in \libvpx-tester\VP8ReleasePlugIn\MasterFile\
23)	Build in Release Mode.
24)	Rename executable produced in \libvpx-tester\VP8ReleasePlugIn\MasterFile\Release\ from VP8v--_PlugIn_RLib_RMode.exe to VP8vNewest_PlugIn_RLib_RMode.exe.

Build Supporting Release Executable from old libvpx Libraries

25)	Place libvpx.lib  from \codec-sdk-build-VP8-Win64-old\Win64\Release\ in \libvpx-tester \SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\lib\
26) Place vpx_config.h from \codec-sdk-build-VP8-Win64-old\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\include\release-old-64\
27) Place vpx_version.h from \codec-sdk-build-VP8-Win64-old\ in \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\include\release-old-64\
28)	Open .sln file located in \libvpx-tester\VP8ReleasePlugIn\MasterFile\
29)	Build in Release Mode.
30)	Rename executable produced in \libvpx-tester\VP8ReleasePlugIn\MasterFile\Release\ from VP8v--_PlugIn_RLib_RMode.exe to VP8vOldest_PlugIn_RLib_RMode.exe.

Gather Executables

31)	Create \libvpx-tester\TestFolder_64Bit\
32)	Place \libvpx-tester \MasterFile\Release\VP8_Tester_API_64Bit.exe in \libvpx-tester\TestFolder_64Bit\
33)	Place \libvpx-tester\SupportingPlugInFiles\VP8DebugPlugIn\MasterFile\Debug\VP8vNewest_PlugIn_DLib_DMode.exe in \libvpx-tester\TestFolder_64Bit\
34)	Place \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\Release\VP8vNewest_PlugIn_RLib_RMode.exe in \libvpx-tester\TestFolder_64Bit\
35)	Place \libvpx-tester\SupportingPlugInFiles\VP8ReleasePlugIn\MasterFile\Release\VP8vOldest_PlugIn_RLib_RMode.exe in \libvpx-tester\TestFolder_64Bit\



vpx libvpx-tester Build Procedure Linux 64Bit

Prerequisites:  Inherited prerequisites from libvpx (see libvpx read me for details)

1)	Pull the libvpx from git://review.webmproject.org/libvpx.git that you wish to test
2)	Pull libvpx-tester from git://review.webmproject.org/libvpx-tester.git
3)	Check out an older revision of libvpx that you wish to test the new version against to \libvpx-old\ (optional)

Build and Collect libvpx Libraries - sample configurations are provided below, for more detailed information on vp8 sdk configurations see the README located in the libvpx directory

4)	Build New VP8 Release Libararies
�	mkdir codec-sdk-build-VP8-Lin64
	cd codec-sdk-build-VP8-Lin64
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86_64-linux-gcc
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-Lin64/

5)	Build New VP8 Debug Libraries
�	mkdir codec-sdk-build-VP8-Lin64-Debug
	cd codec-sdk-build-VP8-Lin64-Debug
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86_64-linux-gcc --enable-mem-tracker --enable-debug-libs
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-Lin64-Debug/

6)	Build Old VP8 Release library  (or locate old release library or executable)
�	mkdir codec-sdk-build-VP8-Lin64-old
	cd codec-sdk-build-VP8-Lin64-old
	chmod -R 777 ../libvpx-old
	../libvpx-old/configure --target=x86_64-linux-gcc
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-Lin64-old/

Copy and Rename VP8 Libraries

7)	Copy /codec-sdk-build-VP8-Lin64/libvpx.a to /libvpx-tester/MasterFile/lib/ renaming libvpx.a to libvpx_Lin64.a
8)	Copy /codec-sdk-build-VP8-Lin64-Debug/libvpx.a to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/lib/ renaming libvpx.a to libvpx_MemLin64.a
9)	Copy /codec-sdk-build-VP8-Lin64/libvpx.a to /libvpx-tester /SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/newlib/ renaming libvpx.a to libvpx_NewLin64.a
10)	Copy /codec-sdk-build-VP8-Lin64-old/libvpx.a to /libvpx-tester /SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/oldlib/ renaming libvpx.a to libvpx_OldLin64.a

11)	Copy /codec-sdk-build-VP8-Lin64/vpx_config.h to /libvpx-tester/MasterFile/include/release-64/
12)	Copy /codec-sdk-build-VP8-Lin64-Debug/vpx_config.h to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/debug-64/
13)	Copy /codec-sdk-build-VP8-Lin64/vpx_config.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-new-64/
14)	Copy /codec-sdk-build-VP8-Lin64-old/vpx_config.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-old-64/

15)	Copy /codec-sdk-build-VP8-Lin64/vpx_version.h to /libvpx-tester/MasterFile/include/release-64/
16)	Copy /codec-sdk-build-VP8-Lin64-Debug/vpx_version.h to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/debug-64/
17)	Copy /codec-sdk-build-VP8-Lin64/vpx_version.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-new-64/
18)	Copy /codec-sdk-build-VP8-Lin64-old/vpx_version.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-old-64/

Build Test Executables

19)	Run make_Linux_API_64Bit located in /libvpx-tester/ Test Executables will be located in /libvpx-tester/TestFolder_64Bit/



vpx libvpx-tester Build Procedure IMac 64Bit

Prerequisites:  Inherited prerequisites from libvpx (see libvpx read me for details)

1)	Pull the libvpx from git://review.webmproject.org/libvpx.git that you wish to test
2)	Pull libvpx-tester from git://review.webmproject.org/libvpx-tester.git
3)	Check out an older revision of libvpx that you wish to test the new version against to \libvpx-old\ (optional)

Build and Collect libvpx Libraries - sample configurations are provided below, for more detailed information on vp8 sdk configurations see the README located in the libvpx directory

4)	Build New VP8 Release Libararies
�	mkdir codec-sdk-build-VP8-IMac64
	cd codec-sdk-build-VP8-IMac64
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86_64-darwin9-gcc
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-IMac64/

5)	Build New VP8 Debug Libraries
�	mkdir codec-sdk-build-VP8-IMac64-Debug
	cd codec-sdk-build-VP8-IMac64-Debug
	chmod -R 777 ../libvpx
	../libvpx/configure --target=x86_64-darwin9-gcc --enable-mem-tracker --enable-debug-libs
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-IMac64-Debug/

6)	Build Old VP8 Release library  (or locate old release library or executable)
�	mkdir codec-sdk-build-VP8-IMac64-old
	cd codec-sdk-build-VP8-IMac64-old
	chmod -R 777 ../libvpx-old
	../libvpx-old/configure --target=x86_64-darwin9-gcc
	make
�	make install
�	libvpx.a will be located in /codec-sdk-build-VP8-IMac64-old/

Copy and Rename VP8 Libraries

7)	Copy /codec-sdk-build-VP8-IMac64/libvpx.a to /libvpx-tester/MasterFile/lib/ renaming libvpx.a to libvpx_IMac64.a
8)	Copy /codec-sdk-build-VP8-IMac64-Debug/libvpx.a to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/lib/ renaming libvpx.a to libvpx_MemIMac64.a
9)	Copy /codec-sdk-build-VP8-IMac64/libvpx.a to /libvpx-tester /SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/newlib/ renaming libvpx.a to libvpx_NewIMac64.a
10)	Copy /codec-sdk-build-VP8-IMac64-old/libvpx.a to /libvpx-tester /SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/oldlib/ renaming libvpx.a to libvpx_OldIMac64.a

11)	Copy /codec-sdk-build-VP8-IMac64/vpx_config.h to /libvpx-tester/MasterFile/include/release-64/
12)	Copy /codec-sdk-build-VP8-IMac64-Debug/vpx_config.h to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/debug-64/
13)	Copy /codec-sdk-build-VP8-IMac64/vpx_config.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-new-64/
14)	Copy /codec-sdk-build-VP8-IMac64-old/vpx_config.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-old-64/

15)	Copy /codec-sdk-build-VP8-IMac64/vpx_version.h to /libvpx-tester/MasterFile/include/release-64/
16)	Copy /codec-sdk-build-VP8-IMac64-Debug/vpx_version.h to to /libvpx-tester/SupportingPlugInFiles/VP8DebugPlugIn/MasterFile/debug-64/
17)	Copy /codec-sdk-build-VP8-IMac64/vpx_version.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-new-64/
18)	Copy /codec-sdk-build-VP8-IMac64-old/vpx_version.h to /libvpx-tester/SupportingPlugInFiles/VP8ReleasePlugIn/MasterFile/release-old-64/

Build Test Executables

19)	Run make_IMac_API_64Bit located in /libvpx-tester/ Test Executables will be located in /libvpx-tester/TestFolder_64Bit/
