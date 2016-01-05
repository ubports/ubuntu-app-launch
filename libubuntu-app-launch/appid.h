
#include "type-tagger.h"

#pragma once
#pragma GCC visibility push(default)

namespace Ubuntu {
namespace AppLaunch {

struct AppID {
	struct PackageTag;
	struct AppNameTag;
	struct VersionTag;

	typedef TypeTagger<PackageTag, std::string> Package;
	typedef TypeTagger<AppNameTag, std::string> AppName;
	typedef TypeTagger<VersionTag, std::string> Version;

	Package package;
	AppName appname;
	Version version;

	operator std::string() const;
	int operator == (const AppID &other) const;
	int operator != (const AppID &other) const;

	AppID();
	AppID(Package pkg, AppName app, Version ver);
	bool empty () const;

	static AppID parse (const std::string &appid);

	enum ApplicationWildcard {
		FIRST_LISTED,
		LAST_LISTED
	};
	enum VersionWildcard {
		CURRENT_USER_VERSION
	};

	static AppID discover (const std::string &package);
	static AppID discover (const std::string &package,
						   const std::string &appname);
	static AppID discover (const std::string &package,
						   const std::string &appname,
						   const std::string &version);
	static AppID discover (const std::string &package,
						   ApplicationWildcard appwildcard);
	static AppID discover (const std::string &package,
						   ApplicationWildcard appwildcard,
						   VersionWildcard versionwildcard);
	static AppID discover (const std::string &package,
						   const std::string &appname,
						   VersionWildcard versionwildcard);
};

}; // namespace AppLaunch
}; // namespace Ubuntu

#pragma GCC visibility pop
