.. Ubuntu App Launch documentation master file, created by
   sphinx-quickstart on Thu Apr  7 09:22:13 2016.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

.. toctree::
   :maxdepth: 2

Overview
========

Ubuntu App Launch is the abstraction that creates a consistent interface for
managing apps on Ubuntu Touch. It is used by Unity8 and other programs to
start and stop applications, as well as query which ones are currently open.
It doesn't have its own service or processes though, it relies on the system
init daemon to manage the processes (currently Upstart_) but configures them
in a way that they're discoverable and usable by higher level applications.

.. _Upstart: http://upstart.ubuntu.com/


Environment Variables
=====================

There are a few environment variables that can effect the behavior of UAL while
it is running.

UBUNTU_APP_LAUNCH_CG_MANAGER_NAME
  The DBus name that CG Manager registers under if it is on the session bus.

UBUNTU_APP_LAUNCH_CG_MANAGER_SESSION_BUS
  Tell UAL to look on the session bus for CG Manager.

UBUNTU_APP_LAUNCH_DEMANGLER
  Path to the UAL demangler tool that will get the Mir FD for trusted prompt session.

UBUNTU_APP_LAUNCH_DISABLE_SNAPD_TIMEOUT
  Wait as long as Snapd wants to return data instead of erroring after 100ms.

UBUNTU_APP_LAUNCH_LEGACY_ROOT
  Set the path that represents the root for legacy applications.

UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH
  Path to the libertine launch utility for setting up libertine containers and XMir based legacy apps.

UBUNTU_APP_LAUNCH_LINK_FARM
  Path to the link farm that is created by Click of all the installed Click applications.

UBUNTU_APP_LAUNCH_OOM_HELPER
  Path to the setuid helper that configures OOM values on application processes that we otherwise couldn't, mostly this is for Oxide.

UBUNTU_APP_LAUNCH_OOM_PROC_PATH
  Path to look for the files to set OOM values, defaults to /proc.

UBUNTU_APP_LAUNCH_SNAP_BASEDIR
  The place where snaps are installed in the system, /snap is the default.

UBUNTU_APP_LAUNCH_SNAP_LEGACY_EXEC
  A snappy command that is used to launch legacy applications in the current snap, to ensure the environment gets configured correctly.

UBUNTU_APP_LAUNCH_SNAPD_SOCKET
  Path to the snapd socket.

UBUNTU_APP_LAUNCH_XMIR_PATH
  Tool that helps to start XMir and sets the DISPLAY variable for applications


API Documentation
=================

AppID
-----

.. doxygenstruct:: ubuntu::app_launch::AppID
   :project: libubuntu-app-launch
   :members:
   :undoc-members:

Application
-----------

.. doxygenclass:: ubuntu::app_launch::Application
   :project: libubuntu-app-launch
   :members:
   :undoc-members:

Helper
------

.. doxygenclass:: ubuntu::app_launch::Helper
   :project: libubuntu-app-launch
   :members:
   :undoc-members:

Registry
--------

.. doxygenclass:: ubuntu::app_launch::Registry
   :project: libubuntu-app-launch
   :members:
   :undoc-members:

Implementation Details
======================

Application Implementation Base
-------------------------------

.. doxygenclass:: ubuntu::app_launch::app_impls::Base
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Application Implementation Click
--------------------------------

.. doxygenclass:: ubuntu::app_launch::app_impls::Click
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Application Implementation Legacy
---------------------------------

.. doxygenclass:: ubuntu::app_launch::app_impls::Legacy
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Application Implementation Libertine
------------------------------------

.. doxygenclass:: ubuntu::app_launch::app_impls::Libertine
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Application Implementation Snappy
---------------------------------

.. doxygenclass:: ubuntu::app_launch::app_impls::Snap
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Application Info Desktop
------------------------

.. doxygenclass:: ubuntu::app_launch::app_info::Desktop
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Application Info Snap
------------------------

.. doxygenclass:: ubuntu::app_launch::app_impls::SnapInfo
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Application Icon Finder
------------------------

.. doxygenclass:: ubuntu::app_launch::IconFinder
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Helper Implementation Click
---------------------------

.. doxygenclass:: ubuntu::app_launch::helper_impls::Click
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Jobs Manager Base
-----------------

.. doxygenclass:: ubuntu::app_launch::jobs::manager::Base
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Jobs Instance Base
------------------

.. doxygenclass:: ubuntu::app_launch::jobs::instance::Base
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Jobs Manager Upstart
--------------------

.. doxygenclass:: ubuntu::app_launch::jobs::manager::Upstart
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Jobs Instance Upstart
---------------------

.. doxygenclass:: ubuntu::app_launch::jobs::instance::Upstart
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Registry Implementation
-----------------------

.. doxygenclass:: ubuntu::app_launch::Registry::Impl
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Snapd Info
----------

.. doxygenclass:: ubuntu::app_launch::snapd::Info
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Type Tagger
-----------

.. doxygenclass:: ubuntu::app_launch::TypeTagger
   :project: libubuntu-app-launch
   :members:
   :protected-members:
   :private-members:
   :undoc-members:

Quality
=======

Merge Requirements
------------------

This documents the expections that the project has on what both submitters
and reviewers should ensure that they've done for a merge into the project.

Submitter Responsibilities
..........................

	* Ensure the project compiles and the test suite executes without error
	* Ensure that non-obvious code has comments explaining it

Reviewer Responsibilities
.........................

	* Did the Jenkins build compile?  Pass?  Run unit tests successfully?
	* Are there appropriate tests to cover any new functionality?
	* If this MR effects application startup:
		* Run test case: ubuntu-app-launch/click-app
		* Run test case: ubuntu-app-launch/legacy-app
		* Run test case: ubuntu-app-launch/secondary-activation
	* If this MR effect untrusted-helpers:
		* Run test case: ubuntu-app-launch/helper-run
