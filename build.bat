@ECHO OFF

REM Don't want to keep adding stuff to PATH, otherwise the batch file will start failing after too many times
IF NOT DEFINED DevEnvDir (
	CALL "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
)

FOR /F "tokens=* USEBACKQ" %%F IN (`uuidgen`) DO (
	SET GUID=%%F
)

SET SOURCES_EXE=main.c

SET INCLUDES=/I . 
REM SET INCLUDES=%INCLUDES% /I../lib
REM SET INCLUDES=%INCLUDES% /I"C:\Program Files (x86)\Windows Kits\8.1\Include\um"
REM SET INCLUDES=%INCLUDES% /I"C:\Program Files (x86)\Windows Kits\8.1\Include\shared"
REM SET INCLUDES=%INCLUDES% /I"C:\Program Files (x86)\Windows Kits\8.1\Include\winrt"
REM SET INCLUDES=%INCLUDES% /I"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\include"
REM SET INCLUDES=%INCLUDES% /I"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\atlmfc\include"
REM SET INCLUDES=%INCLUDES% /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.10240.0\ucrt"

SET DEFINES=/D"BUILD_DEBUG=1" /D"BUILD_ARCH64=1" /D"BUILD_WIN=1"

REM Zi - use old embedded debug format
REM W4 - max warning level (4)
REM sdl - additional security features/warnings
REM MT - link with multithreaded lib (msvcrt.lib)
REM GR- - disable RTTI??
REM EHa - enable structured exception handling
REM WX - warnings as errors
REM nologo - no copyright message
REM Oi - enable intrinsics
REM Yc - specify precompiled header
REM Gm - disable incremental build
REM /MP - multithreaded compilation
REM SET CFLAGS=/Z7 /W4 /sdl /MT /GR- /EHa /WX /nologo /Oi /Yc"precompiled.h" /Gm-
SET CFLAGS=/Z7 /W4 /sdl /MT /GR- /EHa /WX /nologo /Oi /Gm- /MP

SET LIBS="kernel32.lib" "user32.lib" "gdi32.lib" "d3d12.lib" "dxguid.lib" "dxgi.lib"

REM opt:ref - strip unreferenced functions/data
REM /SUBSYSTEM:windows,5.1 - run on Windows XP
SET LFLAGS=/opt:ref /DEBUG /SUBSYSTEM:windows,5.1
REM SET LFLAGS=%LFLAGS% /LIBPATH:../lib/
REM SET LFLAGS=%LFLAGS% /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\lib"
REM SET LFLAGS=%LFLAGS% /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\atlmfc\lib"
REM SET LFLAGS=%LFLAGS% /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\lib\10.0.10240.0\ucrt\x86"
REM SET LFLAGS=%LFLAGS% /LIBPATH:"C:\Program Files (x86)\Windows Kits\8.1\lib\winv6.3\um\x86"
REM SET LFLAGS=%LFLAGS% /LIBPATH:"C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6.1\Lib\um\x86"

REM ////////////////////////////////////////////////
REM Build exe

SET CFLAGS_EXE= 
SET LFLAGS_EXE=

ECHO.
ECHO Building exe...
cl %INCLUDES% %DEFINES% %DEFINES_EXE% %CFLAGS% %CFLAGS_EXE% %SOURCES_EXE% /link %LFLAGS% %LFLAGS_EXE% %LIBS% /OUT:demo.exe

