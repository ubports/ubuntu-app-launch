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
#include "snapd-mock.h"
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtest/gtest.h>

class SnapdInfo : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        g_setenv("UBUNTU_APP_LAUNCH_SNAPD_SOCKET", SNAPD_TEST_SOCKET, TRUE);
    }

    virtual void TearDown()
    {
        g_unlink(SNAPD_TEST_SOCKET);
    }
};

TEST_F(SnapdInfo, Init)
{
    auto info = std::make_shared<ubuntu::app_launch::snapd::Info>();

    info.reset();
}

TEST_F(SnapdInfo, PackageInfo)
{
    SnapdMock mock{SNAPD_TEST_SOCKET,
                   {{"GET /v2/snaps/test-package HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
                     SnapdMock::httpJsonResponse(SnapdMock::snapdOkay(SnapdMock::packageJson(
                         "test-package", "active", "app", "1.2.3.4", "x123", {"foo", "bar"})))}}};
    auto info = std::make_shared<ubuntu::app_launch::snapd::Info>();

    auto pkginfo = info->pkgInfo(ubuntu::app_launch::AppID::Package::from_raw("test-package"));

    mock.result();

    ASSERT_NE(nullptr, pkginfo);
    EXPECT_EQ("test-package", pkginfo->name);
    EXPECT_EQ("1.2.3.4", pkginfo->version);
    EXPECT_EQ("x123", pkginfo->revision);
    EXPECT_EQ("/snap/test-package/x123", pkginfo->directory);
    EXPECT_NE(pkginfo->appnames.end(), pkginfo->appnames.find("foo"));
    EXPECT_NE(pkginfo->appnames.end(), pkginfo->appnames.find("bar"));
}

#if 0
TEST_F(SnapdInfo, AppsForInterface)
{
    SnapdMock mock{
        SNAPD_TEST_SOCKET,
        {{"GET /v2/interfaces HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
          SnapdMock::httpJsonResponse("{ 'status': 'OK', 'status-code': 200, 'type': 'sync', 'result': { 'name': "
                                      "'test-package', 'status': 'active', 'type': 'app', 'version': '1.2.3.4', "
                                      "'revision': 'x123', 'apps': [ { 'name': 'foo' }, {'name': 'bar'} ] } }")}}};
        {{"GET /v2/snaps/test-package HTTP/1.1\r\nHost: http\r\nAccept: */*\r\n\r\n",
          SnapdMock::httpJsonResponse("{ 'status': 'OK', 'status-code': 200, 'type': 'sync', 'result': { 'name': "
                                      "'test-package', 'status': 'active', 'type': 'app', 'version': '1.2.3.4', "
                                      "'revision': 'x123', 'apps': [ { 'name': 'foo' }, {'name': 'bar'} ] } }")}}};
    auto info = std::make_shared<ubuntu::app_launch::snapd::Info>();

    auto pkginfo = info->pkgInfo(ubuntu::app_launch::AppID::Package::from_raw("test-package"));

    mock.result();

    ASSERT_NE(nullptr, pkginfo);
    EXPECT_EQ("test-package", pkginfo->name);
    EXPECT_EQ("1.2.3.4", pkginfo->version);
    EXPECT_EQ("x123", pkginfo->revision);
    EXPECT_EQ("/snap/test-package/x123", pkginfo->directory);
    EXPECT_NE(pkginfo->appnames.end(), pkginfo->appnames.find("foo"));
    EXPECT_NE(pkginfo->appnames.end(), pkginfo->appnames.find("bar"));
}
#endif
