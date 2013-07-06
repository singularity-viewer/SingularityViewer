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

#include <boost/signals2.hpp>

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

	Platform getPlatform() { return mPlatform; }
	bool isOpenSimulator() const { return (mPlatform == PLATFORM_OPENSIM || mPlatform == PLATFORM_AURORA); }
	bool isAurora() const { return (mPlatform == PLATFORM_AURORA); }
	bool isSecondLife() const { return (mPlatform == PLATFORM_SECONDLIFE); }
	bool isAvination() const { return mIsInAvination; }
	bool isInProductionGrid() const { llassert(mPlatform == PLATFORM_SECONDLIFE); return mIsInProductionGrid; } // Should only be called if isSecondLife() returns true.
	const std::string& getGridName()        const { return mGridName; }
	const std::string& getGridOwner()       const;	
	const std::string& getLoginUri()        const { return mLoginUri; }
	const std::string& getLoginPage()       const { return mLoginPage; }
	const std::string& getHelperUri()       const { return mHelperUri; }
	const std::string& getWebSite()         const { return mWebSite; }
	const std::string& getSupportUrl()      const { return mSupportUrl; }
	const std::string& getRegisterUrl()     const { return mRegisterUrl; }
	const std::string& getPasswordUrl()     const { return mPasswordUrl; }
	// Returns the url base used for the Web Search tab
	const std::string& getSearchUrl()       const { return mSearchUrl; }
	const std::string& getGridMessage()     const { return mGridMessage; }
	const std::string& getVoiceConnector()  const { return mVoiceConnector; }
	std::string getSearchUrl(SearchType ty, bool is_web) const;
	bool isRenderCompat()                   const { return mRenderCompat; }
	std::string getGridNick() const;
	int getMaxAgentGroups() const { return mMaxAgentGroups; }

	const std::string& getCurrencySymbol()     const { return mCurrencySymbol; }
	const std::string& getCurrencyText()       const { return mCurrencyText; }
	const std::string& getRealCurrencySymbol() const { return mRealCurrencySymbol; }
	std::string getUploadFee()                 const;
	std::string getGroupCreationFee()          const;
	std::string getDirectoryFee()              const;

	void setPlatform (const std::string& platform);
	void setPlatform (Platform platform);
	void setGridNick (std::string gridNick);
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
	void setVoiceConnector(const std::string& vc)     { mVoiceConnector = vc;    }
	void setUPCSupported(bool on);
	bool getUPCSupported();

	void setCurrencySymbol(const std::string& sym);
	void setCurrencyText(const std::string& text);
	void setRealCurrencySymbol(const std::string& sym);
	void setDirectoryFee(int fee);
	bool supportsInvLinks();
	void setSupportsInvLinks(bool b);
	bool getAutoUpdate();
	void setAutoUpdate(bool b);	

	bool retrieveGridInfo();

	static const char* getPlatformString(Platform platform);
	static std::string sanitizeGridNick(const std::string &gridnick);

	static HippoGridInfo FALLBACK_GRIDINFO;
	static void initFallback();

private:
	Platform mPlatform;
	std::string mGridName;
	std::string mGridNick;
	std::string mLoginUri;
	std::string mLoginPage;
	std::string mHelperUri;
	std::string mWebSite;
	std::string mSupportUrl;
	std::string mRegisterUrl;
	std::string mPasswordUrl;
	std::string mSearchUrl;
	std::string mVoiceConnector;
	bool mIsInProductionGrid;
	bool mIsInAvination;
	bool mRenderCompat;
	bool mInvLinks;
	bool mAutoUpdate;
	bool mUPCSupported;
	int mMaxAgentGroups;

	std::string mCurrencySymbol;
	std::string mCurrencyText;
	std::string mRealCurrencySymbol;
	int mDirectoryFee;
	std::string mGridMessage;

	// for parsing grid info XML
	enum XmlState 
	{
		XML_VOID, XML_PLATFORM, XML_GRIDNAME, XML_GRIDNICK,
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
	typedef boost::signals2::signal<void (HippoGridInfo* pNewGrid, HippoGridInfo* pPrevGrid)> current_grid_change_signal_t;

	HippoGridManager();
	~HippoGridManager();

	void init();
	void saveFile();
	void discardAndReload();

	HippoGridInfo* getGrid(const std::string& grid) const;
	HippoGridInfo* getConnectedGrid()               const { return mConnectedGrid ? mConnectedGrid : getCurrentGrid(); }

	HippoGridInfo* getCurrentGrid()                 const;
	std::string	   getDefaultGridNick()         const;
	std::string	   getCurrentGridNick()         const;
	const std::string&	   getDefaultGridName()         const;
	const std::string&	   getCurrentGridName()         const;

	void setDefaultGrid(const std::string& grid);
	void setCurrentGrid(const std::string& grid);
	void setCurrentGridAsConnected();

	void addGrid(HippoGridInfo* grid);
	void deleteGrid(const std::string& grid);

	typedef std::map<std::string, HippoGridInfo*>::iterator GridIterator;
	GridIterator beginGrid() { return mGridInfo.begin(); }
	GridIterator endGrid() { return mGridInfo.end(); }

	boost::signals2::connection setCurrentGridChangeCallback( const current_grid_change_signal_t::slot_type& cb )
	{
		if(!mCurrentGridChangeSignal)
			mCurrentGridChangeSignal = new current_grid_change_signal_t;
		return mCurrentGridChangeSignal->connect(cb);
	}

private:
	friend class HippoGridInfo;
	std::map<std::string, HippoGridInfo*> mGridInfo;
	std::string mDefaultGrid;
	std::string mCurrentGrid;
	HippoGridInfo* mConnectedGrid;
	int mDefaultGridsVersion;

	current_grid_change_signal_t* mCurrentGridChangeSignal; 

	void cleanup();
	void loadFromFile();
	void parseFile(const std::string& fileName, bool mergeIfNewer);
	void parseUrl(const std::string url, bool mergeIfNewer);
	void parseData(LLSD &gridInfo, bool mergeIfNewer);
};


extern HippoGridManager* gHippoGridManager;


#endif
