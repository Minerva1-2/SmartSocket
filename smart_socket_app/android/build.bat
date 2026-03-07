@echo off
gradlew.bat assembleDebug > build_output.txt 2>&amp;1
type build_output.txt
