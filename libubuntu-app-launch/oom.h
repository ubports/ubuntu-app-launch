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

#pragma once
#pragma GCC visibility push(default)

namespace ubuntu
{
namespace app_launch
{
namespace oom
{

enum class Score : std::int32_t;

/** Get the OOM Score that should be associated with an application that
    is focused. */
const Score focused();
/** Get the OOM Score that should be associated with an application that
    is pause. */
const Score paused();
/** Create a new OOM Score value with a label for debugging messages. This
    function will throw a warning if the value isn't between focused() and
    paused(). An exception will be thrown if it isn't between -1000 and 1000.

    \warning This function should only be used for prototyping, the behavior
             of what causes exceptions or warning may change in the future.

    \param value A value between -1000 and 1000 to pass to the kernel's oom_adjust
    \param label The name of this state for debugging
*/
const Score fromLabelAndValue(std::int32_t value, const std::string& label);

}  // namespace oom
}  // namespace app_launch
}  // namespace ubuntu

#pragma GCC visibility pop
