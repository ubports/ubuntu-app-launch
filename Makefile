default: desktop-exec lsapp application.conf
	@echo "Building"

desktop-exec: desktop-exec.c
	gcc -o desktop-exec desktop-exec.c `pkg-config --cflags --libs glib-2.0`

lsapp: lsapp.c
	gcc -o lsapp lsapp.c `pkg-config --cflags --libs gio-2.0`

application.conf: application.conf.in
	sed -e "s|\@libexecdir\@|/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/|" application.conf.in > application.conf

install: application.conf desktop-exec
	mkdir -p $(DESTDIR)/usr/share/upstart/sessions
	install -m 644 application.conf $(DESTDIR)/usr/share/upstart/sessions/
	mkdir -p $(DESTDIR)/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/
	install -m 755 desktop-exec $(DESTDIR)/usr/lib/$(DEB_BUILD_MULTIARCH)/upstart-app-launch/
	mkdir -p $(DESTDIR)/usr/bin/
	install -m 755 lsapp $(DESTDIR)/usr/bin/

