// SPDX-License-Identifier: GPL-2.0
#ifndef NEO_LOG_METADATA_H
#define NEO_LOG_METADATA_H
#include <QByteArray>
#include <QJsonObject>
#include <optional>
class QSettings;

// Versioned libraries coupled to one canonical log snapshot. Unknown JSON
// fields remain intact; this codec never changes global UI/model state.
class NeoLogMetadata {
public:
	static constexpr int Limit = 4 * 1024 * 1024;
	static std::optional<QJsonObject> parse(const QByteArray &bytes);
	// Empty bytes mean a complete legacy log without metadata; nullopt means
	// malformed or unsupported metadata. No model/settings side effects.
	static std::optional<QByteArray> fromLog(const QByteArray &xml);
	static std::optional<QByteArray> captureProfile(QSettings &settings, const QByteArray &preserved = {});
	// Explicit cache projection by the application after accepted load/apply.
	// The XML/Git log remains the durable authority, including restart recovery.
	static bool projectProfile(QSettings &settings, const QByteArray &bytes);
};
#endif
