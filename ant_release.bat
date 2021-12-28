rem todo: rename original BleWin10Lib.dll to BleWin10Lib.org.dll
del BleWin10Lib.dll
del ANT_DLL.dll
mklink /H BleWin10Lib.dll c:\Users\build\source\repos\ursoft\ANT_Libraries\x64\Release\ANT_DLL.dll
mklink /H ANT_DLL.dll c:\Users\build\source\repos\ursoft\ANT_Libraries\x64\Release\ANT_DLL.dll
pause