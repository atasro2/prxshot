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

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspthreadman.h>
#include <pspctrl.h>
#include <pspinit.h>

#include <stdio.h>
#include <string.h>

#include "pspdefs.h"
#include "kalloc.h"
#include "bitmap.h"
#include "pbp.h"
#include "minIni.h"
#include "logger.h"

PSP_MODULE_INFO("prxshot", 0x1000, 0, 3);
PSP_MAIN_THREAD_ATTR(0);
PSP_HEAP_SIZE_KB(8);


#define PICTURE_DIR_MS "ms0:/PSP/SCREENSHOT"
#define PICTURE_DIR_GO "ef0:/PSP/SCREENSHOT"
#define GAMEID_DIR "disc0:/UMD_DATA.BIN"
#define SHOT_BLK_NAME "shot_blk"
#define MAX_IMAGES 10000
#define BMP_SIZE 391734

char eboot_path[128];
char ini_path[128];
char screenshot_filename[64];
char screenshot_basedir[32];

SceUID last_id = 0;
int game_found = 0;
int clear_cache = -1;

int directory_ready = 0;

// previous module handler
STMOD_HANDLER previous = NULL;
int module_found = 0;

int key_button;
char picture[32];
int force_ms0;

void *get_mem(SceSize size, int *id) {
    void *mem = NULL;
    int api = sceKernelInitKeyConfig();
    if((game_found || api == PSP_INIT_KEYCONFIG_VSH) && sceKernelGetModel() >= PSP_MODEL_SLIM) {
        // use the umd cache only if is a game and slim or superior
        mem = kalloc(size, SHOT_BLK_NAME, id, PSP_MEMORY_PARTITION_UMDCACHE, PSP_SMEM_Low);
    }
    if(!mem) {
        // else get the memory from kernel
        mem = kalloc(size, SHOT_BLK_NAME, id, PSP_MEMORY_PARTITION_KERNEL, PSP_SMEM_Low);
        if(!mem && api == PSP_INIT_KEYCONFIG_GAME) {
            // as a last resort, use the volatile mem
            mem = kalloc_volatile();
        }
    }
	return mem;
}

int take_shot(const char *path) {
	void *frame_addr;
	SceUID block_id = -1;
	int frame_width, pixel_format;
	unsigned int ptr;
	void *mem = get_mem(BMP_SIZE, &block_id);
	if(mem) {
		sceDisplayWaitVblankStart();
		if(sceDisplayGetFrameBuf(&frame_addr, &frame_width, &pixel_format, PSP_DISPLAY_SETBUF_NEXTFRAME) >= 0 && frame_addr) {
			ptr = (unsigned int)frame_addr;
			ptr |= ptr & 0x80000000 ?  0xA0000000 : 0x40000000;
			bitmapWrite((void *)ptr, mem, pixel_format, path);
			block_id >= 0 ? kfree(block_id) : kfree_volatile();
			return 0;
		}
	}
	return -1;
}

int update_filename(const char *basedir, char *filename) {
	int end = last_id;
	do {
		sprintf(filename, picture, basedir, last_id++);
		if(last_id == MAX_IMAGES)
			last_id = 0;
		SceUID fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
		if(fd < 0) {
			return last_id;
		}
		sceIoClose(fd);
	} while(end != last_id);
	return -1;
}

int get_gameid(char *buffer) {
    char gameid[12];
    // check if an UMD (or ISO) is present
    kprintf("Trying to open %s\n", GAMEID_DIR);
	SceUID fd = sceIoOpen(GAMEID_DIR, PSP_O_RDONLY, 0777);
	if(fd >= 0) {
	    kprintf("UMD/ISO found\n");
		game_found = 1;
		sceIoRead(fd, gameid, 10);
		gameid[10] = 0;
		sceIoClose(fd);
		strcpy(buffer, gameid);
		if(gameid[4] == '-')
		    strcpy(buffer + 4, gameid + 5);
		return 1;
	} else if(sceKernelBootFrom() != PSP_BOOT_DISC) {
	    if(*eboot_path) {
            if(generate_gameid(eboot_path, gameid, sizeof(gameid))) {
                strcpy(buffer, gameid);
                game_found = 1;
                return 1;
            }
	    }
	}
	return 0;
}

void build_gamedir(char *dir, const char *argp) {
    if(!directory_ready) {
        int model = sceKernelGetModel();
        if(force_ms0)
            model = PSP_MODEL_SLIM;
        strcpy(dir, model == PSP_MODEL_GO ? PICTURE_DIR_GO : PICTURE_DIR_MS);
        strcat(dir,"/");
        if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_VSH) {
            strcat(dir, "XMB");
        } else if(!get_gameid(dir + strlen(model == PSP_MODEL_GO ? PICTURE_DIR_GO : PICTURE_DIR_MS)+1)) {
            strcat(dir, "Homebrew");
        }
        kprintf("Creating directory %s\n", dir);
        sceIoMkdir(dir, 0777);
        directory_ready = 1;
    }
}

// this causes problems to game categories D:
void update_xmb_cache() {
    if(clear_cache < 0) {
        clear_cache = ini_getbool("General", "XMBClearCache", 0, ini_path);
        kprintf("Read XMBClearCache: %i\n", clear_cache);
    }
    if(clear_cache && sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_VSH) {
        kprintf("Refreshing the xmb cache\n");
        sceIoDevctl("fatms0:", 0x0240D81E, NULL, 0, NULL, 0);
    }
}

int pbp_thread_start(SceSize args, void *argp) {
    kprintf("pbp_thread_start called\n");
    char *str = *eboot_path ? eboot_path : NULL;
    write_pbp(screenshot_basedir, str, argp);
    // refresh the cache after creating the PSCM.DAT
    update_xmb_cache();
    return 0;
}

