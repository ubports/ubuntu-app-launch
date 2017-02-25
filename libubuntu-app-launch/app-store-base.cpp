/*
 * Copyright © 2017 Canonical Ltd.
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

#include "app-store-base.h"
#include "app-store-legacy.h"
#include "app-store-libertine.h"

#if ENABLE_SNAPPY
#include "app-store-snap.h"
#endif

namespace ubuntu
{
namespace app_launch
{
namespace app_store
{

Base::Base()
    : info_watcher::Base({})
{
}

Base::~Base()
{
}

std::list<std::shared_ptr<Base>> Base::allAppStores()
{
    return
    {
            std::make_shared<Legacy>() /* Legacy */
            ,
            std::make_shared<Libertine>() /* Libertine */
#if ENABLE_SNAPPY
            ,
            std::make_shared<Snap>() /* Snappy */
#endif
    };
}

}  // namespace app_store
}  // namespace app_launch
}  // namespace ubuntu
