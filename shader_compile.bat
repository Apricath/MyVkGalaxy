@echo off
Setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "INPUT_DIR=%SCRIPT_DIR%assets\shaders"
set "OUTPUT_DIR=%SCRIPT_DIR%assets\spirv"

for /r "%INPUT_DIR%" %%i in (*) do (
	set input=%%i
	set output=!input:%INPUT_DIR%=%OUTPUT_DIR%!.spv

	set pathOfInput=%%~dpi
	set pathToCreate=!pathOfInput:%INPUT_DIR%=%OUTPUT_DIR%!
	mkdir !pathToCreate! 2>NUL

	echo Compiling shader %%i
	glslc !input! -o !output!
)
