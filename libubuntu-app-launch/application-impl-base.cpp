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

#include "application-impl-base.h"

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{

Base::Base(const std::shared_ptr<Registry>& registry)
    : _registry(registry)
{
}

bool Base::hasInstances()
{
    std::string sappid = appId();
    return ubuntu_app_launch_get_primary_pid(sappid.c_str()) != 0;
}

class BaseInstance : public Application::Instance
{
public:
    explicit BaseInstance(const std::string& appId);

    /* Query lifecycle */
    bool isRunning() override
    {
        return ubuntu_app_launch_get_primary_pid(_appId.c_str()) != 0;
    }
    pid_t primaryPid() override
    {
        return ubuntu_app_launch_get_primary_pid(_appId.c_str());
    }
    bool hasPid(pid_t pid) override
    {
        return ubuntu_app_launch_pid_in_app_id(pid, _appId.c_str()) == TRUE;
    }
    std::string logPath() override
    {
        auto cpath = ubuntu_app_launch_application_log_path(_appId.c_str());
        if (cpath != nullptr)
        {
            std::string retval(cpath);
            g_free(cpath);
            return retval;
        }
        else
        {
            return {};
        }
    }
    std::vector<pid_t> pids() override
    {
        std::vector<pid_t> vector;
        GList* list = ubuntu_app_launch_get_pids(_appId.c_str());

        for (GList* pntr = list; pntr != nullptr; pntr = g_list_next(pntr))
        {
            vector.push_back(static_cast<pid_t>(GPOINTER_TO_INT(list->data)));
        }

        g_list_free(list);

        return vector;
    }

    /* Manage lifecycle */
    void pause() override
    {
        ubuntu_app_launch_pause_application(_appId.c_str());
    }
    void resume() override
    {
        ubuntu_app_launch_resume_application(_appId.c_str());
    }
    void stop() override
    {
        ubuntu_app_launch_stop_application(_appId.c_str());
    }

private:
    std::string _appId;
};

BaseInstance::BaseInstance(const std::string& appId)
    : _appId(appId)
{
}

std::vector<std::shared_ptr<Application::Instance>> Base::instances()
{
    std::vector<std::shared_ptr<Instance>> vect;
    vect.emplace_back(std::make_shared<BaseInstance>(appId()));
    return vect;
}

std::shared_ptr<gchar*> urlsToStrv(std::vector<Application::URL> urls)
{
    auto array = g_array_new(TRUE, FALSE, sizeof(gchar*));

    for (auto url : urls)
    {
        auto str = g_strdup(url.value().c_str());
        g_array_append_val(array, str);
    }

    return std::shared_ptr<gchar*>((gchar**)g_array_free(array, FALSE), g_strfreev);
}

std::shared_ptr<Application::Instance> Base::launch(const std::vector<Application::URL>& urls)
{
    std::string appIdStr = appId();
    std::shared_ptr<gchar*> urlstrv;

    if (urls.size() > 0)
    {
        urlstrv = urlsToStrv(urls);
    }

    ubuntu_app_launch_start_application(appIdStr.c_str(), urlstrv.get());

    return std::make_shared<BaseInstance>(appIdStr);
}

std::shared_ptr<Application::Instance> Base::launchTest(const std::vector<Application::URL>& urls)
{
    std::string appIdStr = appId();
    std::shared_ptr<gchar*> urlstrv;

    if (urls.size() > 0)
    {
        urlstrv = urlsToStrv(urls);
    }

    ubuntu_app_launch_start_application_test(appIdStr.c_str(), urlstrv.get());

    return std::make_shared<BaseInstance>(appIdStr);
}

}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
