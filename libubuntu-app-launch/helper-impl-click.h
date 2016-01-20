/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Ted Gould <ted.gould@canonical.com>
 */

#include <list>

#include "helper.h"

namespace Ubuntu {
namespace AppLaunch {
namespace HelperImpls {

class Click : public Helper {
public:
	Click (Helper::Type type, AppID appid, std::shared_ptr<Registry> registry) :
		_type(type), _appid(appid), _registry(registry)
	{
	}

	AppID appId() override {
		return _appid;
	}

	bool hasInstances() override;
	std::vector<std::shared_ptr<Helper::Instance>> instances() override;

	std::shared_ptr<Helper::Instance> launch(std::vector<Helper::URL> urls = {}) override;
	std::shared_ptr<Helper::Instance> launch(MirPromptSession * session, std::vector<Helper::URL> urls = {}) override;

	static std::list<std::shared_ptr<Helper>> running(Helper::Type type, std::shared_ptr<Registry> registry);

private:
	Helper::Type _type;
	AppID _appid;
	std::shared_ptr<Registry> _registry;
};

}; // namespace HelperImpl
}; // namespace AppLaunch
}; // namespace Ubuntu

