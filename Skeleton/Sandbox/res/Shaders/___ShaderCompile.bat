@echo off

set d=C:\VulkanSDK\1.2.162.1\
if NOT EXIST %d% (
	set d=D:\VulkanSDK\1.2.162.1\
)

for /R %%1 in (*.vert, *.frag) do (
	call :RenameAndCompile %%1
)

pause
GOTO:EOF
	
:RenameAndCompile
	set name=%~n1
	set ext=%~x1
	set ext=%ext:.=%
	set finalName=%name%_%ext%
	echo ==============================
	echo %finalName%
	echo ==============================
	%VULKAN_SDK%\Bin\glslc.exe %~1 -o %finalName%.spv
	::copy %finalName%.spv ..\..\bin\Release\x64\res\%finalName%.spv
	GOTO:EOF
	