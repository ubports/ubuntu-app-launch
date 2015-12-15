
#include "registry-impl.h"

namespace Ubuntu {
namespace AppLaunch {

Registry::Impl::Impl ():
	thread([](){}, [this]() {
		_clickUser.reset();
		_clickDB.reset();
	})
{

}

void
Registry::Impl::initClick ()
{
	if (_clickDB && _clickUser) {
		return;
	}

	thread.executeOnThread([this]() {
		GError * error = nullptr;

		if (!_clickDB) {
			_clickDB = std::shared_ptr<ClickDB>(click_db_new(), [](ClickDB * db){ g_clear_object(&db); });
			/* If TEST_CLICK_DB is unset, this reads the system database. */
			click_db_read(_clickDB.get(), g_getenv("TEST_CLICK_DB"), &error);

			if (error != nullptr) {
				auto perror = std::shared_ptr<GError>(error, [](GError * error){ g_error_free(error); });
				throw std::runtime_error(error->message);
			}
		}

		if (!_clickUser) {
			_clickUser = std::shared_ptr<ClickUser>(click_user_new_for_user(_clickDB.get(), g_getenv("TEST_CLICK_USER"), &error), [](ClickUser * user) { g_clear_object(&user); });

			if (error != nullptr) {
				auto perror = std::shared_ptr<GError>(error, [](GError * error){ g_error_free(error); });
				throw std::runtime_error(error->message);
			}
		}
	});
}

std::shared_ptr<JsonObject>
Registry::Impl::getClickManifest(const std::string& package)
{
	initClick();

	return thread.executeOnThread<std::shared_ptr<JsonObject>>([this, package]() {
		GError * error = nullptr;
		auto retval = std::shared_ptr<JsonObject>(
			click_user_get_manifest(_clickUser.get(), package.c_str(), &error),
			[](JsonObject * obj) { g_clear_object(&obj); });

		if (error != nullptr) {
			auto perror = std::shared_ptr<GError>(error, [](GError * error){ g_error_free(error); });
			throw std::runtime_error(error->message);
		}

		return retval;
	});
}

std::list<std::string>
Registry::Impl::getClickPackages()
{
	initClick();

	return thread.executeOnThread<std::list<std::string>>([this]() {
		GError * error = nullptr;
		GList * pkgs = click_db_get_packages(_clickDB.get(), FALSE, &error);

		if (error != nullptr) {
			auto perror = std::shared_ptr<GError>(error, [](GError * error){ g_error_free(error); });
			throw std::runtime_error(error->message);
		}

		std::list<std::string> list;
		for (GList * item = pkgs; item != NULL; item = g_list_next(item)) {
			list.emplace_back(std::string((gchar *)item->data));	
		}

		g_list_free_full(pkgs, g_free);
		return list;
	});
}

std::string
Registry::Impl::getClickDir(const std::string& package)
{
	initClick();

	return thread.executeOnThread<std::string>([this, package]() {
		GError * error = nullptr;
		auto dir = click_user_get_path(_clickUser.get(), package.c_str(), &error);

		if (error != nullptr) {
			auto perror = std::shared_ptr<GError>(error, [](GError * error){ g_error_free(error); });
			throw std::runtime_error(error->message);
		}

		std::string cppdir(dir);
		g_free(dir);
		return cppdir;
	});
}

}; // namespace AppLaunch
}; // namespace Ubuntu
