// <edit>
#ifndef LL_LLASSETCONVERTER_H
#define LL_LLASSETCONVERTER_H

#include "llcommon.h"
#include "llassettype.h"

class LLAssetConverter
{
public:
	static LLAssetType::EType convert(const std::string &src_filename, const std::string &filename);
	static bool copyFile(const std::string &src_filename, const std::string &dest_filename);
	static bool copyBVH(const std::string &src_filename, const std::string &dst_filename);
};
#endif
// </edit>
