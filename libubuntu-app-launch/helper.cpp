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
#include <numeric>

#include "helper-impl.h"
#include "registry-impl.h"

#include "ubuntu-app-launch.h"

extern "C" {
#include "proxy-socket-demangler.h"
#include <gio/gunixfdlist.h>
}

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

/** Find an instance that we already know the ID of */
std::shared_ptr<Helper::Instance> Base::existingInstance(const std::string& instanceid)
{
    auto appinst = _registry->impl->jobs->existing(_appid, _type.value(), instanceid, {});

    return std::make_shared<BaseInstance>(appinst);
}

std::string genInstanceId()
{
    return std::to_string(g_get_real_time());
}

std::vector<Application::URL> appURL(const std::vector<Helper::URL>& in)
{
    std::vector<Application::URL> out;
    std::transform(in.begin(), in.end(), out.begin(),
                   [](Helper::URL url) { return Application::URL::from_raw(url.value()); });
    return out;
}

/** Sets up the executable environment variable based on the appid and
 *  the type of helper. We look for the exec-tool, but if we can't find
 *  it we're cool with that and we just execute the helper. If we do find
 *  an exec-tool we'll use that to fill in the parameters. For legacy appid's
 *  we'll allow the exec-tool to set everything. */
std::list<std::pair<std::string, std::string>> Base::defaultEnv()
{
    std::list<std::pair<std::string, std::string>> envs{};
    auto csnapenv = getenv("SNAP");
    std::string helperpath;
    if (csnapenv != nullptr)
    {
        helperpath = std::string{csnapenv} + "/" HELPER_EXEC_TOOL_DIR "/" + _type.value() + "/exec-tool";
    }
    else
    {
        helperpath = HELPER_EXEC_TOOL_DIR "/" + _type.value() + "/exec-tool";
    }

    std::list<std::string> exec;
    /* We have an exec tool that'll give us params */
    if (g_file_test(helperpath.c_str(), G_FILE_TEST_IS_EXECUTABLE))
    {
        exec.push_back("helper-helper"); /* TODO */
        exec.push_back(helperpath);
    }
    else
    {
        if (_appid.package.value().empty())
        {
            throw std::runtime_error{
                "Executing a helper that isn't package, but doesn't have an exec-tool. We can't do that. Sorry. Bad "
                "things will happen."};
        }
    }

    /* This is kinda hard coded for snaps right now, we don't have
     * another posibility today other than really custom stuff. But
     * if we do, we'll need to abstract this. */
    /* Insert package executable */
    if (!_appid.package.value().empty())
    {
        std::string snapdir{"/snap/bin/"};

        if (_appid.package.value() == _appid.appname.value())
        {
            exec.push_back(snapdir + _appid.package.value());
        }
        else
        {
            exec.push_back(snapdir + _appid.package.value() + "." + _appid.appname.value());
        }
    }

    exec.push_back("%U");

    envs.emplace_back(
        std::make_pair("APP_EXEC", std::accumulate(exec.begin(), exec.end(), std::string{},
                                                   [](const std::string& accum, const std::string& addon) {
                                                       return accum.empty() ? addon : accum + " " + addon;
                                                   })));

    return envs;
}

std::shared_ptr<Helper::Instance> Base::launch(std::vector<Helper::URL> urls)
{
    auto defaultenv = defaultEnv();
    std::function<std::list<std::pair<std::string, std::string>>()> envfunc = [defaultenv]() { return defaultenv; };

    return std::make_shared<BaseInstance>(_registry->impl->jobs->launch(
        _appid, _type.value(), genInstanceId(), appURL(urls), jobs::manager::launchMode::STANDARD, envfunc));
}

class MirFDProxy
{
public:
    int mirfd;
    std::shared_ptr<proxySocketDemangler> skel;
    guint handle;
    std::string path;
    std::string name;

    MirFDProxy(MirPromptSession* session, const AppID& appid, const std::shared_ptr<Registry>& reg)
        : name(g_dbus_connection_get_unique_name(reg->impl->_dbus.get()))
    {
        /* Get the Mir FD */
        std::promise<int> promise;
        mir_prompt_session_new_fds_for_prompt_providers(
            session, 1,
            [](MirPromptSession* session, size_t count, int const* fdin, void* user_data) {
                auto promise = static_cast<std::promise<int>*>(user_data);

                if (count != 1)
                {
                    g_warning("Mir trusted session returned %d FDs instead of one", (int)count);
                    promise->set_value(0);
                    return;
                }

                promise->set_value(fdin[0]);
            },
            &promise);

        mirfd = promise.get_future().get();

        if (mirfd == 0)
        {
            throw std::runtime_error{"Unable to Mir FD from Prompt Session"};
        }

        /* Setup the DBus interface */
        std::tie(skel, handle, path) =
            reg->impl->thread.executeOnThread<std::tuple<std::shared_ptr<proxySocketDemangler>, guint, std::string>>(
                [this, appid, reg]() {
                    auto skel = std::shared_ptr<proxySocketDemangler>(
                        proxy_socket_demangler_skeleton_new(),
                        [](proxySocketDemangler* skel) { g_clear_object(&skel); });
                    auto handle = g_signal_connect(G_OBJECT(skel.get()), "handle-get-mir-socket",
                                                   G_CALLBACK(staticProxyCb), this);

                    /* Find a path to export on */
                    auto dbusAppid = dbusSafe(std::string{appid});
                    std::string path;

                    while (path.empty())
                    {
                        GError* error = nullptr;
                        std::string tryname =
                            "/com/canonical/UbuntuAppLaunch/" + dbusAppid + "/" + std::to_string(rand());

                        g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skel.get()), reg->impl->_dbus.get(),
                                                         tryname.c_str(), &error);

                        if (error == nullptr)
                        {
                            path = tryname;
                        }
                        else
                        {
                            if (!g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_OBJECT_PATH_IN_USE))
                            {
                                std::string message =
                                    "Unable to export Mir trusted proxy: " + std::string{error->message};
                                g_clear_error(&error);
                                throw std::runtime_error{message};
                            }
                            g_clear_error(&error);
                        }
                    }

