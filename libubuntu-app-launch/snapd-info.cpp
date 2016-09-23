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

#include "snapd-info.h"

#include "registry-impl.h"

#include <curl/curl.h>
#include <vector>

namespace ubuntu
{
namespace app_launch
{
namespace snapd
{

/** Initializes the info object which mostly means checking what is overridden
    by environment variables (mostly for testing) and making sure there is a
    snapd socket available to us. */
Info::Info()
{
    auto snapdEnv = g_getenv("UBUNTU_APP_LAUNCH_SNAPD_SOCKET");
    if (G_UNLIKELY(snapdEnv != nullptr))
    {
        snapdSocket = snapdEnv;
    }
    else
    {
        snapdSocket = "/run/snapd.socket";
    }

    auto snapcBasedir = g_getenv("UBUNTU_APP_LAUNCH_SNAP_BASEDIR");
    if (G_UNLIKELY(snapcBasedir != nullptr))
    {
        snapBasedir = snapcBasedir;
    }
    else
    {
        snapBasedir = "/snap";
    }

    if (g_file_test(snapdSocket.c_str(), G_FILE_TEST_EXISTS))
    {
        snapdExists = true;
    }
}

/** Gets package information out of snapd by using the REST
    interface and turning the JSON object into a C++ Struct

    \param package Name of the package to look for
*/
std::shared_ptr<Info::PkgInfo> Info::pkgInfo(const AppID::Package &package) const
{
    if (!snapdExists)
    {
        return {};
    }

    try
    {
        auto snapnode = snapdJson("/v2/snaps/" + package.value());
        auto snapobject = json_node_get_object(snapnode.get());
        if (snapobject == nullptr)
        {
            throw std::runtime_error("Results returned by snapd were not a valid JSON object");
        }

        /******************************************/
        /* Validation of the object we got        */
        /******************************************/
        for (const auto &member : {"apps"})
        {
            if (!json_object_has_member(snapobject, member))
            {
                throw std::runtime_error("Snap JSON didn't have a '" + std::string(member) + "'");
            }
        }

        for (const auto &member : {"name", "status", "revision", "type", "version"})
        {
            if (!json_object_has_member(snapobject, member))
            {
                throw std::runtime_error("Snap JSON didn't have a '" + std::string(member) + "'");
            }

            auto node = json_object_get_member(snapobject, member);
            if (json_node_get_node_type(node) != JSON_NODE_VALUE)
            {
                throw std::runtime_error{"Snap JSON had a '" + std::string(member) + "' but it's an object!"};
            }

            if (json_node_get_value_type(node) != G_TYPE_STRING)
            {
                throw std::runtime_error{"Snap JSON had a '" + std::string(member) + "' but it's not a string!"};
            }
        }

        std::string namestr = json_object_get_string_member(snapobject, "name");
        if (namestr != package.value())
        {
            throw std::runtime_error("Snapd returned information for snap '" + namestr + "' when we asked for '" +
                                     package.value() + "'");
        }

        std::string statusstr = json_object_get_string_member(snapobject, "status");
        if (statusstr != "active")
        {
            throw std::runtime_error("Snap is not in the 'active' state.");
        }

        std::string typestr = json_object_get_string_member(snapobject, "type");
        if (typestr != "app")
        {
            throw std::runtime_error("Specified snap is not an application, we only support applications");
        }

        /******************************************/
        /* Validation complete — build the object */
        /******************************************/

        auto pkgstruct = std::make_shared<PkgInfo>();
        pkgstruct->name = namestr;
        pkgstruct->version = json_object_get_string_member(snapobject, "version");
        std::string revisionstr = json_object_get_string_member(snapobject, "revision");
        pkgstruct->revision = revisionstr;

        /* TODO: Seems like snapd should give this to us */
        auto gdir = g_build_filename(snapBasedir.c_str(), namestr.c_str(), revisionstr.c_str(), nullptr);
        pkgstruct->directory = gdir;
        g_free(gdir);

        auto appsarray = json_object_get_array_member(snapobject, "apps");
        for (unsigned int i = 0; i < json_array_get_length(appsarray); i++)
        {
            auto appobj = json_array_get_object_element(appsarray, i);
            if (json_object_has_member(appobj, "name"))
            {
                auto appname = json_object_get_string_member(appobj, "name");
                if (appname)
                {
                    pkgstruct->appnames.insert(appname);
                }
            }
        }

        return pkgstruct;
    }
    catch (std::runtime_error &e)
    {
        g_warning("Unable to get snap information for '%s': %s", package.value().c_str(), e.what());
        return {};
    }
}

/** Function that acts as the return from cURL to add data to
    our storage vector.

    \param ptr incoming data
    \param size block size
    \param nmemb number of blocks
    \param userdata our local vector to store things in
*/
static size_t snapd_writefunc(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto data = static_cast<std::vector<char> *>(userdata);
    data->insert(data->end(), ptr, ptr + (size * nmemb));
    return size * nmemb;
}

/** Asks the snapd process for some JSON. This function parses the basic
    response JSON that snapd returns and will error if a return code error
    is in the JSON. It then passes on the "result" part of the response
    to the caller.

    \param endpoint End of the URL to pass to snapd
*/
std::shared_ptr<JsonNode> Info::snapdJson(const std::string &endpoint) const
{
    /* Setup the CURL connection and suck some data */
    CURL *curl = curl_easy_init();
    if (curl == nullptr)
    {
        throw std::runtime_error("Unable to create new cURL connection");
    }

    std::vector<char> data;

    /* Configure the command */
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, ("http://snapd" + endpoint).c_str());
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, snapdSocket.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, snapd_writefunc);

