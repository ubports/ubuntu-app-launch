
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

	const std::string &name() override;
	const std::string &description() override;
	const std::string &iconPath() override;
	std::list<std::string> categories() override;

private:
	std::string _name;
	std::string _description;
	std::string _iconPath;

	std::once_flag _nameFlag;
	std::once_flag _descriptionFlag;
	std::once_flag _iconPathFlag;

	std::shared_ptr<GDesktopAppInfo> _appinfo;
	std::string _basePath;
};


}; // namespace AppInfo
}; // namespace AppLaunch
}; // namespace Ubuntu
