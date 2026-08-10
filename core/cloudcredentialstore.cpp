// SPDX-License-Identifier: GPL-2.0
#include "cloudcredentialstore.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <wincred.h>
#elif defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

namespace {

QString credentialName(const QString &providerId)
{
	return QStringLiteral("SubsurfaceNeo/Cloud/%1").arg(providerId);
}

} // namespace

QByteArray CloudCredentialStore::load(const QString &providerId)
{
#if defined(Q_OS_WIN)
	const std::wstring target = credentialName(providerId).toStdWString();
	PCREDENTIALW credential = nullptr;
	if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential))
		return {};
	QByteArray result(reinterpret_cast<const char *>(credential->CredentialBlob),
			  static_cast<qsizetype>(credential->CredentialBlobSize));
	CredFree(credential);
	return result;
#elif defined(Q_OS_ANDROID)
	const QJniObject key = QJniObject::fromString(providerId);
	QJniObject value = QJniObject::callStaticObjectMethod(
		"org/subsurfacedivelog/mobile/CloudCredentialStore",
		"load",
		"(Ljava/lang/String;)Ljava/lang/String;",
		key.object<jstring>());
	if (!value.isValid())
		return {};
	return QByteArray::fromBase64(value.toString().toLatin1());
#else
	Q_UNUSED(providerId)
	// Deliberately no plaintext fallback. Platforms without a secure credential
	// backend keep OAuth tokens in memory for the current process only.
	return {};
#endif
}

bool CloudCredentialStore::save(const QString &providerId, const QByteArray &payload)
{
#if defined(Q_OS_WIN)
	const std::wstring target = credentialName(providerId).toStdWString();
	CREDENTIALW credential = {};
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = const_cast<LPWSTR>(target.c_str());
	credential.CredentialBlobSize = static_cast<DWORD>(payload.size());
	credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(payload.constData()));
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	return CredWriteW(&credential, 0) != FALSE;
#elif defined(Q_OS_ANDROID)
	const QJniObject key = QJniObject::fromString(providerId);
	const QJniObject value = QJniObject::fromString(QString::fromLatin1(payload.toBase64()));
	return QJniObject::callStaticMethod<jboolean>(
		"org/subsurfacedivelog/mobile/CloudCredentialStore",
		"save",
		"(Ljava/lang/String;Ljava/lang/String;)Z",
		key.object<jstring>(), value.object<jstring>());
#else
	Q_UNUSED(providerId)
	Q_UNUSED(payload)
	return false;
#endif
}

bool CloudCredentialStore::remove(const QString &providerId)
{
#if defined(Q_OS_WIN)
	const std::wstring target = credentialName(providerId).toStdWString();
	if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0))
		return true;
	return GetLastError() == ERROR_NOT_FOUND;
#elif defined(Q_OS_ANDROID)
	const QJniObject key = QJniObject::fromString(providerId);
	return QJniObject::callStaticMethod<jboolean>(
		"org/subsurfacedivelog/mobile/CloudCredentialStore",
		"remove",
		"(Ljava/lang/String;)Z",
		key.object<jstring>());
#else
	Q_UNUSED(providerId)
	return true;
#endif
}
