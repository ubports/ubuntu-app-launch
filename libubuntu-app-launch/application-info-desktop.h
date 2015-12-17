
#include "application.h"
#include <gio/gdesktopappinfo.h>
#include <mutex>

#pragma once

namespace Ubuntu {
namespace AppLaunch {
namespace AppInfo {

class Desktop : public Application::Info {
public:
	Desktop(std::shared_ptr<GDesktopAppInfo> appinfo,
	        const std::string &basePath);

	const Application::Info::Name &name() override;
	const Application::Info::Description &description() override;
	const Application::Info::IconPath &iconPath() override;
	std::list<Application::Info::Category> categories() override;

private:
	Application::Info::Name _name;
	Application::Info::Description _description;
	Application::Info::IconPath _iconPath;

	std::once_flag _nameFlag;
	std::once_flag _descriptionFlag;
	std::once_flag _iconPathFlag;

	std::shared_ptr<GDesktopAppInfo> _appinfo;
	std::string _basePath;
};


}; // namespace AppInfo
}; // namespace AppLaunch
}; // namespace Ubuntu
