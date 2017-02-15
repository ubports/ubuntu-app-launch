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
#include "registry-impl.h"

#include "ubuntu-app-launch.h"

namespace ubuntu
{
namespace app_launch
{
namespace helper_impls
{

class Click : public Helper
{
public:
    Click(const Helper::Type& type, const AppID& appid, const std::shared_ptr<Registry>& registry)
        : _type(type)
        , _appid(appid)
        , _registry(registry)
    {
    }

    AppID appId() override
    {
        return _appid;
    }

    bool hasInstances() override;
    std::vector<std::shared_ptr<Helper::Instance>> instances() override;

    std::shared_ptr<Helper::Instance> launch(std::vector<Helper::URL> urls = {}) override;
    std::shared_ptr<Helper::Instance> launch(MirPromptSession* session, std::vector<Helper::URL> urls = {}) override;

    static std::list<std::shared_ptr<Helper>> running(Helper::Type type, std::shared_ptr<Registry> registry);

private:
    Helper::Type _type;
    AppID _appid;
    std::shared_ptr<Registry> _registry;
};

bool Click::hasInstances()
{
    return _registry->impl->thread.executeOnThread<bool>([this]() {
        auto instances = ubuntu_app_launch_list_helper_instances(_type.value().c_str(), ((std::string)_appid).c_str());
        auto retval = (g_strv_length(instances) != 0);

        g_strfreev(instances);

        return retval;
    });
}

class ClickInstance : public Helper::Instance
{
public: /* all one file, no one to hide from */
    AppID _appid;
    Helper::Type _type;
    std::string _instanceid;
    std::shared_ptr<Registry> _registry;

    ClickInstance(const AppID& appid,
                  const Helper::Type& type,
                  const std::string& instanceid,
                  std::shared_ptr<Registry> registry)
        : _appid(appid)
        , _type(type)
        , _instanceid(instanceid)
        , _registry(registry)
    {
    }

    bool isRunning() override
    {
        return _registry->impl->thread.executeOnThread<bool>([this]() {
            bool found = false;

            auto instances =
                ubuntu_app_launch_list_helper_instances(_type.value().c_str(), ((std::string)_appid).c_str());
            for (int i = 0; instances[i] != nullptr; i++)
            {
                if (_instanceid == std::string(instances[i]))
                {
                    found = true;
                    break;
                }
            }

            g_strfreev(instances);

            return found;
        });
    }

    void stop() override
    {
        _registry->impl->thread.executeOnThread<bool>([this]() {
            return ubuntu_app_launch_stop_multiple_helper(_type.value().c_str(), ((std::string)_appid).c_str(),
                                                          _instanceid.c_str()) == TRUE;
        });
    }
};

std::vector<std::shared_ptr<Click::Instance>> Click::instances()
{
    return _registry->impl->thread.executeOnThread<std::vector<std::shared_ptr<Click::Instance>>>(
        [this]() -> std::vector<std::shared_ptr<Click::Instance>> {
            std::vector<std::shared_ptr<Click::Instance>> vect;
            auto instances =
                ubuntu_app_launch_list_helper_instances(_type.value().c_str(), ((std::string)_appid).c_str());
            for (int i = 0; instances[i] != nullptr; i++)
            {
                auto inst = std::make_shared<ClickInstance>(_appid, _type, instances[i], _registry);
                vect.push_back(inst);
            }

            g_strfreev(instances);

            return vect;
        });
}

std::shared_ptr<gchar*> urlsToStrv(std::vector<Helper::URL> urls)
{
    if (urls.size() == 0)
    {
        return {};
    }

    auto array = g_array_new(TRUE, FALSE, sizeof(gchar*));

    for (auto url : urls)
    {
        auto str = g_strdup(url.value().c_str());
        g_array_append_val(array, str);
    }

    return std::shared_ptr<gchar*>((gchar**)g_array_free(array, FALSE), g_strfreev);
}

std::shared_ptr<Click::Instance> Click::launch(std::vector<Helper::URL> urls)
{
    auto urlstrv = urlsToStrv(urls);

    return _registry->impl->thread.executeOnThread<std::shared_ptr<Click::Instance>>([this, urlstrv]() {
        auto instanceid = ubuntu_app_launch_start_multiple_helper(_type.value().c_str(), ((std::string)_appid).c_str(),
                                                                  urlstrv.get());
        auto ret = std::make_shared<ClickInstance>(_appid, _type, instanceid, _registry);
        g_free(instanceid);
        return ret;
    });
}

std::shared_ptr<Click::Instance> Click::launch(MirPromptSession* session, std::vector<Helper::URL> urls)
{
    auto urlstrv = urlsToStrv(urls);

    return _registry->impl->thread.executeOnThread<std::shared_ptr<Click::Instance>>([this, session, urlstrv]() {
        auto instanceid = ubuntu_app_launch_start_session_helper(_type.value().c_str(), session,
                                                                 ((std::string)_appid).c_str(), urlstrv.get());

        return std::make_shared<ClickInstance>(_appid, _type, instanceid, _registry);
    });
}

std::list<std::shared_ptr<Helper>> Click::running(Helper::Type type, std::shared_ptr<Registry> registry)
{
    return registry->impl->thread.executeOnThread<std::list<std::shared_ptr<Helper>>>([type, registry]() {
        std::list<std::shared_ptr<Helper>> helpers;

        auto appidv = ubuntu_app_launch_list_helpers(type.value().c_str());
        for (int i = 0; appidv[i] != nullptr; i++)
        {
            auto helper = std::make_shared<Click>(type, AppID::parse(appidv[i]), registry);
            helpers.push_back(helper);
        }

        g_strfreev(appidv);

        return helpers;
    });
}

}  // namespace helper_impl

/***************************/
/* Helper Public Functions */
/***************************/

std::shared_ptr<Helper> Helper::create(Type type, AppID appid, std::shared_ptr<Registry> registry)
{
    /* Only one type today */
    return std::make_shared<helper_impls::Click>(type, appid, registry);
}

}  // namespace app_launch
}  // namespace ubuntu
