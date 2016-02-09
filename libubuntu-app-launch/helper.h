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

#include <vector>
#include <memory>

#include <mir_toolkit/mir_prompt_session.h>

#include "appid.h"
#include "type-tagger.h"

#pragma once
#pragma GCC visibility push(default)

namespace ubuntu
{
namespace app_launch
{

class Registry;

class Helper
{
public:
    struct TypeTag;
    struct URLTag;

    typedef TypeTagger<TypeTag, std::string> Type;
    typedef TypeTagger<URLTag, std::string> URL;

    static std::shared_ptr<Helper> create(Type type, AppID appid, std::shared_ptr<Registry> registry);

    virtual AppID appId() = 0;

    class Instance
    {
    public:
        /* Query lifecycle */
        virtual bool isRunning() = 0;

        /* Manage lifecycle */
        virtual void stop() = 0;
    };

    virtual bool hasInstances() = 0;
    virtual std::vector<std::shared_ptr<Instance>> instances() = 0;

    virtual std::shared_ptr<Instance> launch(std::vector<URL> urls = {}) = 0;
    virtual std::shared_ptr<Instance> launch(MirPromptSession* session, std::vector<URL> urls = {}) = 0;
};

};  // namespace app_launch
};  // namespace ubuntu

#pragma GCC visibility pop
