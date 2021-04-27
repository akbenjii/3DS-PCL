#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <algorithm>

#include <3ds.h>
#include "md5.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

int ret, n;
struct sockaddr_in client;
const char delimiter[] = "%";

bool hasSocketCreated = false;

static char buffer[1026];
static char inputBuffer[60];

static u32 *SOC_buffer = NULL;
s32 sock = -1;

char *rndK = NULL;
std::string staticKey = "Y(02.>'H}t\":E1";

static char PenguinID[16];
static char LoginKey[35];

static char username[12];
static char password[48];

static SwkbdState swkbd;
SwkbdButton button = SWKBD_BUTTON_NONE;

bool enableNoBlock = false;

int x = 370;
int y = 335;

int walk_frequency = 15;

void failExit(const char *fmt, ...);

void emit(std::string request);

void createSocket(const char *server_address, int port);

void success(const char *msg) {
    printf("\n\x1b[32m%s\x1b[0m\n\n", msg);
}

void receive() {
    read(sock, buffer, 1024);
    printf("Incoming : \x1b[33m%s\x1b[0m\n", buffer);
}

void send_verChk() {
    printf("Sending verChk...\n\n");
    emit("<msg t='sys'><body action='verChk' r='0'><ver v='153' /></body></msg>");
    receive();

    char *includesApiOK = strstr(buffer, "apiOK");
    char *includesApiKO = strstr(buffer, "apiKO");

    if (includesApiOK)
        success("Received apiOK!");
    else if (includesApiKO)
        failExit("Received apiKO!");
    else
        failExit("Received invalid verChk response!");
}

void send_rndK() {
    printf("Sending rndK...\n\n");
    emit("<msg t=\"sys\"><body action=\"rndK\" r=\" - 1\"></body></msg>");
    receive();

    char *start, *end;
    start = strstr(buffer, "<k>");
    if (start) {
        start += strlen("<k>");
        end = strstr(start, "</k>");
        if (end) {
            rndK = (char *) malloc(end - start + 1);
            memcpy(rndK, start, end - start);
            rndK[end - start] = '\0';
        }
    }

    if (!rndK)
        failExit("Failed to find rndK.");

    printf("\n\x1b[32mReceived rndK response : \x1b[35m%s\x1b[0m\n\n", rndK);
}

std::string swapMD5(std::string password) {
    std::string swapped;

    swapped = password.substr(16, 32);
    swapped += password.substr(0, 16);

    return swapped;
}

const char *crypto(std::string type, std::string password) {
    if (type == "world") {
        password += rndK;

        password = md5(password);
        password = swapMD5(password);

        password += LoginKey;
    } else {
        password = md5(password);
        std::transform(password.begin(), password.end(), password.begin(), ::toupper);

        password = swapMD5(password);
        password += rndK;
        password += staticKey;

        password = md5(password);
        password = swapMD5(password);
    }

    return password.c_str();
}

void socShutdown() {
    printf("waiting for socExit...\n");
    socExit();
}

void getPacketData(const char **bucket) {
    int i = 0;

    char *ptr = strtok(buffer, delimiter);
    while (ptr != NULL) {
        bucket[i] = ptr;
        i++;

        ptr = strtok(NULL, delimiter);
    }

    if (bucket[1] && strcmp(bucket[1], "e") == 0)
        failExit("Received CPPS error code : %s", bucket[3]);
}

void login() {
    createSocket("45.148.120.145", 6112);

    send_verChk();
    send_rndK();

    printf("Attempting login...\n\n");
    char login_request[175];

    snprintf(login_request, 170,
             "<msg t=\"sys\"><body action=\"login\" r=\"0\"><login z=\"w1\"><nick><![CDATA[%s]]></nick><pword><![CDATA[%s]]></pword></login></body></msg>",
             username, crypto("login", password));
    emit(login_request);
    receive();

    close(sock);

    const char *login_packet[16];
    getPacketData(login_packet);

    if (!login_packet[3] || !login_packet[4])
        failExit("Failed to receive Penguin ID or Login Key from packet.");

    strcpy(PenguinID, login_packet[3]);
    strcpy(LoginKey, login_packet[4]);

    printf("\nPenguin ID : %s\n", login_packet[3]);
    printf("Login Key  : %s\n\n", login_packet[4]);
}

inline std::string const &to_string(std::string const &s) { return s; }

template<typename... Args>
void emitPacket(Args const &... args) {
    std::string packet = "%xt%";

    using ::to_string;
    using std::to_string;

    int unpack[]{0, (packet += to_string(args), packet += "%", 0)...};
    static_cast<void>(unpack);

    emit(packet);
}