                    return std::make_tuple(skel, handle, path);
                });
    }

    ~MirFDProxy()
    {
        if (mirfd != 0)
        {
            close(mirfd);
        }

        if (handle != 0)
        {
            g_signal_handler_disconnect(skel.get(), handle);
        }

        g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(skel.get()));
    }

    static std::string dbusSafe(const std::string& in)
    {
        std::string out = in;
        std::transform(out.begin(), out.end(), out.begin(), [](char in) { return std::isalpha(in) ? in : '_'; });
        return out;
    }

    bool proxyCb(GDBusMethodInvocation* invocation)
    {
        if (mirfd == 0)
        {
            g_warning("Mir FD proxy called with no FDs!");
            return false;
        }

        /* Index into fds */
        auto handle = g_variant_new_handle(0);
        auto tuple = g_variant_new_tuple(&handle, 1);

        GError* error = nullptr;
        GUnixFDList* list = g_unix_fd_list_new();
        g_unix_fd_list_append(list, mirfd, &error);

        if (error == nullptr)
        {
            g_dbus_method_invocation_return_value_with_unix_fd_list(invocation, tuple, list);
        }
        else
        {
            g_variant_ref_sink(tuple);
            g_variant_unref(tuple);
        }

        g_object_unref(list);

        if (error != nullptr)
        {
            g_warning("Unable to pass FD %d: %s", mirfd, error->message);
            g_error_free(error);
            return false;
        }

        mirfd = 0;
        return true;
    }

    static gboolean staticProxyCb(GObject* obj, GDBusMethodInvocation* invocation, gpointer user_data)
    {
        return static_cast<MirFDProxy*>(user_data)->proxyCb(invocation) ? TRUE : FALSE;
    }

    std::string getPath()
    {
        return path;
    }

    std::string getName()
    {
        return name;
    }
};

std::shared_ptr<Helper::Instance> Base::launch(MirPromptSession* session, std::vector<Helper::URL> urls)
{
    std::shared_ptr<MirFDProxy> proxy;
    try
    {
        proxy = std::make_shared<MirFDProxy>(session, _appid, _registry);
    }
    catch (std::runtime_error& e)
    {
        g_warning("Error setting up Mir FD Proxy: %s", e.what());
        return {};
    }

    auto defaultenvs = defaultEnv();
    std::function<std::list<std::pair<std::string, std::string>>()> envfunc = [defaultenvs, proxy]() {
        auto envs = defaultenvs;

        envs.emplace_back(std::make_pair("UBUNTU_APP_LAUNCH_DEMANGLE_PATH", proxy->getPath()));
        envs.emplace_back(std::make_pair("UBUNTU_APP_LAUNCH_DEMANGLE_NAME", proxy->getName()));

        return envs;
    };

    /* This will maintain a reference to the proxy for two
       seconds. And then it'll be dropped. */
    _registry->impl->thread.timeout(std::chrono::seconds{2}, [proxy]() { g_debug("Mir Proxy Timeout"); });

    return std::make_shared<BaseInstance>(_registry->impl->jobs->launch(
        _appid, _type.value(), genInstanceId(), appURL(urls), jobs::manager::launchMode::STANDARD, envfunc));
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

/* Hardcore socket stuff */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

void Helper::setExec(std::vector<std::string> exec)
{
    auto cenv = getenv("UBUNTU_APP_LAUNCH_HELPER_EXECTOOL_SETEXEC_SOCKET");
    if (cenv == nullptr)
    {
        throw std::runtime_error{"Unable to find a socket to write exec information to."};
    }

    class SmartSocket
    {
    public:
        int fd;
        SmartSocket()
            : fd(socket(AF_UNIX, SOCK_STREAM, 0))
        {
        }
        ~SmartSocket()
        {
            close(fd);
        }
    };

    SmartSocket sock;
    if (sock.fd <= 0)
    {
        throw std::runtime_error{"Unable to create socket to systemd-helper-helper"};
    }

    struct sockaddr_un socketaddr = {0};
    socketaddr.sun_family = AF_UNIX;
    strncpy(socketaddr.sun_path, cenv, sizeof(socketaddr.sun_path) - 1);
    socketaddr.sun_path[0] = 0;

    if (connect(sock.fd, (const struct sockaddr*)&socketaddr, sizeof(struct sockaddr_un)) < 0)
    {
        throw std::runtime_error{"Unable to connecto to socket of systemd-helper-helper"};
    }

    for (const auto& item : exec)
    {
        auto citem = item.c_str();
        int writesize = write(sock.fd, citem, strlen(citem) + 1);

        if (writesize <= 0)
        {
            throw std::runtime_error{"Error writing to systemd-helper-helper socket"};
        }
    }
}

}  // namespace app_launch
}  // namespace ubuntu
