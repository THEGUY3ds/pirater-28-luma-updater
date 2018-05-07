#pragma once

#include "libs.h"

#define DEFAULT_SIGHAX_PATH "boot.firm"
//#define DEFAULT_MHAX_PATH "Luma3DS.dat"
//#define DEFAULT_3DSX_PATH "3DS/Luma3DS/Luma3DS.3dsx"

enum class PayloadType {
	SIGHAX,    /*!< sighax payload (boot.firm) */
	/*Menuhax, !< menuhax payload (Luma3DS.dat)             */
	/*Homebrew !< hblauncher payload (Luma3DS.3dsx/smdh)    */
};

struct ReleaseVer {
	std::string filename;
	std::string friendlyName;
	std::string url;
	size_t      fileSize;
};

struct ReleaseInfo {
	std::string name = "";
	std::string description = "";
	std::vector<ReleaseVer> versions = {};
	std::map<std::string, std::string> commits = {};
};

/* \brief Gets last official release (from THEGUY Github)
 *
 * \return ReleaseInfo containing the last release name and available versions
 */
ReleaseInfo releaseGetLatestStable();

/* \brief Gets the latest available nightly build (from astronautlevel2's website) 
 *
 * \return ReleaseInfo containing the last nightly
 */
ReleaseInfo releaseGetLatestHourly();

/* \brief Update to stable version
 * Gets the chosen payload (A9LH/Menuhax/3dsx) file from either a stable release or a nightly
 * The buffer must be free'd after used. If the release is not an nightly, the `payloadData` pointer must
 * increased by `offset` to get the correct bytes
 *
 * \param type        Payload type to fetch
 * \param release     Release data
 * \param isHourly    Wether the release is a nightly (.zip) or stable (.7z)
 * \param payloadData Pointer to fill with the payload bytes (should be nullptr when passing)
 * \param offset      Pointer to fill with the offset to the correct bytes (if 7z)
 * \param payloadSize Pointer to fill with size (in bytes) of the payload
 *
 * \return true if everything succeeds, false otherwise
 */
bool releaseGetPayload(const PayloadType type, const ReleaseVer& release, const bool isHourly, u8** payloadData, size_t* offset, size_t* payloadSize);