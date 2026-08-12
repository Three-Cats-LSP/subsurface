// SPDX-License-Identifier: GPL-2.0
#include "qmlinterface.h"
#include "qmlmanager.h"
#include "neodashboardmodel.h"
#include "neodivecollections.h"
#include "neoequipmentkits.h"
#include "core/cloudstorage.h"
#include "core/cloudsyncmanager.h"
#include "core/neoupdatemanager.h"
#include <QQmlEngine>
#include <QMetaObject>
#include <QTimer>
#include <QUrl>

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#include <jni.h>
#endif

namespace {
CloudSyncManager *neoCloudSync = nullptr;
}

QMLInterface::QMLInterface()
{
	// relink signals to QML
	connect(qPrefCloudStorage::instance(), &qPrefCloudStorage::cloud_verification_statusChanged,
		[this] (int value) { emit cloud_verification_statusChanged(CLOUD_STATUS(value)); });
	connect(qPrefUnits::instance(), &qPrefUnits::duration_unitsChanged,
			[this] (int value) { emit duration_unitsChanged(DURATION(value)); });
	connect(qPrefUnits::instance(), &qPrefUnits::lengthChanged,
			[this] (int value) { emit lengthChanged(LENGTH(value)); });
	connect(qPrefUnits::instance(), &qPrefUnits::pressureChanged,
			[this] (int value) { emit pressureChanged(PRESSURE(value)); });
	connect(qPrefUnits::instance(), &qPrefUnits::temperatureChanged,
			[this] (int value) { emit temperatureChanged(TEMPERATURE(value)); });
	connect(qPrefUnits::instance(), &qPrefUnits::unit_systemChanged,
			[this] (int value) { emit unit_systemChanged(UNIT_SYSTEM(value)); });
	connect(qPrefUnits::instance(), &qPrefUnits::vertical_speed_timeChanged,
			[this] (int value) { emit vertical_speed_timeChanged(TIME(value)); });
	connect(qPrefUnits::instance(), &qPrefUnits::volumeChanged,
			[this] (int value) { emit volumeChanged(VOLUME(value)); });
	connect(qPrefUnits::instance(), &qPrefUnits::weightChanged,
			[this] (int value) { emit weightChanged(WEIGHT(value)); });

	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::ascratelast6mChanged,
			this, &QMLInterface::ascratelast6mChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::ascratestopsChanged,
			this, &QMLInterface::ascratestopsChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::ascrate50Changed,
			this, &QMLInterface::ascrate50Changed);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::ascrate75Changed,
			this, &QMLInterface::ascrate75Changed);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::descrateChanged,
			this, &QMLInterface::descrateChanged);

	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::safetystopChanged,
			this, &QMLInterface::safetystopChanged);
	connect(qPrefTechnicalDetails::instance(), &qPrefTechnicalDetails::vpmb_conservatismChanged,
			this, &QMLInterface::vpmb_conservatismChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::drop_stone_modeChanged,
			this, &QMLInterface::drop_stone_modeChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::last_stopChanged,
			this, &QMLInterface::last_stop6mChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::switch_at_req_stopChanged,
			this, &QMLInterface::switch_at_req_stopChanged);

	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::problemsolvingtimeChanged,
			this, &QMLInterface::problemsolvingtimeChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::bottomsacChanged,
			this, &QMLInterface::bottomsacChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::decosacChanged,
			this, &QMLInterface::decosacChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::sacfactorChanged,
			this, &QMLInterface::sacfactorChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::problemsolvingtimeChanged,
			this, &QMLInterface::problemsolvingtimeChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::o2narcoticChanged,
			this, &QMLInterface::o2narcoticChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::bottompo2Changed,
			this, &QMLInterface::bottompo2Changed);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::decopo2Changed,
			this, &QMLInterface::decopo2Changed);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::bestmixendChanged,
			this, &QMLInterface::bestmixendChanged);

	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::display_runtimeChanged,
			this, &QMLInterface::display_runtimeChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::display_durationChanged,
			this, &QMLInterface::display_durationChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::display_transitionsChanged,
			this, &QMLInterface::display_transitionsChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::verbatim_planChanged,
			this, &QMLInterface::verbatim_planChanged);
	connect(qPrefDivePlanner::instance(), &qPrefDivePlanner::display_variationsChanged,
			this, &QMLInterface::display_variationsChanged);
	connect(qPrefGeneral::instance(), &qPrefGeneral::defaultsetpointChanged,
			this, &QMLInterface::default_setpointChanged);

	connect(qPrefDiveComputer::instance(), &qPrefDiveComputer::sync_dc_timeChanged,
			this, &QMLInterface::sync_dc_timeChanged);
}

void QMLInterface::setup(QQmlContext *ct)
{
	// Register interface class
	static QMLInterface self;
	ct->setContextProperty("Backend", &self);
	ct->setContextProperty("ProfilePrefs", qPrefTechnicalDetails::instance());

	static NeoDashboardModel neoDashboardModel;
	ct->setContextProperty("NeoDashboard", &neoDashboardModel);
	static NeoDiveCollections neoDiveCollections;
	ct->setContextProperty("NeoDiveCollections", &neoDiveCollections);
	static NeoEquipmentKits neoEquipmentKits;
	ct->setContextProperty("NeoEquipmentKits", &neoEquipmentKits);

	// Neo cloud sync coordinator. It deliberately shares Subsurface's existing
	// QNetworkAccessManager, while keeping OAuth/provider state separate from
	// the legacy Subsurface Cloud account backend.
	static CloudSyncManager cloudSync(manager());
	neoCloudSync = &cloudSync;
	ct->setContextProperty("CloudSync", &cloudSync);

	// Shared Neo release/update service. The desktop client has a QWidget
	// presentation for the same manifest; mobile exposes the state to QML and
	// reuses QMLManager's existing passive-notification channel at startup.
	static NeoUpdateManager neoUpdate;
	ct->setContextProperty("NeoUpdate", &neoUpdate);
	connect(&neoUpdate, &NeoUpdateManager::updateAvailableFound, &neoUpdate, []() {
		if (QMLManager::instance()) {
			QMLManager::instance()->setNotificationText(
				QMLInterface::tr("A new Subsurface Neo version (%1) is available. Open Settings / About to download it.")
					.arg(neoUpdate.latestVersion()));
		}
	}, Qt::UniqueConnection);
	QTimer::singleShot(1500, &neoUpdate, []() { neoUpdate.checkForUpdates(false); });

	// Make enums available as types
	qmlRegisterUncreatableType<QMLInterface>("org.subsurfacedivelog.mobile", 1, 0, "Enums", "Enum is not a type");

	qmlRegisterUncreatableType<DivePlannerPointsModel>("org.subsurfacedivelog.mobile", 1, 0, "DivePlannerPointsModel", "Planner model cannot be created in QML.");
	qmlRegisterUncreatableType<CylindersModel>("org.subsurfacedivelog.mobile", 1, 0, "CylindersModel", "Cylinder model cannot be created in QML.");
}

#if defined(Q_OS_ANDROID)
extern "C" JNIEXPORT void JNICALL
Java_org_subsurfacedivelog_mobile_SubsurfaceMobileActivity_oauthCallback(JNIEnv *, jobject, jstring url)
{
	if (!neoCloudSync || !url)
		return;
	const QString callbackUrl = QJniObject::fromLocalRef(url).toString();
	QMetaObject::invokeMethod(neoCloudSync, [callbackUrl]() {
		if (neoCloudSync)
			neoCloudSync->handleAuthorizationRedirect(QUrl(callbackUrl));
	}, Qt::QueuedConnection);
}
#endif
