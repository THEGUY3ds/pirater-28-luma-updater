#include "update.h"
#include "main.h"

#include "arnutil.h"
#include "console.h"
#include "lumautils.h"
#include "utils.h"

bool ctrnand = true; //true by default
static inline bool pathchange(u8* buf, const size_t bufSize, const std::string& path) {
	const static char original[] = "sdmc:/boot.firm";
	const static size_t prefixSize = 12; // S \0 D \0 M \0 C \0 : \0 / \0
	const static size_t originalSize = sizeof(original)/sizeof(char);
	u8 pathLength = path.length();

	if (pathLength > MAXPATHLEN) {
		logPrintf("Cannot accept payload path: too long (max %d chars)\n", MAXPATHLEN);
		return false;
	}

	logPrintf("Searching for \"%s\" in payload...\n", original);

	size_t curProposedOffset = 0;
	u8 curStringIndex = 0;
	bool found = false;

	// Byte-by-byte search. (memcmp might be faster?)
	// Since "s" (1st char) is only used once in the whole string we can search in O(n)
	for (size_t offset = 0; offset < bufSize-originalSize; ++offset) {
		if (buf[offset] == original[curStringIndex] && buf[offset+1] == 0) {
			if (curStringIndex == originalSize - 1) {
				found = true;
				break;
			}
			if (curStringIndex == 0) {
				curProposedOffset = offset;
			}
			curStringIndex++;
			offset++; // Skip one byte because Unicode
			continue;
		}

		if (curStringIndex > 0) {
			curStringIndex = 0;
		}
	}

	// Not found?
	if (!found) {
		logPrintf("Could not find payload path, is this even a valid payload?\n");
		return false;
	}

	// Replace "boot.firm" with own payload path
	size_t offset = curProposedOffset + prefixSize;
	u8 i = 0;
	for (i = 0; i < pathLength; ++i) {
		buf[offset + i*2] = path[i];
	};
	// Replace remaining characters from original path with 0s
	for (i = pathLength; i < originalSize; ++i) {
		buf[offset + i*2] = 0;
	}

	return true;
}

static inline bool backupSighax(const std::string& payloadName) {
	std::ifstream original(payloadName, std::ifstream::binary);
	if (!original.good()) {
		logPrintf("Could not open %s\n", payloadName.c_str());
		return false;
	}

	std::string backupName = payloadName + ".bak";
	std::ofstream target(backupName, std::ofstream::binary);
	if (!target.good()) {
		logPrintf("Could not open %s\n", backupName.c_str());
		original.close();
		return false;
	}

	target << original.rdbuf();

	original.close();
	target.close();

	//---------------------------------------------------------------- Ctr Archive code.
	if(ctrnand == true)
	{
		fsInit();
		FS_Archive ctrArchive;
		FS_Path path = fsMakePath (PATH_EMPTY,"");
		FS_Path ori = fsMakePath(PATH_ASCII, "/boot.firm");
		FS_Path back = fsMakePath(PATH_ASCII ,"/boot.firm.bak");
		Result ret = FSUSER_OpenArchive(&ctrArchive, ARCHIVE_NAND_CTR_FS,path);
		if(ret != 0)
		{
			logPrintf("FATAL\nCouldn't open CTR-NAND for renaming");
			FSUSER_CloseArchive (ctrArchive);
			fsExit();
			return false;
		}
		ret = FSUSER_RenameFile(ctrArchive,ori,ctrArchive,back);
		if((u32)ret == 0xC82044BE)
		{
			Result res = FSUSER_DeleteFile (ctrArchive, back);
			if(res == 0)
				FSUSER_RenameFile(ctrArchive,ori,ctrArchive,back);
			else
			{
				logPrintf("Something is critically wrong %08X\n",res);
				FSUSER_CloseArchive (ctrArchive);
				fsExit();
				return false;
			}
		}
		else if((u32)ret == 0xC8804478)
		{
			logPrintf("boot.firm not found on CTR-NAND.Skipping\n");
		}
		FSUSER_CloseArchive (ctrArchive);
		fsExit();
	}
	return true;
	//---------------------------------------------------------------------------
}

