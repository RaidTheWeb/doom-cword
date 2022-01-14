//doomgeneric for cword os

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <termios.h>

#include <cword/fb.h>

static int framebuffer_fd = -1;
static uint32_t* framebuffer;
static FILE* file;

static int ttyfd = -1;

#define KEYQUEUE_SIZE 16

static uint16_t keyqueue[KEYQUEUE_SIZE];
static uint32_t keyqueuewriteindex = 0;
static uint32_t keyqueuereadindex = 0;

static uint32_t posX = 0;
static uint32_t posY = 0;

static uint32_t screenWidth = 0;
static uint32_t screenHeight = 0;
static uint32_t bpp = 0;
static uint32_t pitch = 0;

static uint8_t convertToDoomKey(uint8_t scancode) {
    uint8_t key = 0;
    switch(scancode) {
        case 0x9C:
        case 0x1C:
            key = KEY_ENTER;
            break;
        case 0x01:
            key = KEY_ESCAPE;
            break;
        case 0xCB:
        case 0x4B:
            key = KEY_LEFTARROW;
            break;
        case 0xCD:
        case 0x4D:
            key = KEY_RIGHTARROW;
            break;
        case 0xC8:
        case 0x48:
            key = KEY_UPARROW;
            break;
        case 0xD0:
        case 0x50:
            key = KEY_DOWNARROW;
            break;
        case 0x1D:
            key = KEY_FIRE;
            break;
        case 0x39:
            key = KEY_USE;
            break;
        case 0x2A:
        case 0x36:
            key = KEY_RSHIFT;
            break;
        case 0x15:
            key = 'y';
            break;
        default:
            break;
    }

    return key;
}

static void addKeyToQueue(int pressed, uint8_t keyCode) {
    uint8_t key = convertToDoomKey(keyCode);
    uint16_t keyData = (pressed << 8) | key;

    keyqueue[keyqueuewriteindex] = keyData;
    keyqueuewriteindex++;
    keyqueuewriteindex %= KEYQUEUE_SIZE;
}

struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO);
    raw.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void DG_Init() {
    framebuffer_fd = open("/dev/fb0", O_RDWR);

    if(framebuffer_fd >= 0) {
        struct fb_info info;
        printf("Getting info...\n");
        ioctl(framebuffer_fd, FB_GETINFO, &info);
        screenWidth = info.xres;
        screenHeight = info.yres;
        pitch = info.line_length;
        bpp = info.bpp;

        printf("Screen res = %dx%d\n", screenWidth, screenHeight);

        //framebuffer = mmap(NULL, screenWidth * screenHeight * info.bpp / 8, PROT_READ | PROT_WRITE, 0, framebuffer_fd, 0);
        framebuffer = (uint32_t*)malloc((screenWidth * screenHeight * info.bpp / 8) * sizeof(uint8_t));

        if(framebuffer != (uint32_t*)-1) {
            printf("Mapped framebuffer successfully!\n");
        } else {
            printf("Failed to map framebuffer!\n");
            exit(1);
        }
    } else {
        printf("Failed to open framebuffer!\n");
        exit(1);
    }

    close(framebuffer_fd);

    file = fopen("/dev/fb0", "w+b");

    enableRawMode();

    if(STDIN_FILENO >= 0) {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    int argPosX = 0;
    int argPosY = 0;

    argPosX = M_CheckParmWithArgs("-posx", 1);
    if(argPosX > 0) {
        sscanf(myargv[argPosX + 1], "%d", &posX);
    }

    argPosY = M_CheckParmWithArgs("-posy", 1);
    if(argPosY > 0) {
        sscanf(myargv[argPosY + 1], "%d", &posY);
    }
}

static void handleKeyInput() {
    if(STDIN_FILENO < 0) return;

    uint8_t scancode = 0;

    if(read(STDIN_FILENO, &scancode, 1) > 0) {
        uint8_t pressed = (scancode & 0x80) == 0x80;

        scancode = scancode & 0x7F;

        printf("Handling input!\n");

        if(pressed) addKeyToQueue(1, scancode);
        else addKeyToQueue(0, scancode);
    }
}

void DG_DrawFrame() {
    /*fclose(file);
    file = fopen("/dev/fb0", "r+b");
    fseek(file, 0, SEEK_SET);
    fread(framebuffer, pitch * screenHeight * sizeof(uint8_t), 1, file);
    fseek(file, 0, SEEK_SET);
    fclose(file);
    file = fopen("/dev/fb0", "w+b");*/
    for(int i = 0; i < DOOMGENERIC_RESY; ++i) {
        // TODO: Work with proper screensize information
        memcpy(framebuffer + posX + (i + posY) * screenWidth, DG_ScreenBuffer + i * DOOMGENERIC_RESX, DOOMGENERIC_RESX * (bpp / 8));
    }
    
    fwrite(framebuffer, (pitch * screenHeight) * sizeof(uint8_t), 1, file);
    fseek(file, 0, SEEK_SET);
    //printf("Wrote to framebuffer!\n");

    //handleKeyInput();
}

void DG_SleepMs(uint32_t ms) {
    sleepms(ms);
}

uint32_t DG_GetTicksMs() {
    return (uint32_t)xtime();
}

int DG_GetKey(int* pressed, uint8_t* doomKey) {
    if(keyqueuereadindex == keyqueuewriteindex) {
        // funny empty queue
        return 0;
    } else {
        uint16_t keyData = keyqueue[keyqueuereadindex];
        keyqueuereadindex++;
        keyqueuereadindex %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *doomKey = keyData & 0xFF;

        return 1;
    }
}

void DG_SetWindowTitle(const char* title) {
    printf("Attempted to set window title!\n");
}