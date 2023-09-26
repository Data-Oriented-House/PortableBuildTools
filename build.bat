@echo off

odin build src -out:PortableBuildTools.exe -microarch:generic -o:speed -resource:res/main.rc -subsystem:windows
