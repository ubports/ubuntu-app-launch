default: desktop-exec desktop-hook lsapp zg-report-app application.conf application-click.conf application-legacy.conf upstart-app-launch-desktop.hook
	@echo "Building"

desktop-exec: desktop-exec.c
	gcc -o desktop-exec desktop-exec.c `pkg-config --cflags --libs glib-2.0 gio-2.0`

desktop-hook: desktop-hook.c
	gcc -o desktop-hook desktop-hook.c helpers.c `pkg-config --cflags --libs glib-2.0 gio-2.0 json-glib-1.0`

lsapp: lsapp.c
	gcc -o lsapp lsapp.c `pkg-config --cflags --libs gio-2.0`

zg-report-app: zg-report-app.c
	gcc -o zg-report-app zg-report-app.c `pkg-config --cflags --libs zeitgeist-1.0`

application-legacy.conf: application-legacy.conf.in
	sed -e "s|\@libexecdir\@|/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/|" application-legacy.conf.in > application-legacy.conf

application-click.conf: application-click.conf.in
	sed -e "s|\@libexecdir\@|/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/|" application-click.conf.in > application-click.conf

upstart-app-launch-desktop.hook: upstart-app-launch-desktop.hook.in
	sed -e "s|\@libexecdir\@|/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/|" upstart-app-launch-desktop.hook.in > upstart-app-launch-desktop.hook

install: application-legacy.conf application-legacy.conf desktop-exec desktop-hook upstart-app-launch-desktop.hook
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
	mkdir -p $(DESTDIR)/usr/share/click/hooks
	install -m 644 upstart-app-launch-desktop.hook $(DESTDIR)/usr/share/click/hooks/

check: application-legacy.conf application-click.conf
	@echo " *** Checking Application Job *** "
	@./test-conffile.sh application.conf
	@echo " *** Checking Application Click Job *** "
	@./test-conffile.sh application-click.conf
	@echo " *** Checking Application Legacy Job *** "
	@./test-conffile.sh application-legacy.conf