void join_world() {
    login();

    createSocket("45.148.120.145", 9875);

    send_verChk();
    send_rndK();

    char world_request[258];
    snprintf(world_request, 256,
             "<msg t=\"sys\"><body action=\"login\" r=\"0\"><login z=\"w1\"><nick><![CDATA[%s]]></nick><pword><![CDATA[%s]]></pword></login></body></msg>",
             username, crypto("world", LoginKey));
    emit(world_request);
    receive();

    const char *world_packet[16];
    getPacketData(world_packet);

    emitPacket("s", "j#js", -1, PenguinID, LoginKey, "en");
    receive();

    emitPacket("s", "j#jr", -1, 100, 0, 0);

    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    enableNoBlock = true;
}

void movePenguin() {
    emitPacket("s", "u#sp", "3", x, y);
}

void dance() {
    emitPacket("s", "u#sf", "3", 26);
}

void sendMessage() {
    emitPacket("s", "m#sm", "3", PenguinID, inputBuffer);
}

void joinRoom() {
    x = 370;
    y = 335;
    emitPacket("s", "j#jr", "3", inputBuffer, x, y);
}

void getCredentials() {
    swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, -1);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetFeatures(&swkbd, SWKBD_DARKEN_TOP_SCREEN | SWKBD_ALLOW_HOME | SWKBD_ALLOW_RESET | SWKBD_ALLOW_POWER);
    swkbdSetHintText(&swkbd, "Penguin username");

    bool shouldQuit = false;
    inputBuffer[0] = 0;
    do {
        swkbdSetInitialText(&swkbd, inputBuffer);
        button = swkbdInputText(&swkbd, inputBuffer, sizeof(inputBuffer));
        if (button != SWKBD_BUTTON_NONE)
            break;

        SwkbdResult res = swkbdGetResult(&swkbd);
        if (res == SWKBD_RESETPRESSED) {
            shouldQuit = true;
            aptSetChainloaderToSelf();
            break;
        } else if (res != SWKBD_HOMEPRESSED && res != SWKBD_POWERPRESSED)
            break;
    } while (!shouldQuit);

    memcpy(username, inputBuffer, 12);

    swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, -1);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetFeatures(&swkbd, SWKBD_DARKEN_TOP_SCREEN | SWKBD_ALLOW_HOME | SWKBD_ALLOW_RESET | SWKBD_ALLOW_POWER);
    swkbdSetHintText(&swkbd, "Penguin password");

    inputBuffer[0] = 0;
    do {
        swkbdSetInitialText(&swkbd, inputBuffer);
        button = swkbdInputText(&swkbd, inputBuffer, sizeof(inputBuffer));
        if (button != SWKBD_BUTTON_NONE)
            break;

        SwkbdResult res = swkbdGetResult(&swkbd);
        if (res == SWKBD_RESETPRESSED) {
            shouldQuit = true;
            aptSetChainloaderToSelf();
            break;
        } else if (res != SWKBD_HOMEPRESSED && res != SWKBD_POWERPRESSED)
            break;
    } while (!shouldQuit);

    memcpy(password, inputBuffer, 48);

    if (strlen(username) == 0 || strlen(password) == 0) {
        printf("\x1b[31m%s is too short.\n\n\x1b[0m", strlen(username) == 0 ? "Username" : "Password");
        getCredentials();
    }
}

