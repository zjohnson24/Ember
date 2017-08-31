cd debug
objcopy --only-keep-debug %1.exe %1.exe.debug
strip -g %1.exe
objcopy --add-gnu-debuglink=%1.exe.debug %1.exe