#ifndef __HIPPO_GRID_MANAGER_H__
#define __HIPPO_GRID_MANAGER_H__


#include <map>
#include <string>

#ifndef XML_STATIC
#define XML_STATIC
#endif

#ifdef LL_STANDALONE
#include <expat.h>
#else
#include "expat/expat.h"
#endif

class LLSD;


class HippoGridInfo
{
public:
	enum Platform {
		PLATFORM_OTHER = 0,
		PLATFORM_AURORA,
		PLATFORM_OPENSIM,
		PLATFORM_SECONDLIFE,
		PLATFORM_LAST
	};
	enum SearchType {
		SEARCH_ALL_EMPTY,
		SEARCH_ALL_QUERY,
		SEARCH_ALL_TEMPLATE
	};

	explicit HippoGridInfo(const std::string& gridName);

	Platform           getPlatform();
	bool isOpenSimulator() const;
	bool isSecondLife() const;
	const std::string& getGridName()        const;
	const std::string& getGridOwner()       const;	
	const std::string& getLoginUri()        const;
	const std::string& getLoginPage()       const;
	const std::string& getHelperUri()       const;
	const std::string& getWebSite()         const;
	const std::string& getSupportUrl()      const;
	const std::string& getRegisterUrl()     const;
	const std::string& getPasswordUrl()     const;
	// Returns the url base used for the Web Search tab
	const std::string& getSearchUrl()       const;
	const std::string& getGridMessage()     const;
	const std::string& getFirstName()       const;
	const std::string& getLastName()        const;
	const std::string& getAvatarPassword()  const;
	const std::string& getVoiceConnector()  const { return mVoiceConnector; }
	std::string getSearchUrl(SearchType ty, bool is_web) const;
	bool isRenderCompat()                   const;
	int getMaxAgentGroups() const { return mMaxAgentGroups; }

	const std::string& getCurrencySymbol()     const;
	const std::string& getRealCurrencySymbol() const;
	std::string getUploadFee()                 const;
	std::string getGroupCreationFee()          const;
	std::string getDirectoryFee()              const;

	void setPlatform (const std::string& platform);
	void setPlatform (Platform platform);
	void setGridName (const std::string& gridName);
	void setLoginUri (const std::string& loginUri);
	void setLoginPage(const std::string& loginPage);
	void setHelperUri(const std::string& helperUri);
	void setWebSite  (const std::string& website);
	void setSupportUrl(const std::string& url);
	void setRegisterUrl(const std::string& url);
	void setPasswordUrl(const std::string& url);
	// sets the url base used for the Web Search tab
	void setSearchUrl(const std::string& url);
	void setGridMessage(const std::string& message);
	void setRenderCompat(bool compat);
	void setMaxAgentGroups(int max)                   { mMaxAgentGroups = max;   }
	void setFirstName(const std::string& firstName);
	void setLastName(const std::string& lastName);
	void setAvatarPassword(const std::string& avatarPassword);
	void setVoiceConnector(const std::string& vc)     { mVoiceConnector = vc;    }

	void setCurrencySymbol(const std::string& sym);
	void setRealCurrencySymbol(const std::string& sym);
	void setDirectoryFee(int fee);
	bool supportsInvLinks();
	void setSupportsInvLinks(bool b);

	bool retrieveGridInfo();

	static const char* getPlatformString(Platform platform);

	static HippoGridInfo FALLBACK_GRIDINFO;
	static void initFallback();

private:
	Platform mPlatform;
	std::string mGridName;
	std::string mLoginUri;
	std::string mLoginPage;
	std::string mHelperUri;
	std::string mWebSite;
	std::string mSupportUrl;
	std::string mRegisterUrl;
	std::string mPasswordUrl;
	std::string mSearchUrl;
	std::string mVoiceConnector;
	std::string mFirstName;
	std::string mLastName;
	std::string mAvatarPassword;
	bool mRenderCompat;
	bool mInvLinks;
	int mMaxAgentGroups;

	std::string mCurrencySymbol;
	std::string mRealCurrencySymbol;
	int mDirectoryFee;
	std::string mGridMessage;

	// for parsing grid info XML
	enum XmlState 
	{
		XML_VOID, XML_PLATFORM, XML_GRIDNAME,
		XML_LOGINURI, XML_LOGINPAGE, XML_HELPERURI,
		XML_WEBSITE, XML_SUPPORT, XML_REGISTER, XML_PASSWORD, XML_SEARCH, XML_MESSAGE
	};
	XmlState mXmlState;

	static std::string sanitizeUri(std::string &uri);
	void formatFee(std::string &fee, int cost, bool showFree) const;

	static void onXmlElementStart(void* userData, const XML_Char* name, const XML_Char** atts);
	static void onXmlElementEnd(void* userData, const XML_Char* name);
	static void onXmlCharacterData(void* userData, const XML_Char* s, int len);
};


class HippoGridManager
{
public:
	HippoGridManager();
	~HippoGridManager();

	void init();
	void saveFile();
	void discardAndReload();

	HippoGridInfo* getGrid(const std::string& grid) const;
	HippoGridInfo* getConnectedGrid()               const;
	HippoGridInfo* getCurrentGrid()                 const;
	const std::string& getDefaultGridNick()         const;
	const std::string& getCurrentGridNick()         const;

	void setDefaultGrid(const std::string& grid);
	void setCurrentGrid(const std::string& grid);
	void setCurrentGridAsConnected();

	void addGrid(HippoGridInfo* grid);
	void deleteGrid(const std::string& grid);

	typedef std::map<std::string, HippoGridInfo*>::iterator GridIterator;
	GridIterator beginGrid() { return mGridInfo.begin(); }
	GridIterator endGrid() { return mGridInfo.end(); }

private:
	std::map<std::string, HippoGridInfo*> mGridInfo;
	std::string mDefaultGrid;
	std::string mCurrentGrid;
	HippoGridInfo* mConnectedGrid;
	int mDefaultGridsVersion;

	void cleanup();
	void loadFromFile();
	void parseFile(const std::string& fileName, bool mergeIfNewer);
	void parseUrl(const std::string url, bool mergeIfNewer);
	void parseData(LLSD &gridInfo, bool mergeIfNewer);
};


extern HippoGridManager* gHippoGridManager;


#endif
