
#include "jobs-systemd.h"

namespace ubuntu
{
namespace app_launch
{
namespace jobs
{
namespace instance
{

class SystemD : public Base
{
public:
    explicit SystemD(const AppID& appId,
                     const std::string& job,
                     const std::string& instance,
                     const std::vector<Application::URL>& urls,
                     const std::shared_ptr<Registry>& registry);

    /* Query lifecycle */
    pid_t primaryPid() override;
    std::string logPath() override;
    std::vector<pid_t> pids() override;

    /* Manage lifecycle */
    void stop() override;
};  // class SystemD

SystemD::SystemD(const AppID& appId,
                 const std::string& job,
                 const std::string& instance,
                 const std::vector<Application::URL>& urls,
                 const std::shared_ptr<Registry>& registry)
    : Base(appId, job, instance, urls, registry)
{
    g_debug("Creating a new SystemD for '%s' instance '%s'", std::string(appId).c_str(), instance.c_str());
}

pid_t SystemD::primaryPid()
{
    return {};
}

std::string SystemD::logPath()
{
    return {};
}

std::vector<pid_t> SystemD::pids()
{
    return {};
}

void SystemD::stop()
{
}

}  // namespace instance

namespace manager
{

SystemD::SystemD(std::shared_ptr<Registry> registry)
    : Base(registry)
{
}

SystemD::~SystemD()
{
}

std::shared_ptr<Application::Instance> SystemD::launch(
    const AppID& appId,
    const std::string& job,
    const std::string& instance,
    const std::vector<Application::URL>& urls,
    launchMode mode,
    std::function<std::list<std::pair<std::string, std::string>>(void)>& getenv)
{
    return {};
}

std::shared_ptr<Application::Instance> SystemD::existing(const AppID& appId,
                                                         const std::string& job,
                                                         const std::string& instance,
                                                         const std::vector<Application::URL>& urls)
{
    return {};
}

std::vector<std::shared_ptr<instance::Base>> SystemD::instances(const AppID& appID, const std::string& job)
{
    return {};
}

std::list<std::shared_ptr<Application>> SystemD::runningApps()
{
    return {};
}

}  // namespace manager
}  // namespace jobs
}  // namespace app_launch
}  // namespace ubuntu
