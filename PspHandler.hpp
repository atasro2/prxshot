/*
 *  prxshot module
 *
 *  Copyright (C) 2011  Codestation
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PSPHANDLER_H_
#define PSPHANDLER_H_

// pspinit.h doesn't have C style exports in C++ ¬_¬
#ifdef __cplusplus
extern "C" {
#endif
#include <pspinit.h>
#ifdef __cplusplus
}
#endif
#include "pspdefs.h"

class PspHandler {
    enum game_type {HOMEBREW, UMD_ISO, XMB, PSN, PSX};

    //static variables
    static STMOD_HANDLER previous;
    static volatile int loader_found;
    static bool module_found;
    static bool eboot_found;
    game_type type;
    char *pbp_path;
    // string array
    static const char *blacklist[];
    //functions
    static int module_start_handler(SceModule2 *module);
    static bool checkBlacklist(const char *str);
public:
    enum boot_type {FLASH = PSP_BOOT_FLASH,
                    DISC = PSP_BOOT_DISC,
                    MS = PSP_BOOT_MS };

    enum app_type { VSH = PSP_INIT_KEYCONFIG_VSH,
                    GAME = PSP_INIT_KEYCONFIG_GAME,
                    POPS = PSP_INIT_KEYCONFIG_POPS};

    enum model_type {MODEL_PHAT, MODEL_SLIM, MODEL_GO = 4};

    PspHandler();
    inline int updated() { return loader_found ? loader_found-- : 0; }
    inline const char *getPBPPath() { return pbp_path; }
    inline game_type getGameType() { return type; }
    inline boot_type bootFrom() { return static_cast<boot_type>(sceKernelBootFrom()); }
    inline app_type applicationType() { return static_cast<app_type>(sceKernelInitKeyConfig()); }
    inline model_type getModel() { return static_cast<model_type>(sceKernelGetModel()); }
    int getKeyPress();
    bool isPressed(unsigned int buttons);
    ~PspHandler();
};

#endif /* PSPHANDLER_H_ */