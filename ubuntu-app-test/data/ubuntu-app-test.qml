import QtQuick 2.0
import Ubuntu.Components 0.1

MainView {
	automaticOrientation: true
	width: parent.width
	height: parent.height

	Page {
		title: i18n.tr("Ubuntu Application Test")
		TextArea {
			text: i18n.tr("An application that exists as a dummy so that the application you're wishing to run can be overlayed on top of it. If you're seeing this the application is probably starting, or somehow failed to run.")
			width: parent.width - units.gu(4)
			height: parent.height - units.gu(4)
			x: units.gu(2)
			y: units.gu(2)
			readOnly: true
		}
	}
}
