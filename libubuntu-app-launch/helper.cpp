/*
 * Copyright © 2016-2017 Canonical Ltd.
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

#include <algorithm>
#include <list>

#include "helper-impl.h"
#include "registry-impl.h"

#include "ubuntu-app-launch.h"

namespace ubuntu
{
namespace app_launch
{
namespace helper_impls
{

/**********************
 * Instance
 **********************/

BaseInstance::BaseInstance(const std::shared_ptr<jobs::instance::Base>& inst)
    : impl{inst}
{
}

BaseInstance::BaseInstance(const std::shared_ptr<Application::Instance>& inst)
    : impl{std::dynamic_pointer_cast<jobs::instance::Base>(inst)}
{
}

bool BaseInstance::isRunning()
{
    return impl->isRunning();
}

void BaseInstance::stop()
{
    impl->stop();
}

/**********************
 * Helper Class
 **********************/

Base::Base(const Helper::Type& type, const AppID& appid, const std::shared_ptr<Registry>& registry)
    : _type(type)
    , _appid(appid)
    , _registry(registry)
{
}

AppID Base::appId()
{
    return _appid;
}

bool Base::hasInstances()
{
    return instances().size() > 0;
}

std::vector<std::shared_ptr<Helper::Instance>> Base::instances()
{
    auto insts = _registry->impl->jobs->instances(_appid, _type.value());
    std::vector<std::shared_ptr<Helper::Instance>> wrapped;

    std::transform(insts.begin(), insts.end(), wrapped.begin(),
                   [](std::shared_ptr<jobs::instance::Base>& inst) { return std::make_shared<BaseInstance>(inst); });

    return wrapped;
}

std::vector<Application::URL> appURL(const std::vector<Helper::URL>& in)
{
    std::vector<Application::URL> out;
    std::transform(in.begin(), in.end(), out.begin(),
                   [](Helper::URL url) { return Application::URL::from_raw(url.value()); });
    return out;
}

std::shared_ptr<Helper::Instance> Base::launch(std::vector<Helper::URL> urls)
{
    std::function<std::list<std::pair<std::string, std::string>>()> envfunc = [this]() {
        return std::list<std::pair<std::string, std::string>>{};
    };

    /* TODO */
    return std::make_shared<BaseInstance>(_registry->impl->jobs->launch(
        _appid, _type.value(), std::string{}, appURL(urls), jobs::manager::launchMode::STANDARD, envfunc));
}

std::shared_ptr<Helper::Instance> Base::launch(MirPromptSession* session, std::vector<Helper::URL> urls)
{
    std::function<std::list<std::pair<std::string, std::string>>()> envfunc = [this]() {
        return std::list<std::pair<std::string, std::string>>{};
    };

    /* TODO */
    return std::make_shared<BaseInstance>(_registry->impl->jobs->launch(
        _appid, _type.value(), std::string{}, appURL(urls), jobs::manager::launchMode::STANDARD, envfunc));
}

}  // namespace helper_impl

/***************************/
/* Helper Public Functions */
/***************************/

std::shared_ptr<Helper> Helper::create(Type type, AppID appid, std::shared_ptr<Registry> registry)
{
    /* Only one type today */
    return std::make_shared<helper_impls::Base>(type, appid, registry);
}

}  // namespace app_launch
}  // namespace ubuntu
