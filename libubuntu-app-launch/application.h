
#include <sys/types.h>
#include <vector>
#include <memory>
#include <list>

#include "type-tagger.h"

#pragma once
#pragma GCC visibility push(default)

namespace Ubuntu {
namespace AppLaunch {

class Registry;

class Application {
public:
	struct PackageTag;
	struct AppNameTag;
	struct VersionTag;
	struct AppIDTag;
	struct URLTag;

	typedef TypeTagger<PackageTag, std::string> Package;
	typedef TypeTagger<AppNameTag, std::string> AppName;
	typedef TypeTagger<VersionTag, std::string> Version;
	typedef TypeTagger<AppIDTag, std::string> AppID;
	typedef TypeTagger<URLTag, std::string> URL;

	static std::shared_ptr<Application> create (const Package &package,
	                                            const AppName &appname,
	                                            const Version &version,
	                                            std::shared_ptr<Registry> registry);

	/* System level info */
	virtual const Package &package() = 0;
	virtual const AppName &appname() = 0;
	virtual const Version &version() = 0;
	virtual AppID appId() = 0;

	class Info {
	public:
		struct NameTag;
		struct DescriptionTag;
		struct IconPathTag;
		struct CategoryTag;

		typedef TypeTagger<NameTag, std::string> Name;
		typedef TypeTagger<DescriptionTag, std::string> Description;
		typedef TypeTagger<IconPathTag, std::string> IconPath;
		typedef TypeTagger<CategoryTag, std::string> Category;

		/* Package provided user visible info */
		virtual const Name &name() = 0;
		virtual const Description &description() = 0;
		virtual const IconPath &iconPath() = 0;
		virtual std::list<Category> categories() = 0;
	};

	virtual std::shared_ptr<Info> info() = 0;

	class Instance {
	public:
		/* Query lifecycle */
		virtual bool isRunning() = 0;
		virtual pid_t primaryPid() = 0;
		virtual bool hasPid(pid_t pid) = 0;
		virtual const std::string &logPath() = 0;

		/* Manage lifecycle */
		virtual void pause() = 0;
		virtual void resume() = 0;
		virtual void stop() = 0;
	};

	virtual bool hasInstances() = 0;
	virtual std::vector<std::shared_ptr<Instance>> instances() = 0;

	virtual std::shared_ptr<Instance> launch(std::vector<URL> urls = {}) = 0;
};

}; // namespace AppLaunch
}; // namespace Ubuntu

#pragma GCC visibility pop
