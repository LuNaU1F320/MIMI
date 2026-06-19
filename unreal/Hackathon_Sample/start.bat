@echo off
title Showdown Live Server Launcher
echo ===================================================
echo  Showdown Live Server Startup Utility
echo ===================================================
echo.

:: Check for node_modules folder
if not exist node_modules (
    echo [System] node_modules folder not found.
    echo [System] Running "npm install" to install required dependencies...
    call npm install
    if errorlevel 1 (
        echo [Error] Failed to install dependencies. Please ensure Node.js is installed.
        pause
        exit /b 1
    )
    echo [System] Installation complete!
    echo.
)

:: Automatically open the Host Dashboard in default browser
echo [System] Launching presenter dashboard in browser...
start http://localhost:3000/host.html

:: Start Node.js server
echo [System] Starting Node.js server...
echo.
node server.js
pause
