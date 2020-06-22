#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct registers {
    uint16_t AX;
    uint16_t BX;
    uint16_t CX;
    uint16_t DX;
    uint16_t BP;
    uint16_t SI;
    uint16_t DI;

    uint16_t SP;
    uint16_t IP;

    uint16_t CS;
    uint16_t DS;
    uint16_t ES;
    uint16_t FS;
    uint16_t GS;
    uint16_t SS;

    uint16_t FLAGS;
} registers;

uint8_t *RAM;

registers regs;

#define BIT(num, bit) ((num) & (1 << (bit)))
#define CHARBIT(num, bit) (BIT(num, bit) ? '1' : '0')
#define SETBIT(num, bit, val) { if(val) (num) |= 1 << (bit); else (num) &= ~(1 << (bit)); }

#define MODRM16(rm, modregrm) {     \
    uint8_t mod = modregrm >> 6;    \
    switch(mod) {                   \
    case 3:                         \
        switch(modregrm & 0x07) {   \
        case 0:                     \
            (rm) = &regs.AX;        \
            break;                  \
        case 1:                     \
            (rm) = &regs.CX;        \
            break;                  \
        case 2:                     \
            (rm) = &regs.DX;        \
            break;                  \
        case 3:                     \
            (rm) = &regs.BX;        \
            break;                  \
        case 4:                     \
            (rm) = &regs.SP;        \
            break;                  \
        case 5:                     \
            (rm) = &regs.BP;        \
            break;                  \
        case 6:                     \
            (rm) = &regs.SI;        \
            break;                  \
        case 7:                     \
            (rm) = &regs.DI;        \
            break;                  \
        }                           \
        break;                      \
    default:                        \
        printf("Rdr forgot a 16-bit MOD parameter: %c%cxxxxxx\n", CHARBIT(mod, 1), CHARBIT(mod, 0)); \
        exit(1);                    \
    }                               \
}

bool tick() {
    uint8_t inst = RAM[(regs.CS<<4)+regs.IP];
    regs.IP++;
    switch(inst) {
    case 0x81: {
        uint8_t modregrm = RAM[(regs.CS<<4)+regs.IP]; regs.IP++;
        uint8_t reg = (modregrm >> 3) & 0x07;
        uint16_t *rm; MODRM16(rm, modregrm)
        uint16_t imm16 = RAM[(regs.CS<<4)+regs.IP]; regs.IP++;
        imm16 |= RAM[(regs.CS<<4)+regs.IP] << 8; regs.IP++;
        switch(reg) {
        case 7: {
            uint32_t tmp = *rm - imm16;

            SETBIT(regs.FLAGS, 0, tmp & 0x10000)

            bool parity = 1;
            uint8_t checkByte = tmp & 0xff;
            while(checkByte) {
                parity = !parity;
                checkByte &= checkByte - 1;
            }
            SETBIT(regs.FLAGS, 2, parity)

            SETBIT(regs.FLAGS, 4, !!(~(tmp ^ (*rm) ^ imm16) & 0x10)) // !! = not not; converts to boolean

            SETBIT(regs.FLAGS, 6, !(tmp & 0xFFFF))

            SETBIT(regs.FLAGS, 7, !!(tmp & 0x8000))

            if(!((*rm) & 0x8000) && (imm16 & 0x8000) && (tmp & 0x8000)) SETBIT(regs.FLAGS, 11, 1)
            else if(((*rm) & 0x8000) && !(imm16 & 0x8000) && !(tmp & 0x8000)) SETBIT(regs.FLAGS, 11, 1)
            else SETBIT(regs.FLAGS, 11, 0)

            return 0;
        }
        default:
            printf("Rdr forgot a subinstruction: 0x81 xx%c%c%cxxx\n", CHARBIT(reg, 2), CHARBIT(reg, 1), CHARBIT(reg, 0));
            exit(1);
        }
        break;
    }
    default:
        printf("Rdr forgot an instruction: %02X\n", inst);
        exit(1);
    }
}

int main(int argc, char **argv) {
    if(argc < 2) {
        printf("Usage: %s <executable>\n", argv[0]);
        return 0;
    }
    regs.AX = 0; // Technically undefined
    regs.BX = 0; // Technically undefined, same as AX
    regs.CX = 0xFF;
    regs.DX = 0x600; // PSP segment
    regs.DI = regs.SP = 0xFFFE;
    regs.SI = regs.IP = 0x100;
    regs.CS = regs.DS = regs.ES = 0x60; // PSP segment
    regs.SS = 0x60; // Also PSP segment

    // By the way, 0x600/0x60 is just one possible location for the program to be loaded; it could reasonably be anywhere.

    // Assume the executable is in COM (i.e. raw) format for now; don't want to heck with MZ
    RAM = malloc(640*1024);

    {
        FILE *com = fopen(argv[1], "rb");
        if(!com) {
            printf("Reading the heckin' COM heckin' failed: %s\n", strerror(errno));
            exit(1);
        }
        int curVal = getc(com);
        size_t index = 0;
        while(curVal != EOF) {
            RAM[(regs.CS<<4)+regs.IP+(index++)] = curVal;
            curVal = getc(com);
        }
        if(!feof(com)) {
            printf("Reading the heckin' COM heckin' failed: %s\n", strerror(errno));
            exit(1);
        }
        if(fclose(com))
            printf("Closing the COM failed: %s\nOh well.\n", strerror(errno));
    }

    while(1) {
        if(tick()) getchar(); // Pause
    }
}
