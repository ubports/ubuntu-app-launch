
#include <vector>
#include <memory>

#include <mir_toolkit/mir_prompt_session.h>

#include "appid.h"
#include "type-tagger.h"

#pragma once
#pragma GCC visibility push(default)

namespace Ubuntu {
namespace AppLaunch {

class Registry;

class Helper {
public:
	struct TypeTag;
	struct URLTag;

	typedef TypeTagger<TypeTag, std::string> Type;
	typedef TypeTagger<URLTag, std::string> URL;

	static std::shared_ptr<Helper> create (Type type,
	                                       AppID appid,
	                                       std::shared_ptr<Registry> registry);

	virtual AppID appId() = 0;

	class Instance {
	public:
		/* Query lifecycle */
		virtual bool isRunning() = 0;

		/* Manage lifecycle */
		virtual void stop() = 0;
	};

	virtual bool hasInstances() = 0;
	virtual std::vector<std::shared_ptr<Instance>> instances() = 0;

	virtual std::shared_ptr<Instance> launch(std::vector<URL> urls = {}) = 0;
	virtual std::shared_ptr<Instance> launch(MirPromptSession * session, std::vector<URL> urls = {}) = 0;
};

}; // namespace AppLaunch
}; // namespace Ubuntu

#pragma GCC visibility pop
