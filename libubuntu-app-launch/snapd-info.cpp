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

#include <curl/curl.h>

namespace ubuntu
{
namespace app_launch
{
namespace app_impls
{
namespace snapd
{

Info::Info()
{
}

std::shared_ptr<Info::AppInfo> Info::appInfo(AppID &appid)
{
    return {};
}

std::shared_ptr<JsonNode> Info::snapdJson(const std::string &endpoint)
{
    /* Setup the CURL connection and suck some data */
    CURL *curl = curl_easy_init();
    if (curl == nullptr)
    {
        throw std::runtime_error("Unable to create new cURL connection");
    }

    std::vector<char> data;

    curl_easy_setopt(curl, CURLOPT_URL, ("http:" + endpoint).c_str());
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/run/snapd.socket");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
        unsigned int i;
        std::vector<char> *data = static_cast<std::vector<char> *>(userdata);
        data->reserve(data->size() + (size * nmemb)); /* allocate once */
        for (i = 0; i < size * nmemb; i++)
        {
            data->push_back(ptr[i]);
        }
        return i;
    });

    auto res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        throw std::runtime_error("snapd HTTP server returned an error");
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
        g_warning("Can not parse! %s", error->message);
        g_error_free(error);
        throw std::runtime_error("Can not parse JSON response");
    }

    auto root = json_parser_get_root(parser.get());
    auto rootobj = json_node_get_object(root);

    if (rootobj == nullptr)
    {
        throw std::runtime_error("Root of JSON result isn't an object");
    }

    /* Check members */
    for (auto member : {"status",
                        "status-code"
                        "result",
                        "type"})
    {
        if (!json_object_has_member(rootobj, member))
        {
            throw std::runtime_error("Resulting JSON didn't have a '" + std::string(member) + "'");
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

    auto result = std::shared_ptr<JsonNode>((JsonNode *)g_object_ref(json_object_get_member(rootobj, "result")),
                                            [](JsonNode *node) { g_clear_object(&node); });

    return result;
}

std::vector<AppID> Info::appsForInterface(const std::string &interface)
{
    try
    {
        auto interfaces = snapdJson("/v2/interfaces");
    }
    catch (std::runtime_error &e)
    {
        g_warning("Unable to get interface information: %s", e.what());
        return {};
    }

    return {};
}

}  // namespace snapd
}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
