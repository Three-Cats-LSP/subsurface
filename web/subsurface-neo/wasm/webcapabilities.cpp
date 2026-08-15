// SPDX-License-Identifier: GPL-2.0
#include "webcapabilities.h"

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
