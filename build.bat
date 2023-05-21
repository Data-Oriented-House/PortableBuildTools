@echo off

odin build src -out:PortableBuildTools.exe -microarch:generic -resource:res/main.rc -subsystem:windows -o:speed
