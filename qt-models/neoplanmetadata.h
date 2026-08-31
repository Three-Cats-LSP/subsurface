// SPDX-License-Identifier: GPL-2.0
#ifndef NEOPLANMETADATA_H
#define NEOPLANMETADATA_H

#include <QByteArray>
#include <QJsonDocument>
#include <QVariantMap>

#include <algorithm>
#include <string>

// Neo planner-only fields do not have canonical counterparts in the Subsurface
// dive schema. Keep a compact, invisible payload in notes so exact run/bottom/
// deco times and the rendered schedule survive save, sync and restart.
inline QString neoPlanMetadataMarker(const QVariantMap &metadata)
{
	const QByteArray json = QJsonDocument::fromVariant(metadata).toJson(QJsonDocument::Compact);
	const QByteArray encoded = json.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
	return QStringLiteral("\n<!--NEO_PLAN:%1-->").arg(QString::fromLatin1(encoded));
}

inline QVariantMap neoPlanMetadata(const std::string &notes)
{
	const QString value = QString::fromStdString(notes);
	const QString prefix = QStringLiteral("<!--NEO_PLAN:");
	const qsizetype begin = value.lastIndexOf(prefix);
	if (begin < 0)
		return {};
	const qsizetype payloadBegin = begin + prefix.size();
	const qsizetype end = value.indexOf(QStringLiteral("-->"), payloadBegin);
	if (end < 0)
		return {};
	const QByteArray encoded = value.mid(payloadBegin, end - payloadBegin).toLatin1();
	const QByteArray json = QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding);
	const QJsonDocument document = QJsonDocument::fromJson(json);
	return document.isObject() ? document.toVariant().toMap() : QVariantMap();
}

inline QString neoPlanClock(int seconds)
{
	seconds = std::max(0, seconds);
	return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

#endif
