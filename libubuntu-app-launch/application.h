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
#include <memory>
#include <sys/types.h>
#include <vector>

#include "appid.h"
#include "oom.h"
#include "type-tagger.h"

#pragma once
#pragma GCC visibility push(default)

namespace ubuntu
{
namespace app_launch
{

class Registry;

/** \brief Class to represent an application, whether running or not, and
        query more information about it.

    Generally the Application object represents an Application in the system. It
    hooks up all of it's signals, finds out information about it and controls
    whether it is running or not. This class is what most users of Ubuntu App
    Launch will do the majority of their work.
*/
class Application
{
public:
    /** \private */
    struct URLTag;
    /** \private */
    typedef TypeTagger<URLTag, std::string> URL;

    /** Function to create an Application object. It determines the type of application
        and returns a pointer to that application object. It uses the registry for
        shared connections and is given an AppID. To find the AppID for a given
        application use the AppID::discover() functions.

        \param appid Application ID for the application
        \param registry Shared registry to use
    */
    static std::shared_ptr<Application> create(const AppID& appid, const std::shared_ptr<Registry>& registry);

    virtual ~Application() = default;

    /* System level info */
    /** Get the Application ID of this Application */
    virtual AppID appId() = 0;

    /** \brief Information and metadata about the application for programs that
            are displaying the application to users

        The Info class has all the metadata including user visible strings
        and other nicities that users expect to see about applications. For
        most formats this is gotten from the Desktop file, but those may
        be in different locations depending on the packaging format.
    */
    class Info
    {
    public:
        /* Basic information */
        /** \private */
        struct NameTag;
        /** \private */
        struct DescriptionTag;
        /** \private */
        struct IconPathTag;

        /** \private */
        typedef TypeTagger<NameTag, std::string> Name;
        /** \private */
        typedef TypeTagger<DescriptionTag, std::string> Description;
        /** \private */
        typedef TypeTagger<IconPathTag, std::string> IconPath;

        virtual ~Info() = default;

        /** Name of the application */
        virtual const Name& name() = 0;
        /** Textual description of the application */
        virtual const Description& description() = 0;
        /** Path to the icon that represents the application */
        virtual const IconPath& iconPath() = 0;

        /** Information to be shown on the app splash screen */
        struct Splash
        {
            /** \private */
            struct TitleTag;
            /** \private */
            struct ImageTag;
            /** \private */
            struct ColorTag;
            /** \private */
            struct ShowHeaderTag;

            /** \private */
            typedef TypeTagger<TitleTag, std::string> Title;
            /** \private */
            typedef TypeTagger<ImageTag, std::string> Image;
            /** \private */
            typedef TypeTagger<ColorTag, std::string> Color;
            /** \private */
            typedef TypeTagger<ShowHeaderTag, bool> ShowHeader;

            /** Title text on the screen */
            Title title;
            /** Image to put on the screen */
            Image image;
            /** Color of the background */
            Color backgroundColor;
            /** Color of the header (if shown) */
            Color headerColor;
            /** Color of the footer */
            Color footerColor;
            /** Whether the standard UI Toolkit header should be shown */
            ShowHeader showHeader;
        };

        /** Get information for the splash screen */
        virtual Splash splash() = 0;

        /** Orientation and placement */
        struct Orientations
        {
            /** Can support portrait */
            bool portrait;
            /** Can support landscape */
            bool landscape;
            /** Can support inverted portrait */
            bool invertedPortrait;
            /** Can support inverted landscape */
            bool invertedLandscape;

            /** Check to see if two Orientations are the same */
            bool operator==(const Orientations& b) const
            {
                return portrait == b.portrait && landscape == b.landscape && invertedPortrait == b.invertedPortrait &&
                       invertedLandscape == b.invertedLandscape;
            }
        };

        /** \private */
        struct RotatesWindowTag;

        /** \private */
        typedef TypeTagger<RotatesWindowTag, bool> RotatesWindow;

        /** Return which orientations are supported */
        virtual Orientations supportedOrientations() = 0;
        /** Return whether the window contents can be rotated or not */
        virtual RotatesWindow rotatesWindowContents() = 0;

        /* Lifecycle */
        /** \private */
        struct UbuntuLifecycleTag;

        /** \private */
        typedef TypeTagger<UbuntuLifecycleTag, bool> UbuntuLifecycle;

        /* Return whether the Ubuntu Lifecycle is supported by this application */
        virtual UbuntuLifecycle supportsUbuntuLifecycle() = 0;
    };

    /** Get a Application::Info object to describe the metadata for this
        application */
    virtual std::shared_ptr<Info> info() = 0;

    /** Interface representing the information about a specific application
        running instance. This includes information on the PIDs that make
        up the Application::Instance. */
    class Instance
    {
    public:
        virtual ~Instance() = default;

        /* Query lifecycle */
        /** Check to see if the instance is currently running. The object can
            exist even after the instance has stopped running. */
        virtual bool isRunning() = 0;

        /* Instance Info */
        /** Path to the log file for stdout/stderr for this instance of the
            application. */
        virtual std::string logPath() = 0;

        /* PIDs */
        /** Get the primary PID for this Application::Instance, this will return
            zero when it is not running. The primary PID is the PID keeping the
            instance alive, when it exists the others get reaped. */
        virtual pid_t primaryPid() = 0;
        /** Check to see if a PID is in the cgroup for this application instance.
            Each application instance tracks all the PIDs that are currently being
            used */
        virtual bool hasPid(pid_t pid) = 0;
        /** Check to see if a specific PID is part of this Application::Instance */
        virtual std::vector<pid_t> pids() = 0;

        /* OOM Adjustment */
        /** Sets the value of the OOM Adjust kernel property for the all of
            the processes this instance. */
        virtual void setOomAdjustment(const oom::Score score) = 0;
        /** Gets the value of the OOM Adjust kernel property for the primary process
            of this instance.

            \note This function does not check all the processes and ensure they are
                  consistent, it just checks the primary and assumes that.
        */
        virtual const oom::Score getOomAdjustment() = 0;

        /* Manage lifecycle */
        /** Pause, or send SIGSTOP, to the PIDs in this Application::Instance */
        virtual void pause() = 0;
        /** Resume, or send SIGCONT, to the PIDs in this Application::Instance */
        virtual void resume() = 0;
        /** Stop, or send SIGTERM, to the PIDs in this Application::Instance, if
            the PIDs do not respond to the SIGTERM they will be SIGKILL'd */
        virtual void stop() = 0;
    };

    /** A quick check to see if this application has any running instances */
    virtual bool hasInstances() = 0;
    /** Get a vector of the running instances of this application */
    virtual std::vector<std::shared_ptr<Instance>> instances() = 0;

    /** Start an application, optionally with URLs to pass to it.

        \param urls A list of URLs to pass to the application command line
    */
    virtual std::shared_ptr<Instance> launch(const std::vector<URL>& urls = {}) = 0;
    /** Start an application with text flags, optionally with URLs to
        pass to it.

        \param urls A list of URLs to pass to the application command line
    */
    virtual std::shared_ptr<Instance> launchTest(const std::vector<URL>& urls = {}) = 0;
};

}  // namespace app_launch
}  // namespace ubuntu

#pragma GCC visibility pop
