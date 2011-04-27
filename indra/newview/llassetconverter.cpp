// <edit>
#include "llviewerprecompiledheaders.h"
#include "llvfs.h"
#include "llapr.h"
#include "llvfile.h"
#include "llassetconverter.h"
#include "llviewertexturelist.h"
#include "llvorbisencode.h"
#include "llbvhloader.h"
// static
extern std::string STATUS[];
LLAssetType::EType LLAssetConverter::convert(std::string src_filename, std::string filename)
{
	std::string exten = gDirUtilp->getExtension(src_filename);
	LLAssetType::EType asset_type = LLAssetType::AT_NONE;
	if (exten.empty())
		return LLAssetType::AT_NONE;
	else if(exten == "bmp")
	{
		asset_type = LLAssetType::AT_TEXTURE;
		if (!LLViewerTextureList::createUploadFile(src_filename,
												 filename,
												 IMG_CODEC_BMP ))
		{
			return LLAssetType::AT_NONE;
		}
	}
	else if( exten == "tga")
	{
		asset_type = LLAssetType::AT_TEXTURE;
		if (!LLViewerTextureList::createUploadFile(src_filename,
												 filename,
												 IMG_CODEC_TGA ))
		{
			return LLAssetType::AT_NONE;
		}
	}
	else if( exten == "jpg" || exten == "jpeg")
	{
		asset_type = LLAssetType::AT_TEXTURE;
		if (!LLViewerTextureList::createUploadFile(src_filename,
												 filename,
												 IMG_CODEC_JPEG ))
		{
			return LLAssetType::AT_NONE;
		}
	}
 	else if( exten == "png")
 	{
 		asset_type = LLAssetType::AT_TEXTURE;
 		if (!LLViewerTextureList::createUploadFile(src_filename,
 												 filename,
 												 IMG_CODEC_PNG ))
 		{
 			return LLAssetType::AT_NONE;
 		}
 	}
	else if(exten == "wav")
	{
		asset_type = LLAssetType::AT_SOUND;
		if(encode_vorbis_file(src_filename, filename) != LLVORBISENC_NOERR)
		{
			return LLAssetType::AT_NONE;
		}
	}
	else if(exten == "ogg")
	{
		asset_type = LLAssetType::AT_SOUND;
		if(!copyFile(src_filename, filename))
		{
			return LLAssetType::AT_NONE;
		}
	}
	//else if(exten == "tmp") FIXME
	else if (exten == "bvh")
	{

		asset_type = LLAssetType::AT_ANIMATION;
		S32 file_size;
		LLAPRFile fp;
		fp.open(src_filename, LL_APR_RB, LLAPRFile::global, &file_size);
		if(!fp.getFileHandle()) return LLAssetType::AT_NONE;
		char* file_buffer = new char[file_size + 1];
		ELoadStatus load_status = E_ST_OK;
		S32 line_number = 0; 
		LLBVHLoader* loaderp = new LLBVHLoader(file_buffer, load_status, line_number);
		
		if(load_status == E_ST_NO_XLT_FILE)
		{
			llwarns << "NOTE: No translation table found." << llendl;
		}
		else
		{
			llwarns << "ERROR: [line: " << line_number << "] " << STATUS[load_status].c_str() << llendl;
		}
		S32 buffer_size = loaderp->getOutputSize();
		U8* buffer = new U8[buffer_size];
		LLDataPackerBinaryBuffer dp(buffer, buffer_size);
		loaderp->serialize(dp);
		LLAPRFile apr_file(filename, LL_APR_WB, LLAPRFile::global);
		apr_file.write(buffer, buffer_size);
		delete[] file_buffer;
		delete[] buffer;
		fp.close();
		apr_file.close();
	}
	else if (exten == "animatn")
	{
		asset_type = LLAssetType::AT_ANIMATION;
		if(!copyFile(src_filename, filename))
		{
			return LLAssetType::AT_NONE;
		}
	}
	else if(exten == "jp2" || exten == "j2k" || exten == "j2c")
	{
		asset_type = LLAssetType::AT_TEXTURE;
		if(!copyFile(src_filename, filename))
		{
			return LLAssetType::AT_NONE;
		}
	}
	else if(exten == "gesture")
	{
		asset_type = LLAssetType::AT_GESTURE;
		if(!copyFile(src_filename, filename))
		{
			return LLAssetType::AT_NONE;
		}
	}
	else if(exten == "notecard")
	{
		asset_type = LLAssetType::AT_NOTECARD;
		if(!copyFile(src_filename, filename))
		{
			return LLAssetType::AT_NONE;
		}
	}
	else if(exten == "lsl")
	{
		asset_type = LLAssetType::AT_LSL_TEXT;
		if(!copyFile(src_filename, filename))
		{
			return LLAssetType::AT_NONE;
		}
	}
	else if(exten == "eyes" || exten == "gloves" || exten == "hair" || exten == "jacket" || exten == "pants" || exten == "shape" || exten == "shirt" || exten == "shoes" || exten == "skin" || exten == "skirt" || exten == "socks" || exten == "underpants" || exten == "undershirt" || exten == "bodypart" || exten == "clothing")
	{
		asset_type = LLAssetType::AT_CLOTHING;
		if(!copyFile(src_filename, filename))
		{
			return LLAssetType::AT_NONE;
		}
	}
	else
	{
		llwarns << "Unhandled extension" << llendl;
		return LLAssetType::AT_NONE;
	}
	return asset_type;
}
BOOL LLAssetConverter::copyFile(std::string src_filename, std::string dst_filename)
{
	S32 src_size;
	LLAPRFile src_fp;
	src_fp.open(src_filename, LL_APR_RB, LLAPRFile::global, &src_size);
	if(!src_fp.getFileHandle()) return FALSE;
	LLAPRFile dst_fp;
	dst_fp.open(dst_filename, LL_APR_WB, LLAPRFile::global);
	if(!dst_fp.getFileHandle()) return FALSE;
	char* buffer = new char[src_size + 1];
	src_fp.read(buffer, src_size);
	dst_fp.write(buffer, src_size);
	src_fp.close();
	dst_fp.close();
	delete[] buffer;
	return TRUE;
}
// </edit>
