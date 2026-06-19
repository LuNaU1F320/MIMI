@echo off
title MIMI Spring Server Local
echo ===================================================
echo  MIMI Spring Server Local Only
echo ===================================================
echo.

set ENABLE_TUNNEL=false
set PORT=3000
cd /d "%~dp0"
set JAVA_HOME=%~dp0..\tools\jdk-17.0.19+10
set MAVEN_HOME=%~dp0..\tools\apache-maven-3.9.16
if not exist "%JAVA_HOME%\bin\java.exe" set JAVA_HOME=C:\Users\Admin\Documents\midnight\work\tools\jdk-17.0.19+10
if not exist "%MAVEN_HOME%\bin\mvn.cmd" set MAVEN_HOME=C:\Users\Admin\Documents\midnight\work\tools\apache-maven-3.9.16
set PATH=%JAVA_HOME%\bin;%MAVEN_HOME%\bin;%PATH%

echo [System] Starting Spring Boot server without public tunnel...
echo [System] Host Dashboard: http://localhost:3000/host.html
echo.

if not exist target\showdown-spring-1.0.0.jar (
    echo [System] Built jar not found. Building project first...
    call mvn.cmd -DskipTests package
    if errorlevel 1 (
        echo [Error] Build failed.
        pause
        exit /b 1
    )
)

java -jar target\showdown-spring-1.0.0.jar
pause
