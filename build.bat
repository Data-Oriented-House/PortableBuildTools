@echo off
setlocal enabledelayedexpansion
cd /d %~dp0

where /q cl.exe || (
 echo error: cl.exe not found
 exit /b 1
)

if not exist bin mkdir bin

set build_mode=dev
set cl_options=-nologo -std:c11 -utf-8 -Oi -diagnostics:column -FC -WX -Z7
set rc_options=-nologo
set linker=link
set linker_options=-nologo -incremental:no -manifest:embed -debug

:loop
if "%1" == "r" (
 set build_mode=release
)
shift
if not "%~1" == "" goto loop

if "%build_mode%" == "release" (
 set cl_options=%cl_options% -O1
 set linker_options=%linker_options% -debug:none
)

echo building %build_mode%

for %%f in (source\*.c) do (
 cl %cl_options% -Fo"bin\%%~nf.obj" -Fd"bin\%%~nf.pdb" -c source\%%~nf.c || exit /b 1
 if exist source\%%~nf.rc (
  rc %rc_options% -fo"bin\%%~nf.res" source\%%~nf.rc || exit /b 1
  %linker% %linker_options% -out:bin\%%~nf.exe bin\%%~nf.obj bin\%%~nf.res || exit /b 1
 ) else (
  %linker% %linker_options% -out:bin\%%~nf.exe bin\%%~nf.obj || exit /b 1
 )
)
