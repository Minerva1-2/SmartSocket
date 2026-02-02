@echo off
cd
call gradlew.bat app:dependencies --configuration debugRuntimeClasspath
pause
