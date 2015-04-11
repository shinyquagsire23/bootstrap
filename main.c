#include <3ds.h>
#include <3ds/sdmc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>
//#include "payload_bin.h"
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

void cleanup(void){
	sdmcExit();
	hbExit();
	gfxExit();
}

int main()
{
	// Initialize services
	gfxInitDefault(); // graphics
	hbInit();
	sdmcInit();

	//consoleInit(GFX_TOP, NULL);

	//arm9_payload = payload_bin;
	//arm9_payload_size = payload_bin_size;
	
	// Open payload on SD card
	FILE *payloadFile = fopen("payload.bin", "rb");
	if(payloadFile == NULL){
		printf("Error loading payload.bin\n");
		waitKey();
		cleanup();
		return 0;
	}
	
	// Get size of payload
	fseek(payloadFile, 0, SEEK_END);
	off_t size = ftell(payloadFile);
	fseek(payloadFile, 0, SEEK_SET);
	
	// Allocate the payload buffer
	printf("Allocating payload buffer...");
	arm9_payload = malloc(size);
	if(!arm9_payload){
		printf("Could not allocate arm9 buffer, is the payload to large?");
		waitKey();
		cleanup();
		return 0;
	}
	
	// Read the payload
	printf("Reading payload...");
	off_t read = fread(arm9_payload, 1, size, payloadFile);
	fclose(payloadFile);
	
	// Basic integrity check
	if(size != read){
		printf("Could not read payload.bin");
		waitKey();
		cleanup();
		return 0;
	}
	
	arm9_payload_size = size;
	
    while (!doARM11Hax());

    printf("Success!\n\n");
	waitKey();

	// Exit services
	cleanup();
	
	// Return to hbmenu
	return 0;
}
