// SPDX-License-Identifier: GPL-2.0
#include "webcapabilities.h"
#include "neowebdivelogmodel.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <memory>

namespace {

std::unique_ptr<QGuiApplication> application;
std::unique_ptr<QQmlApplicationEngine> engine;
std::unique_ptr<WebCapabilities> capabilities;
std::unique_ptr<NeoWebDiveLogModel> diveLogModel;

} // namespace

int main(int argc, char *argv[])
{
	QCoreApplication::setOrganizationName(QStringLiteral("Subsurface Neo"));
	QCoreApplication::setApplicationName(QStringLiteral("Subsurface Neo Web"));

	application = std::make_unique<QGuiApplication>(argc, argv);
	engine = std::make_unique<QQmlApplicationEngine>();
	capabilities = std::make_unique<WebCapabilities>();
	diveLogModel = std::make_unique<NeoWebDiveLogModel>();
	engine->rootContext()->setContextProperty(QStringLiteral("webCapabilities"), capabilities.get());
	engine->rootContext()->setContextProperty(QStringLiteral("diveLog"), diveLogModel.get());
	engine->loadFromModule(QStringLiteral("SubsurfaceNeo.Web"), QStringLiteral("Main"));

	if (engine->rootObjects().isEmpty())
		return 1;

#if defined(Q_OS_WASM)
	// The browser owns the event loop. Keep the application objects alive after
	// main() returns, as required by Qt for WebAssembly.
	return 0;
#else
	return application->exec();
#endif
}
