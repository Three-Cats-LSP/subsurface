// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_SOURCE_H
#define CLOUD_SYNC_WORKSPACE_SOURCE_H
#include "cloudsyncfilestore.h"
#include <QJsonObject>
#include <QTimer>

// Metadata evidence for one observed schema 1/2 source, with the complete
// manifest and exact payload identity. This never grants collection permission.
class CloudWorkspaceCopySource {
public:
	QJsonObject value;
	static CloudWorkspaceCopySource parse(const QByteArray &bytes);
	static CloudWorkspaceCopySource load(const QString &fileName);
	bool valid() const;
	bool matches(const QString &provider, const QString &account) const;
	bool accepts(const QByteArray &payload) const;
	QByteArray json() const;
	QString key() const;
	// Immutable, atomic metadata-only proof; existing conflicting files stay intact.
	bool preserve(const QString &fileName) const;
};

// Read-only source inspection for the later guarded copy operation. Caller
// supplies fresh account verification. Exact reads and fresh alias checks must
// agree; equal bytes under a different identity are still a changed branch.
class CloudWorkspaceSourceReview : public QObject {
	Q_OBJECT
public:
	explicit CloudWorkspaceSourceReview(QNetworkAccessManager *network, QObject *parent = nullptr);
	~CloudWorkspaceSourceReview() override;
	void start(const QString &token, const QString &provider, const QString &account, const std::function<bool()> &scopeCurrent);
	void cancel();
signals:
	void finished(const QJsonObject &source, const QByteArray &payload);
	void failed(const QString &message, bool tokenRejected);
private:
	enum class Step { Idle, Manifest, Payload, ExactManifest, ExactPayload, AliasManifest, AliasPayload };
	bool current();
	void fail(bool rejected = false);
	void received(const QString &name, const QByteArray &bytes, const QString &version, const QString &id);
	CloudSyncFileStore files;
	QTimer deadline;
	CloudSyncProviderType provider = CloudSyncProviderType::GoogleDrive;
	Step step = Step::Idle;
	QString token, providerId, account, manifestId, manifestVersion, payloadId, payloadVersion, payloadName;
	QByteArray manifest, payload;
	std::function<bool()> scopeCurrent;
};
#endif
