default: desktop-exec desktop-hook lsapp zg-report-app application.conf application-click.conf application-legacy.conf debian/upstart-app-launch-desktop.click-hook
	@echo "Building"

desktop-exec: desktop-exec.c
	gcc -o desktop-exec desktop-exec.c `pkg-config --cflags --libs glib-2.0 gio-2.0` -Wall -Werror

desktop-hook: desktop-hook.c helpers.c helpers.h
	gcc -o desktop-hook desktop-hook.c helpers.c `pkg-config --cflags --libs glib-2.0 gio-2.0 json-glib-1.0` -Wall -Werror

lsapp: lsapp.c
	gcc -o lsapp lsapp.c `pkg-config --cflags --libs gio-2.0` -Wall -Werror

zg-report-app: zg-report-app.c
	gcc -o zg-report-app zg-report-app.c `pkg-config --cflags --libs zeitgeist-1.0` -Wall -Werror

application-legacy.conf: application-legacy.conf.in
	sed -e "s|\@pkglibexecdir\@|/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/|" application-legacy.conf.in > application-legacy.conf

application-click.conf: application-click.conf.in
	sed -e "s|\@pkglibexecdir\@|/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/|" application-click.conf.in > application-click.conf

debian/upstart-app-launch-desktop.click-hook: upstart-app-launch-desktop.click-hook.in
	sed -e "s|\@pkglibexecdir\@|/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/|" upstart-app-launch-desktop.click-hook.in > debian/upstart-app-launch-desktop.click-hook

install: application-legacy.conf application-legacy.conf desktop-exec desktop-hook
	mkdir -p $(DESTDIR)/usr/share/upstart/sessions
	install -m 644 application.conf $(DESTDIR)/usr/share/upstart/sessions/
	install -m 644 application-legacy.conf $(DESTDIR)/usr/share/upstart/sessions/
	install -m 644 application-click.conf $(DESTDIR)/usr/share/upstart/sessions/
	mkdir -p $(DESTDIR)/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/
	install -m 755 desktop-exec $(DESTDIR)/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/
	install -m 755 desktop-hook $(DESTDIR)/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/
	install -m 755 zg-report-app $(DESTDIR)/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/
	mkdir -p $(DESTDIR)/usr/bin/
	install -m 755 lsapp $(DESTDIR)/usr/bin/

check: application-legacy.conf application-click.conf
	@echo " *** Checking Application Job *** "
	@./test-conffile.sh application.conf
	@echo " *** Checking Application Click Job *** "
	@./test-conffile.sh application-click.conf
	@echo " *** Checking Application Legacy Job *** "
	@./test-conffile.sh application-legacy.conf