int module_start_handler(SceModule2 *module) {
    if(!module_found) {
        const char *path = NULL;
        if(*eboot_path) {
            path = sceKernelInitFileName();
            if(path) {
                strcpy(eboot_path, path);
            }
        }
        if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_POPS) {
            kprintf("POPS found: %s\n", path);
            directory_ready = 0;
            module_found = 1;
        } else if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_GAME) {
            // user module found
            if((module->text_addr & 0x80000000) != 0x80000000) {
                if(!strcmp(module->modname, "aLoader") &&
                // blacklist open idea loader
                !strcmp(module->modname, "OpenIdeaController") &&
                !strcmp(module->modname, "ISO Loader Eboot") &&
                // blacklist the Prometheus iso loader
                !strcmp(module->modname, "PLoaderGUI")) {
                    // loader found, so a ISO has been loaded
                    module_found = 1;
                    path = NULL;
                } else {
                    if(strcmp(module->modname, "sceKernelLibrary")) {
                        // eboot found
                        module_found = 1;
                        directory_ready = 0;
                    }
                }
            }
            module_found = 1;
        }
    }
    return previous ? previous(module) : 0;
}

#ifdef KPRINTF_ENABLED
void boot_info() {
    int boot = sceKernelBootFrom();
    int key = sceKernelInitKeyConfig();
    switch(boot) {
        case PSP_BOOT_MS:
            kprintf("Booting from Memory Stick\n");
            break;
        case PSP_BOOT_DISC:
            kprintf("Booting from UMD\n");
            break;
        case PSP_BOOT_FLASH:
            kprintf("Booting from Flash\n");
            break;
        default:
            kprintf("Booting from: %i\n", boot);
    }
    switch(key) {
        case PSP_INIT_KEYCONFIG_GAME:
            kprintf("Init mode: Game\n");
            break;
        case PSP_INIT_KEYCONFIG_VSH:
            kprintf("Init mode: VSH\n");
            break;
        case PSP_INIT_KEYCONFIG_POPS:
            kprintf("Init mode: POPS\n");
            break;
        default:
            kprintf("Init mode: %i\n", key);
        }
}
#endif

void read_settings(const char *argp) {
    create_path(ini_path, argp, "prxshot.ini");
    key_button = ini_getlhex("General", "ScreenshotKey", PSP_CTRL_NOTE, ini_path);
    kprintf("Read ScreenshotKey: %08X\n", key_button);
    ini_gets("General", "ScreenshotName", "%s/pic_%04d.bmp", picture, sizeof(picture), ini_path);
    kprintf("Read ScreenshotName: %s\n", picture);
    force_ms0 = ini_getbool("General", "PSPGoUseMS0", 0, ini_path);
    if(force_ms0) {
        kprintf("PSPGoUseMS0 enabled, forcing ms0\n");
        //Make sure that the /PSP directory exists first
        sceIoMkdir("ms0:/PSP", 0777);
    }
}

int refresh_directory(const char *dir) {
    if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_VSH) {
        SceUID dfd = sceIoDopen(dir);
        if(dfd >= 0) {
            sceIoDclose(dfd);
            return 0;
        } else {
            kprintf("Recreating %s\n", dir);
            sceIoMkdir(dir, 0777);
            return 1;
        }
    }
    return 0;
}

int thread_start(SceSize args, void *argp) {
    // read config file
    kprintf("PRXshot main thread started\n");
#ifdef KPRINTF_ENABLED
    boot_info();
#endif
    // get custom settings from prxshot.ini
    read_settings(argp);
    // clear buffer
    memset(eboot_path, 0, sizeof(eboot_path));
    if(sceKernelInitKeyConfig() != PSP_INIT_KEYCONFIG_VSH && sceKernelBootFrom() != PSP_BOOT_DISC) {
        kprintf("Booting from Memory Stick/Internal Storage\n");
        previous = sctrlHENSetStartModuleHandler(module_start_handler);
    }
	int picture_id = 0;
	int pbp_created = 0;
	kprintf("Entering screenshot loop\n");
	while(picture_id >= 0) {
		SceCtrlData pad;
		sceCtrlPeekBufferPositive(&pad, 1);
		if(pad.Buttons) {
			if((pad.Buttons & key_button) == key_button) {
			    build_gamedir(screenshot_basedir, argp);
			    picture_id = update_filename(screenshot_basedir, screenshot_filename);
				// recreate the XMB screenshot directory if is deleted
			    pbp_created = refresh_directory(screenshot_basedir) ? 0 : 1;
				kprintf("Taking shot\n");
				if(take_shot(screenshot_filename) == 0) {
				    kprintf("Screenshot OK\n");
				} else {
				    kprintf("Screenshot fail\n");
				}
				//update the filename and wait for the next shot
				picture_id = update_filename(screenshot_basedir, screenshot_filename);
				//launch a thread after the first shot to create the PSCM.DAT
				if(!pbp_created) {
				    SceUID thid = sceKernelCreateThread("pbp_thread", pbp_thread_start, 0x20, 4096, 0, 0);
				    if(thid >= 0) {
				        sceKernelStartThread(thid, args, argp);
				    }
				    pbp_created = 1;
				} else {
				    update_xmb_cache();
				}
			}
		}
		sceKernelDelayThread(100000); //1.000.000 = 1 seg
	}
	return 0;
}

int module_start(SceSize argc, void *argp) {
    kprintf(">>>>>>>>>>>>>>>>>\nPRXshot started\n");
	SceUID thid = sceKernelCreateThread("prxshot", thread_start, 0x10, 4096, 0, 0);
	if(thid >= 0)
		sceKernelStartThread(thid, argc, argp);
	return 0;
}

int module_stop(SceSize argc, void *argp) {
	return 0;
}
