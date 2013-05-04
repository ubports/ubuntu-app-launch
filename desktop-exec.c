
#include <glib.h>
#include <gio/gdesktopappinfo.h>

int
main (int argc, char * argv[])
{
	if (argc != 2) {
		g_error("Should be called as: %s <app_id>", argv[0]);
		return 1;
	}

	gchar * desktop = g_strdup_printf("%s.desktop", argv[1]);
	GDesktopAppInfo * appinfo = g_desktop_app_info_new(desktop);
	g_free(desktop);

	if (!G_IS_DESKTOP_APP_INFO(appinfo)) {
		g_error("Unable to load app '%s'", argv[1]);
		return 1;
	}

	g_app_info_launch(G_APP_INFO(appinfo), NULL, NULL, NULL);

	g_object_unref(appinfo);
	return 0;
}