int main(int argc, char **argv) {
    gfxInitDefault();
    gfxSetWide(true); // Enable wide mode

    atexit(gfxExit);
    atexit(socShutdown);

    consoleInit(GFX_TOP, NULL);

    printf("\x1b[2;45H3DS-PCL");
    printf("\x1b[3;38Hcreated by benji#1337");
    printf("\x1b[4;1H\n");

    getCredentials();
    join_world();
    movePenguin();

    while (aptMainLoop()) {
        bool hasMessage = false;
        bool hasRoomID = false;
        int xHeld = 0;
        hidScanInput();

        if (enableNoBlock) {
            n = recv(sock, buffer, 1024, 0);

            if (n > 0) {
                printf("Incoming : \x1b[33m%s\x1b[0m\n", buffer);
            }
        }

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START)
            break;

        if (kDown & KEY_DLEFT || kDown & KEY_CPAD_LEFT) {
            x -= walk_frequency;
            movePenguin();
        }

        if (kDown & KEY_DRIGHT || kDown & KEY_CPAD_RIGHT) {
            x += walk_frequency;
            movePenguin();
        }

        if (kDown & KEY_DUP || kDown & KEY_CPAD_UP) {
            y -= walk_frequency;
            movePenguin();
        }

        if (kDown & KEY_DDOWN || kDown & KEY_CPAD_DOWN) {
            y += walk_frequency;
            movePenguin();
        }

        if (kDown & KEY_L) {
            dance();
        }

        if (enableNoBlock && kDown & KEY_R) {
            hasMessage = true;
            swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, -1);
            swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
            swkbdSetFeatures(&swkbd,
                             SWKBD_DARKEN_TOP_SCREEN | SWKBD_ALLOW_HOME | SWKBD_ALLOW_RESET | SWKBD_ALLOW_POWER);
            swkbdSetHintText(&swkbd, "Enter message");

            bool shouldQuit = false;
            inputBuffer[0] = 0;
            do {
                swkbdSetInitialText(&swkbd, inputBuffer);
                button = swkbdInputText(&swkbd, inputBuffer, sizeof(inputBuffer));
                if (button != SWKBD_BUTTON_NONE)
                    break;

                SwkbdResult res = swkbdGetResult(&swkbd);
                if (res == SWKBD_RESETPRESSED) {
                    shouldQuit = true;
                    aptSetChainloaderToSelf();
                    break;
                } else if (res != SWKBD_HOMEPRESSED && res != SWKBD_POWERPRESSED)
                    break; // An actual error happened, display it

                shouldQuit = !aptMainLoop();
            } while (!shouldQuit);

            if (shouldQuit)
                break;
        }

        if (enableNoBlock && kDown & KEY_X) {
            hasRoomID = true;
            swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, -1);
            swkbdSetValidation(&swkbd, SWKBD_ANYTHING, 0, 0);
            swkbdSetFeatures(&swkbd,
                             SWKBD_DARKEN_TOP_SCREEN | SWKBD_ALLOW_HOME | SWKBD_ALLOW_RESET | SWKBD_ALLOW_POWER);
            swkbdSetHintText(&swkbd, "Room ID");

            bool shouldQuit = false;
            inputBuffer[0] = 0;
            do {
                swkbdSetInitialText(&swkbd, inputBuffer);
                button = swkbdInputText(&swkbd, inputBuffer, sizeof(inputBuffer));
                if (button != SWKBD_BUTTON_NONE)
                    break;

                SwkbdResult res = swkbdGetResult(&swkbd);
                if (res == SWKBD_RESETPRESSED) {
                    shouldQuit = true;
                    aptSetChainloaderToSelf();
                    break;
                } else if (res != SWKBD_HOMEPRESSED && res != SWKBD_POWERPRESSED)
                    break; // An actual error happened, display it

                shouldQuit = !aptMainLoop();
            } while (!shouldQuit);

            if (shouldQuit)
                break;
        }

        if (hasMessage && strlen(inputBuffer) != 0)
            sendMessage();

        if (hasRoomID && strlen(inputBuffer) != 0)
            joinRoom();

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}

void createSocket(const char *server_address, int port) {
    printf("Creating socket...\t");

    SOC_buffer = (u32 *) memalign(SOC_ALIGN, SOC_BUFFERSIZE);

    if (SOC_buffer == NULL)
        failExit("memalign: failed to allocate\n");

    if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0 && !hasSocketCreated)
        failExit("socInit: 0x%08X\n", (unsigned int) ret);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    if (sock < 0)
        failExit("socket: %d %s\n", errno, strerror(errno));

    memset(&client, 0, sizeof(client));

    printf("\x1b[32mCreated!\n\x1b[0m");
    printf("Connecting to server at %s:%d...\t", server_address, port);

    client.sin_addr.s_addr = inet_addr(server_address);
    client.sin_family = AF_INET;
    client.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *) &client, sizeof(client)) < 0)
        failExit("Failed to connect to socket: %d %s\n", errno, strerror(errno));
    printf("\x1b[32mConnected to server!\n\n\x1b[0m");

    hasSocketCreated = true;
}

void emit(std::string request) {
    std::cout << "Outgoing : \x1b[36m" << request << "\x1b[0m" << std::endl;
    request += "\x00";

    const char *to_send = request.c_str();

    if (send(sock, to_send, strlen(to_send) + 1, 0) == -1)
        failExit("Failed to send request : %s\n\nError: %d %s", to_send, errno, strerror(errno));
}

void failExit(const char *fmt, ...) {
    if (sock > 0) close(sock);
    printf("\n");

    va_list ap;

    printf(CONSOLE_RED);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf(CONSOLE_RESET);
    printf("\nPress B to exit\n");

    while (aptMainLoop()) {
        gspWaitForVBlank();
        hidScanInput();

        u32 kDown = hidKeysDown();
        if (kDown & KEY_B) exit(0);
    }
}