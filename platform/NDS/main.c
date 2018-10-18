#include <nds.h>
#include <fat.h>
//#include <nds/registers_alt.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../common/supervision.h"

// *** DeSmuME ***
// Slot-1: R4, Slot-2: Auto
// Makefile.nds
// *** ELSE (test it on real DS) ***
// Makefile from  nds-examples/filesystem/libfat/libfatdir/
//#define FOR_DESMUME

uint8 *buffer;
uint32 bufferSize = 0;
#ifdef FOR_DESMUME
#undef iprintf
#define iprintf(x)
uint16 screenBuffer[161 * 161]; // I have no idea
#else
uint16 screenBuffer[160 * 160];
#endif

int LoadROM(const char *filename)
{
   if (buffer != NULL) {
       free(buffer);
       buffer = NULL;
   }

   FILE *romfile = fopen(filename, "rb");
   if (romfile == NULL) {
       //printf("fopen(): Unable to open file!\n");
       return 1;
   }
   fseek(romfile, 0, SEEK_END);
   bufferSize = ftell(romfile);
   fseek(romfile, 0, SEEK_SET);

   buffer = (uint8_t *)malloc(bufferSize);

   fread(buffer, bufferSize, 1, romfile);

   if (fclose(romfile) == EOF) {
       //printf("fclose(): Unable to close file!\n");
       return 1;
   }
   return 0;
}

void InitVideo(void)
{
    powerOn(POWER_ALL); // POWER_ALL_2D

    //irqInit(); // ARM7
    //irqSet(IRQ_VBLANK, 0);

    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
#ifdef FOR_DESMUME
    videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);
#endif

    vramSetPrimaryBanks(VRAM_A_MAIN_BG_0x06000000, VRAM_B_LCD,
                        VRAM_C_SUB_BG, VRAM_D_LCD);

#ifdef FOR_DESMUME
    // Draw chessboard on second screen
    uint16* map0 = (uint16*)SCREEN_BASE_BLOCK_SUB(1);
    REG_BG0CNT_SUB = BG_COLOR_256 | (1 << MAP_BASE_SHIFT);
    BG_PALETTE_SUB[0] = RGB15(10,10,10);
    BG_PALETTE_SUB[1] = RGB15( 0,16, 0);
    for (int iy = 0; iy < 24; iy++) {
        for (int ix = 0; ix < 32; ix++) {
            map0[iy * 32 + ix] = (ix ^ iy) & 1;
        }
    }
    for (int i = 0; i < 64 / 2; i++) {
        BG_GFX_SUB[i+32] = 0x0101;
    }

    // Doesn't work without it (tested on DeSmuME)
    consoleDebugInit(DebugDevice_CONSOLE);
    fprintf(stderr, "debug message on DS console screen");
#else
    consoleDemoInit();
#endif

    REG_BG3CNT = BG_BMP16_256x256;// | (1<<13);
    REG_BG3PA = 1 << 8;
    REG_BG3PB = 0; // BG SCALING X
    REG_BG3PC = 0; // BG SCALING Y
    REG_BG3PD = 1 << 8;
    REG_BG3X = -48<< 8;
    REG_BG3Y = -16<< 8;
}

void CheckKeys(void)
{
    scanKeys();
    uint32 keys = keysHeld();

    uint8 controls_state = 0;
    if (keys & KEY_RIGHT ) controls_state|=0x01;
    if (keys & KEY_LEFT  ) controls_state|=0x02;
    if (keys & KEY_DOWN  ) controls_state|=0x04;
    if (keys & KEY_UP    ) controls_state|=0x08;
    if (keys & KEY_B     ) controls_state|=0x10;
    if (keys & KEY_A     ) controls_state|=0x20;
    if (keys & KEY_SELECT) controls_state|=0x40;
    if (keys & KEY_START ) controls_state|=0x80;
    controls_state_write(0, controls_state);

    if (keys & KEY_L && keys & KEY_LEFT)
        supervision_set_color_scheme(0);
    if (keys & KEY_L && keys & KEY_RIGHT)
        supervision_set_color_scheme(1);
    if (keys & KEY_L && keys & KEY_UP)
        supervision_set_color_scheme(2);
    if (keys & KEY_L && keys & KEY_DOWN)
        supervision_set_color_scheme(3);
}

void FailedLoop(void)
{
    while (true) {
        swiWaitForVBlank();
        scanKeys();
        if (keysDown() & KEY_START)
            break;
    }
}

int main()
{
    InitVideo();
    iprintf("Potator NDS\n");

    // Map Game Cartridge memory to ARM9
    //WAIT_CR &= ~0x80;

    if (fatInitDefault()) {
        supervision_init();
        if (LoadROM("fat:/test.sv") == 1 && LoadROM("fat:/sv/test.sv") == 1) {
            iprintf("Unable to open ROM: test.sv\n");
            FailedLoop();
            return 1;
        }
    }
    else {
        iprintf("fatInitDefault() failure\n");
        FailedLoop();
        return 1;
    }

    supervision_load(&buffer, bufferSize);

    while (true) {
        CheckKeys();

        supervision_exec(screenBuffer);

        for (int j = 0; j < 160; j++) {
            // Copy frame buffer to screen
            dmaCopyWordsAsynch(3, screenBuffer + (j * 160), BG_GFX + (j * 256), 160 * 2);
        }
    }
    supervision_done();
    return 0;
}

