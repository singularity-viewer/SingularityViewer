// <edit>
#include "llviewerprecompiledheaders.h"
#include "llvfs.h"
#include "llapr.h"
#include "llvfile.h"
#include "llassetconverter.h"
#include "llviewertexturelist.h"
#include "llvorbisencode.h"
#include "llwearable.h"
#include "llbvhloader.h"
#include <boost/bind.hpp>
struct conversion_s
{
	std::string extension;
	LLAssetType::EType type;
	boost::function<bool(const std::string&, const std::string&)> conv_fn;
}conversion_list[] =
{
	{	"bmp",	LLAssetType::AT_TEXTURE,	boost::bind(&LLViewerTextureList::createUploadFile,_1,_2,IMG_CODEC_BMP)	},
	{	"tga",	LLAssetType::AT_TEXTURE,	boost::bind(&LLViewerTextureList::createUploadFile,_1,_2,IMG_CODEC_TGA)	},
	{	"jpg",	LLAssetType::AT_TEXTURE,	boost::bind(&LLViewerTextureList::createUploadFile,_1,_2,IMG_CODEC_JPEG)},
	{	"jpeg",	LLAssetType::AT_TEXTURE,	boost::bind(&LLViewerTextureList::createUploadFile,_1,_2,IMG_CODEC_JPEG)},
	{	"png",	LLAssetType::AT_TEXTURE,	boost::bind(&LLViewerTextureList::createUploadFile,_1,_2,IMG_CODEC_PNG)	},
	{	"jp2",	LLAssetType::AT_TEXTURE,	&LLAssetConverter::copyFile	},
	{	"j2k",	LLAssetType::AT_TEXTURE,	&LLAssetConverter::copyFile	},
	{	"j2c",	LLAssetType::AT_TEXTURE,	&LLAssetConverter::copyFile	},
	{	"wav",	LLAssetType::AT_SOUND,		&encode_vorbis_file	},
	{	"ogg",	LLAssetType::AT_SOUND,		&LLAssetConverter::copyFile	},
	{	"bvh",	LLAssetType::AT_ANIMATION,	&LLAssetConverter::copyBVH	},
	{	"animatn",	LLAssetType::AT_ANIMATION,	&LLAssetConverter::copyFile	},
	{	"gesture",	LLAssetType::AT_GESTURE,	&LLAssetConverter::copyFile	},
	{	"notecard",	LLAssetType::AT_NOTECARD,	&LLAssetConverter::copyFile	},
	{	"lsl",	LLAssetType::AT_LSL_TEXT,		&LLAssetConverter::copyFile	},
	//tmp ?
};

// static
extern std::string STATUS[];
LLAssetType::EType LLAssetConverter::convert(const std::string &src_filename, const std::string &filename)
{
	std::string exten = gDirUtilp->getExtension(src_filename);
	for(U32 i = 0;i < sizeof(conversion_list) / sizeof(conversion_list[0]); ++i)
	{
		if(conversion_list[i].extension == exten)
		{
			return	conversion_list[i].conv_fn(src_filename, filename) ? 
						conversion_list[i].type : LLAssetType::AT_NONE;
		}
	}
	LLWearableType::EType wear_type = LLWearableType::typeNameToType(exten);
	if(wear_type != LLWearableType::WT_NONE && copyFile(src_filename, filename))
	{
		return  LLWearableType::getAssetType(wear_type);
	}

	llwarns << "Unhandled extension" << llendl;
	return LLAssetType::AT_NONE;
}
bool LLAssetConverter::copyFile(const std::string &src_filename, const std::string &dst_filename)
{
	S32 src_size;

	LLAPRFile src_fp(src_filename, LL_APR_RB, &src_size);
	if(!src_fp.getFileHandle()) return false;

	LLAPRFile dst_fp(dst_filename, LL_APR_WB);
	if(!dst_fp.getFileHandle()) return false;

	std::vector<char> buffer(src_size + 1); 

	src_fp.read(&buffer[0], src_size);
	dst_fp.write(&buffer[0], src_size);

	return true;
}
bool LLAssetConverter::copyBVH(const std::string &src_filename, const std::string &dst_filename)
{
	S32 file_size;

	LLAPRFile fp(src_filename, LL_APR_RB, &file_size);
	if(!fp.getFileHandle()) return false;

	std::vector<char> buffer(file_size + 1); 

	ELoadStatus load_status = E_ST_OK;
	S32 line_number = 0; 

	LLBVHLoader* loaderp = new LLBVHLoader(&buffer[0], load_status, line_number);

	if(load_status == E_ST_NO_XLT_FILE)
	{
		llwarns << "NOTE: No translation table found." << llendl;
	}
	else if(load_status != E_ST_OK)
	{
		llwarns << "ERROR: [line: " << line_number << "] " << STATUS[load_status].c_str() << llendl;
	}

	buffer.resize(loaderp->getOutputSize());

	LLDataPackerBinaryBuffer dp((U8*)&buffer[0], buffer.size());
	loaderp->serialize(dp);

	delete loaderp;

	LLAPRFile apr_file(dst_filename, LL_APR_RB, &file_size);
	if(!apr_file.getFileHandle()) return false;

	apr_file.write(&buffer[0], buffer.size());
	return true;
}
// </edit>
