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

std::vector<AppID> Info::appsForInterface(const std::string &interface)
{
    CURL *curl = curl_easy_init();
    if (curl == nullptr)
    {
        return {};
    }

    curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");
    auto res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        return {};
    }

    curl_easy_cleanup(curl);

    return {};
}

}  // namespace snapd
}  // namespace app_impls
}  // namespace app_launch
}  // namespace ubuntu