    /* Overridable timeout */
    if (g_getenv("UBUNTU_APP_LAUNCH_DISABLE_SNAPD_TIMEOUT") != nullptr)
    {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 100L);
    }

    /* Run the actual request (blocking) */
    auto res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        throw std::runtime_error("snapd HTTP server returned an error: " + std::string(curl_easy_strerror(res)));
    }
    else
    {
        g_debug("Got %d bytes from snapd", int(data.size()));
    }

    curl_easy_cleanup(curl);

    /* Cool, we have data */
    auto parser = std::shared_ptr<JsonParser>(json_parser_new(), [](JsonParser *parser) { g_clear_object(&parser); });
    GError *error = nullptr;
    json_parser_load_from_data(parser.get(), /* parser */
                               data.data(),  /* array */
                               data.size(),  /* size */
                               &error);      /* error */

    if (error != nullptr)
    {
        std::string message{"Can not parse JSON: " + std::string(error->message)};
        g_error_free(error);
        throw std::runtime_error{message};
    }

    auto root = json_parser_get_root(parser.get());
    auto rootobj = json_node_get_object(root);

    if (rootobj == nullptr)
    {
        throw std::runtime_error("Root of JSON result isn't an object");
    }

    /* Check members */
    for (const auto &member : {"status-code", "result"})
    {
        if (!json_object_has_member(rootobj, member))
        {
            throw std::runtime_error("Resulting JSON didn't have a '" + std::string(member) + "'");
        }
    }

    for (const auto &member : {"status", "type"})
    {
        if (!json_object_has_member(rootobj, member))
        {
            throw std::runtime_error("Snap JSON didn't have a '" + std::string(member) + "'");
        }

        auto node = json_object_get_member(rootobj, member);
        if (json_node_get_node_type(node) != JSON_NODE_VALUE)
        {
            throw std::runtime_error{"Snap JSON had a '" + std::string(member) + "' but it's an object!"};
        }

        if (json_node_get_value_type(node) != G_TYPE_STRING)
        {
            throw std::runtime_error{"Snap JSON had a '" + std::string(member) + "' but it's not a string!"};
        }
    }

    auto status = json_object_get_int_member(rootobj, "status-code");
    if (status != 200)
    {
        throw std::runtime_error("Status code is: " + std::to_string(status));
    }

    std::string statusstr = json_object_get_string_member(rootobj, "status");
    if (statusstr != "OK")
    {
        throw std::runtime_error("Status string is: " + statusstr);
    }

    std::string typestr = json_object_get_string_member(rootobj, "type");
    if (typestr != "sync")
    {
        throw std::runtime_error("We only support 'sync' results right now, but we got a: " + typestr);
    }

    auto result = std::shared_ptr<JsonNode>(json_node_ref(json_object_get_member(rootobj, "result")), json_node_unref);

    return result;
}

