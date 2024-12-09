@echo off
setlocal enabledelayedexpansion
cd /d %~dp0

where /q cl.exe || (
 echo error: cl.exe not found
 exit /b 1
)

if not exist bin mkdir bin

set compiler_options=-std:c11 -utf-8 -Oi -diagnostics:column
set warnings=-WX
set linker_options=-link -incremental:no

:: dev = 0, release = 1
if "%1" == "r" (
 set build_mode=1
) else (
 set build_mode=0
)

if %build_mode% equ 0 (
 :: dev
 set warnings=%warnings%
 set compiler_options=%compiler_options% -Dbuild_dev -Od -Zi
 echo building dev...
) else (
 :: release
 set warnings=%warnings%
 set compiler_options=%compiler_options% -Dbuild_release -O2
 echo building release...
)

rc -nologo -fo"bin\window.res" "source\window.rc"
cl -nologo "source\pbt.c" "bin\window.res" -Fe"bin\pbt.exe" -Fo"bin\pbt.obj" -Fd"bin\pbt.pdb" %compiler_options% %warnings% %linker_options%

if %errorlevel% neq 0 exit /b 1
