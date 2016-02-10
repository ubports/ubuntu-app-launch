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

#include <sys/types.h>
#include <vector>
#include <memory>
#include <list>

#include "appid.h"
#include "type-tagger.h"

#pragma once
#pragma GCC visibility push(default)

namespace ubuntu
{
namespace app_launch
{

class Registry;

class Application
{
public:
    struct URLTag;
    typedef TypeTagger<URLTag, std::string> URL;

    static std::shared_ptr<Application> create(const AppID& appid, const std::shared_ptr<Registry>& registry);

    /* System level info */
    virtual AppID appId() = 0;

    class Info
    {
    public:
        /* Basic information */
        struct NameTag;
        struct DescriptionTag;
        struct IconPathTag;

        typedef TypeTagger<NameTag, std::string> Name;
        typedef TypeTagger<DescriptionTag, std::string> Description;
        typedef TypeTagger<IconPathTag, std::string> IconPath;

        virtual const Name& name() = 0;
        virtual const Description& description() = 0;
        virtual const IconPath& iconPath() = 0;

        /* Splash information */
        struct Splash
        {
            struct TitleTag;
            struct ImageTag;
            struct ColorTag;
            struct ShowHeaderTag;

            typedef TypeTagger<TitleTag, std::string> Title;
            typedef TypeTagger<ImageTag, std::string> Image;
            typedef TypeTagger<ColorTag, std::string> Color;
            typedef TypeTagger<ShowHeaderTag, bool> ShowHeader;

            Title title;
            Image image;
            Color backgroundColor;
            Color headerColor;
            Color footerColor;
            ShowHeader showHeader;
        };

        virtual Splash splash() = 0;

        /* Orientation and placement */
        struct Orientations
        {
            bool portrait;
            bool landscape;
            bool invertedPortrait;
            bool invertedLandscape;

            bool operator==(const Orientations& b) const
            {
                return portrait == b.portrait && landscape == b.landscape && invertedPortrait == b.invertedPortrait &&
                       invertedLandscape == b.invertedLandscape;
            }
        };

        struct RotatesWindowTag;

        typedef TypeTagger<RotatesWindowTag, bool> RotatesWindow;

        virtual Orientations supportedOrientations() = 0;
        virtual RotatesWindow rotatesWindowContents() = 0;

        /* Lifecycle */
        struct UbuntuLifecycleTag;

        typedef TypeTagger<UbuntuLifecycleTag, bool> UbuntuLifecycle;

        virtual UbuntuLifecycle ubuntuLifecycle() = 0;
    };

    virtual std::shared_ptr<Info> info() = 0;

    class Instance
    {
    public:
        /* Query lifecycle */
        virtual bool isRunning() = 0;

        /* Instance Info */
        virtual std::string logPath() = 0;

        /* PIDs */
        virtual pid_t primaryPid() = 0;
        virtual bool hasPid(pid_t pid) = 0;
        virtual std::vector<pid_t> pids() = 0;

        /* Manage lifecycle */
        virtual void pause() = 0;
        virtual void resume() = 0;
        virtual void stop() = 0;
    };

    virtual bool hasInstances() = 0;
    virtual std::vector<std::shared_ptr<Instance>> instances() = 0;

    virtual std::shared_ptr<Instance> launch(const std::vector<URL>& urls = {}) = 0;
    virtual std::shared_ptr<Instance> launchTest(const std::vector<URL>& urls = {}) = 0;
};

};  // namespace app_launch
};  // namespace ubuntu

#pragma GCC visibility pop
