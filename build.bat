@ECHO OFF

IF NOT DEFINED DevEnvDir (
	ECHO Run this batch file from a x64 Native Tools Command Prompt for Visual Studio to build.
	ECHO Exiting...
	EXIT
)

SET SOURCES_EXE=main.c

SET INCLUDES=/I . 

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
SET CFLAGS=/Z7 /W4 /sdl /MT /GR- /EHa /WX /nologo /Oi /Gm- /MP

SET LIBS="kernel32.lib" "user32.lib" "gdi32.lib" "d3d12.lib" "dxguid.lib" "dxgi.lib" "d3dcompiler.lib"

REM opt:ref - strip unreferenced functions/data
SET LFLAGS=/opt:ref /DEBUG

REM ////////////////////////////////////////////////
REM Build exe

SET CFLAGS_EXE= 
SET LFLAGS_EXE=

ECHO.
ECHO Building exe...
cl %INCLUDES% %DEFINES% %DEFINES_EXE% %CFLAGS% %CFLAGS_EXE% %SOURCES_EXE% /link %LFLAGS% %LFLAGS_EXE% %LIBS% /OUT:hello.exe