/** Looks through all the plugs in the interfaces and runs a function
    based on them. Avoids pulling objects out of the parsed JSON structure
    from Snappy and making sure they have the same lifecycle as the parser
    object which seems to destroy them when it dies.

    \param plugfunc Function to execute on each plug
*/
void Info::forAllPlugs(std::function<void(JsonObject *plugobj)> plugfunc) const
{
    if (!snapdExists)
    {
        return;
    }

    auto interfacesnode = snapdJson("/v2/interfaces");
    auto interface = json_node_get_object(interfacesnode.get());
    if (interface == nullptr)
    {
        throw std::runtime_error("Interfaces result isn't an object: " + Registry::Impl::printJson(interfacesnode));
    }

    for (const auto &member : {"plugs", "slots"})
    {
        if (!json_object_has_member(interface, member))
        {
            throw std::runtime_error("Interface JSON didn't have a '" + std::string(member) + "'");
        }
    }

    auto plugarray = json_object_get_array_member(interface, "plugs");
    for (unsigned int i = 0; i < json_array_get_length(plugarray); i++)
    {
        auto ifaceobj = json_array_get_object_element(plugarray, i);
        try
        {
            for (const auto &member : {"snap", "interface", "apps"})
            {
                if (!json_object_has_member(ifaceobj, member))
                {
                    throw std::runtime_error("Interface JSON didn't have a '" + std::string(member) + "'");
                }
            }

            plugfunc(ifaceobj);
        }
        catch (std::runtime_error &e)
        {
            /* We'll check the others even if one is bad */
            // g_debug("Malformed inteface instance: %s", e.what());
            continue;
        }
    }
}

/** Gets all the apps that are available for a given interface. It asks snapd
    for the list of interfaces and then finds this one, turning it into a set
    of AppIDs

    \param in_interface Which interface to get the set of apps for
*/
std::set<AppID> Info::appsForInterface(const std::string &in_interface) const
{
    bool interfacefound = false;
    std::set<AppID> appids;

    try
    {
        forAllPlugs([this, &interfacefound, &appids, in_interface](JsonObject *ifaceobj) {
            std::string interfacename = json_object_get_string_member(ifaceobj, "interface");
            if (interfacename != in_interface)
            {
                return;
            }

            interfacefound = true;

            auto cname = json_object_get_string_member(ifaceobj, "snap");
            if (cname == nullptr)
            {
                return;
            }
            std::string snapname(cname);

            auto pkginfo = pkgInfo(AppID::Package::from_raw(snapname));
            if (!pkginfo)
            {
                return;
            }

            std::string revision = pkginfo->revision;

            auto apps = json_object_get_array_member(ifaceobj, "apps");
            for (unsigned int k = 0; apps != nullptr && k < json_array_get_length(apps); k++)
            {
                std::string appname = json_array_get_string_element(apps, k);

                appids.emplace(AppID(AppID::Package::from_raw(snapname),   /* package */
                                     AppID::AppName::from_raw(appname),    /* appname */
                                     AppID::Version::from_raw(revision))); /* version */
            }
        });

        if (!interfacefound)
        {
            g_debug("Unable to find information on interface '%s'", in_interface.c_str());
        }
    }
    catch (std::runtime_error &e)
    {
        g_warning("Unable to get interface information: %s", e.what());
    }

    return appids;
}

/** Finds all the interfaces for a specific appid

    \param appid AppID to search for
*/
std::set<std::string> Info::interfacesForAppId(const AppID &appid) const
{

    std::set<std::string> interfaces;

    try
    {
        forAllPlugs([&interfaces, appid](JsonObject *ifaceobj) {
            auto snapname = json_object_get_string_member(ifaceobj, "snap");
            if (snapname != appid.package.value())
            {
                return;
            }

            auto cinterfacename = json_object_get_string_member(ifaceobj, "interface");
            if (cinterfacename == nullptr)
            {
                return;
            }

            std::string interfacename(cinterfacename);

            auto apps = json_object_get_array_member(ifaceobj, "apps");
            for (unsigned int k = 0; k < json_array_get_length(apps); k++)
            {
                std::string appname = json_array_get_string_element(apps, k);
                if (appname == appid.appname.value())
                {
                    interfaces.insert(interfacename);
                }
            }
        });
    }
    catch (std::runtime_error &e)
    {
        g_warning("Unable to get interface information: %s", e.what());
    }

    return interfaces;
}

}  // namespace snapd
}  // namespace app_launch
}  // namespace ubuntu
