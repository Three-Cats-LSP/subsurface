// SPDX-License-Identifier: GPL-2.0
#ifndef CLOUD_SYNC_WORKSPACE_MODEL_H
#define CLOUD_SYNC_WORKSPACE_MODEL_H
#include <QByteArray>
#include <memory>

// Stage a canonical snapshot with the real Subsurface parser and serializer.
// Rejected/staged-only data never resets the current model/index/selection or
// registers tags/fingerprints. Accepted installation still needs the caller's
// cloud/local recovery guards and an explicit durable-persistence acknowledgement.
class CloudWorkspaceModel {
public:
	CloudWorkspaceModel();
	~CloudWorkspaceModel();
	bool prepare(const QByteArray &payload, const QByteArray &baseline = {});
	QByteArray normalizedXml() const;
	bool install();
	static QByteArray currentXml();
private:
	struct Prepared;
	std::unique_ptr<Prepared> prepared;
};
#endif
