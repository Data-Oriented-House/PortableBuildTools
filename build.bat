@echo off
setlocal enabledelayedexpansion

if not exist bin mkdir bin

where /q cl.exe || (
	echo ERROR: cl.exe not found
	exit /b 1
)

:: Dev = 0, Release = 1
if "%1" == "r" (
	set build_mode=1
) else (
	set build_mode=0
)

set out_name=pbt.exe
set compiler_options=-std:c11 -utf-8 -Oi
set warnings=-WX
set linker_options=-link -incremental:no

if %build_mode% equ 0 (
	:: Dev
	set warnings=%warnings%
	set compiler_options=%compiler_options% -Dbuild_dev -Od -Zi
) else (
	:: Release
	set warnings=%warnings%
	set compiler_options=%compiler_options% -Dbuild_release -O2
	echo Building release...
)

rc -nologo -fo"bin\gui.res" "gui\gui.rc"
cl -nologo source.c "bin\gui.res" -Fe"bin\pbt.exe" -Fo"bin\pbt.obj" -Fd"bin\pbt.pdb" %compiler_options% %warnings% %linker_options%
if %errorlevel% neq 0 exit /b 1
