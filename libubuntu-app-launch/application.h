
namespace Ubuntu {
namespace AppLaunch {

class Application {
	Application (Connection::Ptr connection = std::make_shared<Connection>(),
	             const std::string &package,
	             const std::string &appname,
	             const std::string &version);

	/* System level info */
	const std::string &package();
	const std::string &appname();
	const std::string &version();

	const std::string &logPath();

	/* Package provided user visible info */
	const std::string &name();
	const std::string &description();
	const std::string &iconPath();
	std::list<std::string> categories();

	/* Query lifecycle */
	const bool isRunning();
	GPid primaryPid();
	bool hasPid(GPid pid);

	/* Manage lifecycle */
	void launch(std::list<std::string> urls = {});
	void pause();
	void resume();

	typedef std::shared_ptr<Application> Ptr;
private:
	class Impl;
	std::static_ptr<Impl> impl;
};

}; // namespace AppLaunch
}; // namespace Ubuntu
