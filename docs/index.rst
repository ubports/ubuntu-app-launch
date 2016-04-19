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

Application Info Desktop
------------------------

.. doxygenclass:: ubuntu::app_launch::app_info::Desktop
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

Registry Implementation
-----------------------

.. doxygenclass:: ubuntu::app_launch::Registry::Impl
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

Submitter Responsibilities
..........................
