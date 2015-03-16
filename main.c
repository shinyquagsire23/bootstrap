#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>

#include "bootstrap.h"

void waitKey() {
	while (aptMainLoop())
	{
		// Wait next screen refresh
		gspWaitForVBlank();

		// Read which buttons are currently pressed 
		hidScanInput();
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		
		// If START is pressed, break loop and quit
		if (kDown & KEY_X){
			break;
		}

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
	}
}

int main()
{
	// Initialize services
	gfxInitDefault(); // graphics
	hbInit();

	consoleInit(GFX_TOP, NULL);

    while (!doARM11Hax());

    printf("Success!\n\n");
	waitKey();

	// Exit services
	hbExit();
	gfxExit();
	
	// Return to hbmenu
	return 0;
}