UpdateResult update(const UpdateInfo& args) {
	if(args.currentVersion.release == "7.1") {
		consoleScreen(GFX_BOTTOM);
		consoleClear();
		std::printf("%sUpdate aborted%s\n\nTo update to the latest version of Luma3DS, you need to update boot9strap first.\nPlease visit https://3ds.guide/updating-b9s for instructions.\n\n", CONSOLE_RED, CONSOLE_RESET);
		gfxFlushBuffers();
		logPrintf("FATAL\nIncompatible update path, aborting...\n");
		return { false, "INCOMPATIBLE UPDATE" };
	}

	consoleScreen(GFX_TOP);
	consoleInitProgress("Updating Luma3DS", "Performing preliminary operations", 0);

	consoleScreen(GFX_BOTTOM);
	consoleClear();
	logPrintf("Do you want to enable downloading boot.firm on CTR-NAND?\n Press A + X to enable\n Press B to disable\n\n\nEnabling allows you to update the SD-less version of Luma3DS as well. If in doubt, you should enable this option.\n");
	gfxFlushBuffers();
	while(aptMainLoop())
	{
		hidScanInput();
		if((hidKeysDown() & KEY_A)&&(hidKeysDown() & KEY_X))
		{
			ctrnand = true;
			logPrintf("CTR-NAND related operations enabled\n");
			break;
		}
		if(hidKeysDown() & KEY_B)
		{
			ctrnand = false;
			logPrintf("CTR-NAND related operations disabled\n");
			break;
		}
	}
	// Back up local file if it exists
	if (!args.backupExisting) {
		logPrintf("Payload backup is disabled in config, skipping...\n");
	} else if (!fileExists(args.payloadPath)) {
		logPrintf("Original payload not found, skipping backup...\n");
	} else {
		consoleScreen(GFX_TOP);
		consoleSetProgressData("Backing up old payload", 0.1);
		consoleScreen(GFX_BOTTOM);

		logPrintf("Copying %s to %s.bak...\n", args.payloadPath.c_str(), args.payloadPath.c_str());
		gfxFlushBuffers();
		if (!backupSighax(args.payloadPath)) {
			logPrintf("\nCould not backup %s (!!), aborting...\n", args.payloadPath.c_str());
			return { false, "BACKUP FAILED" };
		}
	}

	consoleScreen(GFX_TOP);
	consoleSetProgressData("Downloading payload", 0.3);
	consoleScreen(GFX_BOTTOM);

	logPrintf("Downloading %s\n", args.chosenVersion().url.c_str());
	gfxFlushBuffers();

	u8* payloadData = nullptr;
	size_t offset = 0;
	size_t payloadSize = 0;
	if (!releaseGetPayload(args.payloadType, args.chosenVersion(), args.isHourly, &payloadData, &offset, &payloadSize)) {
		logPrintf("FATAL\nCould not get sighax payload...\n");
		std::free(payloadData);
		return { false, "DOWNLOAD FAILED" };
	}

	if (args.migrateARN) {
		consoleScreen(GFX_TOP);
		consoleSetProgressData("Migrating AuReiNand -> Luma3DS", 0.8);
		consoleScreen(GFX_BOTTOM);

		logPrintf("Migrating AuReiNand install to Luma3DS...\n");
		if (!arnMigrate()) {
			logPrintf("FATAL\nCould not migrate AuReiNand install (?)\n");
			return { false, "MIGRATION FAILED" };
		}
	}

	if (!lumaMigratePayloads()) {
		logPrintf("WARN\nCould not migrate payloads\n\n");
	}

	consoleScreen(GFX_TOP);
	consoleSetProgressData("Saving payload to SD as well as CTR-NAND", 0.9);
	consoleScreen(GFX_BOTTOM);

	logPrintf("Saving payload to SD/CTR-NAND (as %s)...\n", args.payloadPath.c_str());
	std::ofstream sighaxfile("/" + args.payloadPath, std::ofstream::binary);
	sighaxfile.write((const char*)(payloadData + offset), payloadSize);
	sighaxfile.close();
	//----------------------------------------------------------------CTR ARCHIVE code
	if(ctrnand == true)
	{
		Handle log;
		fsInit();
		FS_Archive ctrArchive;
		FS_Path path = fsMakePath (PATH_EMPTY,"");
		Result ret = FSUSER_OpenArchive(&ctrArchive, ARCHIVE_NAND_CTR_FS,path);
		if(ret != 0)
		{
			logPrintf("FATAL\nCouldn't open CTR-NAND for writing");
			fsExit();
			return { false , "CTR-NAND Failure"};
		}
		FS_Path path2 = fsMakePath(PATH_ASCII, "/boot.firm");
		ret = FSUSER_OpenFile(&log,ctrArchive,path2,FS_OPEN_WRITE|FS_OPEN_CREATE,0x0);
		if(ret != 0)
		{
			logPrintf("FATAL\nCouldn't open boot.firm for writing");
			fsExit();
			FSUSER_CloseArchive (ctrArchive);
			return { false , "CTR-NAND Failure"};
		}
		ret = FSFILE_Write(log,NULL,0x0,(const char*)(payloadData + offset),payloadSize,FS_WRITE_FLUSH);
		if(ret != 0)
		{
			logPrintf("FATAL\nCouldn't write boot.firm");
			FSFILE_Close(log);
			FSUSER_CloseArchive (ctrArchive);
			fsExit();
			return { false , "CTR-NAND Failure"};
		}
		FSFILE_Close(log);
		FSUSER_CloseArchive (ctrArchive);
		fsExit();
	}
	logPrintf("All done, freeing resources and exiting...\n");
	std::free(payloadData);
	consoleClear();
	consoleScreen(GFX_TOP);
	return { true, "NO ERROR" };
}
UpdateResult restore(const UpdateInfo& args) {
	consoleScreen(GFX_BOTTOM);
	logPrintf("Restore payload on CTR-NAND also?\n Press A + X to enable.\n Press B to disable.\n");
	while(aptMainLoop())
	{
		hidScanInput();
		if((hidKeysDown() & KEY_A)&&(hidKeysDown() & KEY_X))
		{
			ctrnand = true;
			logPrintf("CTR-NAND related operations enabled\n");
			break;
		}
		if(hidKeysDown() & KEY_B)
		{
			ctrnand = false;
			logPrintf("CTR-NAND related operations disabled\n");
			break;
		}
	}
	// Rename current payload to .broken
	if (std::rename(args.payloadPath.c_str(), (args.payloadPath + ".broken").c_str()) != 0) {
		logPrintf("Can't rename current version");
		return { false, "RENAME1 FAILED" };
	}
	// Rename .bak to current
	if (std::rename((args.payloadPath + ".bak").c_str(), args.payloadPath.c_str()) != 0) {
		logPrintf("Can't rename backup to current payload name");
		return { false, "RENAME2 FAILED" };
	}
	// Remove .broken
	if (std::remove((args.payloadPath + ".broken").c_str()) != 0) {
		logPrintf("WARN: Could not remove current payload, please remove it manually");
	}
	//----------------------------------------------------CTR-ARCHIVE code
	if(ctrnand == true)
	{
		fsInit();
		FS_Archive ctrArchive;
		FS_Path path = fsMakePath (PATH_EMPTY,"");
		FS_Path ori = fsMakePath(PATH_ASCII, "/boot.firm");
		FS_Path back = fsMakePath(PATH_ASCII ,"/boot.firm.bak");
		Result ret = FSUSER_OpenArchive(&ctrArchive, ARCHIVE_NAND_CTR_FS,path);
		if(ret != 0)
		{
			logPrintf("FATAL\nCouldn't open CTR-NAND for restoring");
			FSUSER_CloseArchive (ctrArchive);
			fsExit();
			return {false,"CTR-NAND ERROR"};
		}
		FSUSER_DeleteFile (ctrArchive, ori);
		ret = FSUSER_RenameFile(ctrArchive,back,ctrArchive,ori);
		if(ret != 0)
		{
			logPrintf("Something is critically wrong %08X\n",ret);
			FSUSER_CloseArchive (ctrArchive);
			fsExit();
			return {false, "CTR-NAND ERROR"};
		}
		FSUSER_CloseArchive (ctrArchive);
		fsExit();
	}
	return { true, "NO ERROR" };
}