
#include <sys/types.h>
#include <vector>
#include <memory>
#include <list>

#pragma once

namespace Ubuntu {
namespace AppLaunch {

template <typename Tag, typename T>
class ApplicationTagger {
public:
	static ApplicationTagger<Tag, T> from_raw(const T& value) {
		return ApplicationTagger<Tag, T>(value);
	}
	const T& value() const {
		return _value;
	}
	operator T() const {
		return _value;
	}
private:
	ApplicationTagger(const T& value) : _value(value) { }
	T _value;
};

namespace Tags {
	/* Base AppID */
	struct Package;
	struct AppName;
	struct Version;
	struct AppID;

	/* App launch */
	struct URL;

	/* Info */
	struct Name;
	struct Description;
	struct IconPath;
	struct Category;
}

class Registry;

class Application {
public:
	typedef ApplicationTagger<Tags::Package, std::string> Package;
	typedef ApplicationTagger<Tags::AppName, std::string> AppName;
	typedef ApplicationTagger<Tags::Version, std::string> Version;
	typedef ApplicationTagger<Tags::AppID, std::string> AppID;
	typedef ApplicationTagger<Tags::URL, std::string> URL;

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
		typedef ApplicationTagger<Tags::Name, std::string> Name;
		typedef ApplicationTagger<Tags::Description, std::string> Description;
		typedef ApplicationTagger<Tags::IconPath, std::string> IconPath;
		typedef ApplicationTagger<Tags::Category, std::string> Category;

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
