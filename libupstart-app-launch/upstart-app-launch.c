
gboolean
upstart_app_launch_start_application (const gchar * appid, const gchar * const * uris)
{

	return FALSE;
}

gboolean
upstart_app_launch_observer_add_app_start (upstart_app_launch_app_observer_t observer, gpointer user_data)
{

	return FALSE;
}

gboolean
upstart_app_launch_observer_add_app_stop (upstart_app_launch_app_observer_t observer, gpointer user_data)
{

	return FALSE;
}

gboolean
upstart_app_launch_observer_delete_app_start (upstart_app_launch_app_observer_t observer, gpointer user_data)
{

	return FALSE;
}

gboolean
upstart_app_launch_observer_delete_app_stop (upstart_app_launch_app_observer_t observer, gpointer user_data)
{

	return FALSE;
}

gchar **
upstart_app_launch_list_running_apps (void)
{
	gchar ** retval = g_new(gchar *, 1);
	retval[0] = NULL;

	return retval;
}

GPid
upstart_app_launch_check_app_running (const gchar * appid)
{

	return 0;
}
