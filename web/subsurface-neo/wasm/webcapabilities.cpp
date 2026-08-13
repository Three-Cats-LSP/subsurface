// SPDX-License-Identifier: GPL-2.0
#include "webcapabilities.h"

#include <QFile>
#include <QFileInfo>
#include <QtGlobal>

#if defined(__EMSCRIPTEN__)
#include <emscripten/val.h>
#endif

namespace {

#if defined(__EMSCRIPTEN__)
bool hasProperty(const emscripten::val &object, const char *name)
{
	const emscripten::val value = object[name];
	return !value.isUndefined() && !value.isNull();
}
#endif

} // namespace

WebCapabilities::WebCapabilities(QObject *parent) : QObject(parent)
{
#if defined(__EMSCRIPTEN__)
	const emscripten::val global = emscripten::val::global();
	const emscripten::val navigator = global["navigator"];
	m_secureContext = global["isSecureContext"].as<bool>();
	m_webBluetoothAvailable = hasProperty(navigator, "bluetooth");
	m_webSerialAvailable = hasProperty(navigator, "serial");
	const QString userAgent = QString::fromStdString(navigator["userAgent"].as<std::string>());
	m_mobileBrowser = userAgent.contains(QStringLiteral("Android"), Qt::CaseInsensitive) ||
		userAgent.contains(QStringLiteral("iPhone"), Qt::CaseInsensitive) ||
		userAgent.contains(QStringLiteral("iPad"), Qt::CaseInsensitive);
	m_browserSummary = userAgent;
#else
	m_browserSummary = tr("Native development preview");
#endif
}

bool WebCapabilities::webAssemblyRuntime() const
{
#if defined(__EMSCRIPTEN__)
	return true;
#else
	return false;
#endif
}

bool WebCapabilities::secureContext() const
{
	return m_secureContext;
}

bool WebCapabilities::webBluetoothAvailable() const
{
	return m_webBluetoothAvailable;
}

bool WebCapabilities::webSerialAvailable() const
{
	return m_webSerialAvailable;
}

bool WebCapabilities::mobileBrowser() const
{
	return m_mobileBrowser;
}

QString WebCapabilities::browserSummary() const
{
	return m_browserSummary;
}

QString WebCapabilities::selectedFileStatus() const
{
	return m_selectedFileStatus;
}

void WebCapabilities::inspectLocalFile(const QUrl &url)
{
	const QString localPath = url.toLocalFile();
	QFile file(localPath);
	if (!file.open(QIODevice::ReadOnly)) {
		m_selectedFileStatus = tr("The selected file could not be opened.");
		emit selectedFileStatusChanged();
		return;
	}

	const QByteArray header = file.read(64 * 1024).toLower();
	const QFileInfo info(file);
	const bool recognizedXml = header.contains("<divelog") || header.contains("<divesites") ||
		header.contains("<uddf");
	const bool portableBundle = info.suffix().compare(QStringLiteral("subsurface-neo"), Qt::CaseInsensitive) == 0;
	if (recognizedXml || portableBundle) {
		m_selectedFileStatus = tr("%1 selected (%2 KB). Ready for the canonical Subsurface import bridge.")
			.arg(info.fileName())
			.arg(qMax<qint64>(1, info.size() / 1024));
	} else {
		m_selectedFileStatus = tr("%1 does not look like a supported Subsurface, UDDF, or Neo backup file.")
			.arg(info.fileName());
	}
	emit selectedFileStatusChanged();
}
