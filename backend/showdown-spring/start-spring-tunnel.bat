@echo off
title Showdown Live Spring Server
echo ===================================================
echo  Showdown Live Spring Server
echo ===================================================
echo.

set ENABLE_TUNNEL=true
set PORT=3000
cd /d "%~dp0"
set JAVA_HOME=%~dp0..\tools\jdk-17.0.19+10
set MAVEN_HOME=%~dp0..\tools\apache-maven-3.9.16
if not exist "%JAVA_HOME%\bin\java.exe" set JAVA_HOME=C:\Users\Admin\Documents\midnight\work\tools\jdk-17.0.19+10
if not exist "%MAVEN_HOME%\bin\mvn.cmd" set MAVEN_HOME=C:\Users\Admin\Documents\midnight\work\tools\apache-maven-3.9.16
set NODE_HOME=C:\nvm4w\nodejs
set PATH=%JAVA_HOME%\bin;%MAVEN_HOME%\bin;%NODE_HOME%;%PATH%

echo [System] Starting Spring Boot server with Cloudflare Quick Tunnel enabled...
echo [System] Host Dashboard: http://localhost:3000/host.html
echo.

echo [System] Building latest Spring Boot jar...
call mvn.cmd -DskipTests package
if errorlevel 1 (
    echo [Error] Build failed.
    echo [Hint] If the server is already running, press Ctrl+C in that server window and run this file again.
    pause
    exit /b 1
)

java -jar target\showdown-spring-1.0.0.jar
pause
