// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspace.h"
#include "cloudsyncmanifest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

namespace {
const QString PageFormat = QStringLiteral("neo-workspace-history-pages-development-1");
constexpr int PageCountLimit = CloudWorkspaceRecord::ArchivePageLimit, DeltaEntryLimit = 512;
QString text(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }
QByteArray compactJson(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
bool fields(const QJsonObject &o, const QStringList &keys)
{
	if (o.size() != keys.size()) return false;
	for (const auto &key : keys) if (!o.contains(key)) return false;
	return true;
}
bool hash(const QString &s) { static const QRegularExpression pattern("^[a-f0-9]{64}$"); return pattern.match(s).hasMatch(); }
bool pinValid(const QJsonObject &pin, const CloudWorkspaceReference &ref)
{
	const auto object = text(pin, "object"), version = text(pin, "version"), name = text(pin, "name");
	const auto prefix = ref.rootName().chopped(5) + "-" + ref.key() + "-archive-";
	const auto nonce = name.mid(prefix.size(), 36);
	const auto bytes = pin.value("bytes").toDouble(-1);
	return ref.valid() && fields(pin, {"name", "object", "version", "sha256", "bytes", "model_sha256"}) &&
		CloudHistoryRecord::validEpoch(nonce) && name == CloudWorkspaceRecord::archiveName(ref, nonce) &&
		CloudWorkspaceReference::create(text(ref.value, "provider"), text(ref.value, "account"), text(ref.value, "epoch"), object).valid() &&
		object != text(ref.value, "root_id") && !version.isEmpty() && version.size() <= 512 &&
		!version.contains(QChar::Null) && !version.contains('\r') && !version.contains('\n') &&
		hash(text(pin, "sha256")) && hash(text(pin, "model_sha256")) && pin.value("bytes").isDouble() &&
		bytes >= 1 && bytes <= CloudWorkspaceRecord::WireLimit && bytes == double(qint64(bytes));
}
bool changesValid(const QJsonObject &changes)
{
	const auto journal = changes.value("journal").toObject();
	const bool metadata = text(changes, "format") == "neo-workspace-history-development-5";
	QStringList keys = {"format", "journal", "copies", "legacy_acceptances"}; if (metadata) keys.append("metadata_payloads");
	return fields(changes, keys) && changes.value("format").isString() &&
		changes.value("journal").isObject() && fields(journal, {"format", "legacy_sha256", "sequence", "head", "history", "tickets", "reservations"}) &&
		journal.value("history").isArray() && journal.value("tickets").isObject() && journal.value("reservations").isObject() &&
		changes.value("copies").isObject() && changes.value("legacy_acceptances").isArray() &&
		journal.value("history").toArray().size() <= DeltaEntryLimit && journal.value("tickets").toObject().size() <= DeltaEntryLimit &&
		journal.value("reservations").toObject().size() <= DeltaEntryLimit && changes.value("copies").toObject().size() <= DeltaEntryLimit &&
		changes.value("legacy_acceptances").toArray().size() <= DeltaEntryLimit &&
		(!metadata || (changes.value("metadata_payloads").isObject() && changes.value("metadata_payloads").toObject().size() <= DeltaEntryLimit));
}
QJsonObject wireObject(const QByteArray &bytes, const CloudWorkspaceReference &ref)
{
	if (bytes.isEmpty() || bytes.size() > CloudWorkspaceRecord::WireLimit) return {};
	QJsonParseError error; const auto doc = QJsonDocument::fromJson(bytes, &error); const auto o = doc.object();
	if (error.error != QJsonParseError::NoError || !doc.isObject() ||
	    !fields(o, {"schema", "format", "workspace", "archive", "changes"}) || o.value("schema") != QJsonValue(4) ||
	    text(o, "format") != PageFormat || o.value("workspace") != ref.value ||
	    !pinValid(o.value("archive").toObject(), ref) || !changesValid(o.value("changes").toObject())) return {};
	return o;
}
QJsonObject difference(const QJsonObject &after, const QJsonObject &before)
{
	QJsonObject result;
	for (auto it = after.begin(); it != after.end(); ++it) if (before.value(it.key()) != it.value()) result[it.key()] = it.value();
	return result;
}
QJsonArray suffix(const QJsonArray &after, int previousSize)
{
	QJsonArray result;
	for (int i = previousSize; i < after.size(); ++i) result.append(after[i]);
	return result;
}
QJsonObject delta(const QJsonObject &after, const QJsonObject &before)
{
	const auto a = after.value("journal").toObject(), b = before.value("journal").toObject();
	QJsonObject result{{"format", after.value("format")}, {"journal", QJsonObject{
		{"format", a.value("format")}, {"legacy_sha256", a.value("legacy_sha256")}, {"sequence", a.value("sequence")}, {"head", a.value("head")},
		{"history", suffix(a.value("history").toArray(), b.value("history").toArray().size())},
		{"tickets", difference(a.value("tickets").toObject(), b.value("tickets").toObject())},
		{"reservations", difference(a.value("reservations").toObject(), b.value("reservations").toObject())}}},
		{"copies", difference(after.value("copies").toObject(), before.value("copies").toObject())},
		{"legacy_acceptances", suffix(after.value("legacy_acceptances").toArray(), before.value("legacy_acceptances").toArray().size())}};
	if (text(after, "format") == "neo-workspace-history-development-5")
		result["metadata_payloads"] = difference(after.value("metadata_payloads").toObject(), before.value("metadata_payloads").toObject());
	return result;
}
void overlay(QJsonObject &base, const QJsonObject &changes)
{
	for (auto it = changes.begin(); it != changes.end(); ++it) base[it.key()] = it.value();
}
QJsonObject apply(const QJsonObject &before, const QJsonObject &changes)
{
	auto after = before; auto journal = before.value("journal").toObject(); const auto patch = changes.value("journal").toObject();
	for (const auto &key : {"format", "legacy_sha256", "sequence", "head"}) journal[QLatin1String(key)] = patch.value(QLatin1String(key));
	auto history = journal.value("history").toArray(); for (const auto &v : patch.value("history").toArray()) history.append(v);
	auto tickets = journal.value("tickets").toObject(), reservations = journal.value("reservations").toObject();
	overlay(tickets, patch.value("tickets").toObject()); overlay(reservations, patch.value("reservations").toObject());
	journal["history"] = history; journal["tickets"] = tickets; journal["reservations"] = reservations;
	after["journal"] = journal; after["format"] = changes.value("format");
	const auto format = text(after, "format");
	if (format != "neo-workspace-history-development-1") {
		auto copies = after.value("copies").toObject(); overlay(copies, changes.value("copies").toObject()); after["copies"] = copies;
	}
	if (format == "neo-workspace-history-development-3" || format == "neo-workspace-history-development-4" || format == "neo-workspace-history-development-5") {
		auto events = after.value("legacy_acceptances").toArray(); for (const auto &v : changes.value("legacy_acceptances").toArray()) events.append(v);
		after["legacy_acceptances"] = events;
	}
	if (format == "neo-workspace-history-development-5") {
		auto metadata = after.value("metadata_payloads").toObject(); overlay(metadata, changes.value("metadata_payloads").toObject()); after["metadata_payloads"] = metadata;
	}
	return after;
}
}

QJsonObject CloudWorkspaceRecord::logicalValue() const
{
	auto result = value; result.remove("segments"); return result;
}
QJsonArray CloudWorkspaceRecord::archivePages() const { return value.value("segments").toObject().value("archives").toArray(); }
QByteArray CloudWorkspaceRecord::json() const
{
	if (!value.contains("segments")) return compactJson(value);
	const auto pages = archivePages();
	if (pages.isEmpty()) return {};
	return compactJson({{"schema", 4}, {"format", PageFormat}, {"workspace", reference().value},
		{"archive", pages.first().toObject().value("pin")},
		{"changes", delta(logicalValue(), value.value("segments").toObject().value("base").toObject())}});
}
QString CloudWorkspaceRecord::archiveName(const CloudWorkspaceReference &ref, const QString &nonce)
{
	return ref.valid() && CloudHistoryRecord::validEpoch(nonce) ? ref.rootName().chopped(5) + "-" + ref.key() + "-archive-" + nonce + ".json" : QString();
}
QJsonObject CloudWorkspaceRecord::archivePointer(const QByteArray &bytes, const CloudWorkspaceReference &ref)
{
	return wireObject(bytes, ref).value("archive").toObject();
}
QJsonObject CloudWorkspaceRecord::archivePin(const QString &nonce, const QString &object, const QString &version) const
{
	if (!valid()) return {};
	const auto raw = json();
	QJsonObject pin{{"name", archiveName(reference(), nonce)}, {"object", object}, {"version", version},
		{"sha256", CloudSyncManifest::sha256(raw)}, {"bytes", double(raw.size())}, {"model_sha256", CloudSyncManifest::sha256(compactJson(logicalValue()))}};
	return pinValid(pin, reference()) ? pin : QJsonObject();
}
bool CloudWorkspaceRecord::archiveTo(const QJsonObject &verifiedPin)
{
	if (!valid() || !pinValid(verifiedPin, reference())) return false;
	auto pages = archivePages();
	for (const auto &entry : pages) if (entry.toObject().value("pin") == verifiedPin) return true;
	const auto raw = json();
	if (verifiedPin.value("bytes").toDouble() != raw.size() || text(verifiedPin, "sha256") != CloudSyncManifest::sha256(raw) ||
	    text(verifiedPin, "model_sha256") != CloudSyncManifest::sha256(compactJson(logicalValue()))) return false;
	pages.prepend(QJsonObject{{"pin", verifiedPin}, {"data", QString::fromLatin1(raw.toBase64())}});
	auto next = *this; next.value["segments"] = QJsonObject{{"base", logicalValue()}, {"archives", pages}};
	if (!next.valid() || !next.follows(*this)) return false;
	value = next.value; return true;
}
CloudWorkspaceRecord CloudWorkspaceRecord::decodePages(const QByteArray &bytes, const QJsonArray &pages)
{
	CloudWorkspaceRecord result;
	if (pages.isEmpty() || pages.size() > PageCountLimit || bytes.size() > WireLimit) return result;
	const auto top = QJsonDocument::fromJson(bytes).object();
	CloudWorkspaceReference ref; ref.value = top.value("workspace").toObject();
	const auto wire = wireObject(bytes, ref); if (wire.isEmpty()) return result;
	QList<QJsonObject> patches; QSet<QString> ids, names; qint64 total = 0;
	auto expected = wire.value("archive").toObject();
	for (int i = 0; i < pages.size(); ++i) {
		const auto page = pages[i].toObject(), pin = page.value("pin").toObject();
		if (!fields(page, {"pin", "data"}) || !page.value("data").isString() || pin != expected || !pinValid(pin, ref) ||
		    ids.contains(text(pin, "object")) || names.contains(text(pin, "name"))) return {};
		const auto encoded = page.value("data").toString().toLatin1();
		if (encoded.size() > ((WireLimit + 2) / 3) * 4) return {};
		const auto raw = QByteArray::fromBase64(encoded); total += raw.size();
		if (raw.toBase64() != encoded || total > ExpandedLimit || raw.size() != pin.value("bytes").toDouble() ||
		    CloudSyncManifest::sha256(raw) != text(pin, "sha256")) return {};
		ids.insert(text(pin, "object")); names.insert(text(pin, "name"));
		const auto previousWire = wireObject(raw, ref);
		if (previousWire.isEmpty()) {
			if (i != pages.size() - 1) return {};
			result = parse(raw);
			if (!result.valid() || result.reference().value != ref.value ||
			    CloudSyncManifest::sha256(compactJson(result.logicalValue())) != text(pin, "model_sha256")) return {};
		} else {
			if (i == pages.size() - 1) return {};
			patches.append(previousWire);
			expected = previousWire.value("archive").toObject();
		}
	}
	// Each archived delta must itself extend its verified predecessor. Checking
	// only the final state would hide an invalid intermediate safety transition.
	for (int i = patches.size() - 1; i >= 0; --i) {
		const auto &patch = patches[i]; const auto pin = patch.value("archive").toObject();
		if (text(pin, "model_sha256") != CloudSyncManifest::sha256(compactJson(result.logicalValue()))) return {};
		CloudWorkspaceRecord next; next.value = apply(result.logicalValue(), patch.value("changes").toObject());
		if (!next.validLogical(true) || !next.followsLogical(result, true)) return {};
		// The page containing this delta is pinned by the preceding page/top.
		const auto ownPin = pages[i].toObject().value("pin").toObject();
		if (text(ownPin, "model_sha256") != CloudSyncManifest::sha256(compactJson(next.logicalValue()))) return {};
		result = next;
	}
	if (text(wire.value("archive").toObject(), "model_sha256") != CloudSyncManifest::sha256(compactJson(result.logicalValue()))) return {};
	CloudWorkspaceRecord next; next.value = apply(result.logicalValue(), wire.value("changes").toObject());
	if (!next.validLogical(true) || !next.followsLogical(result, true)) return {};
	const auto tickets = next.value.value("journal").toObject().value("tickets").toObject();
	for (const auto &entry : tickets) if (ids.contains(text(entry.toObject(), "object"))) return {};
	next.value["segments"] = QJsonObject{{"base", result.logicalValue()}, {"archives", pages}};
	return next;
}
CloudWorkspaceRecord CloudWorkspaceRecord::parsePages(const QByteArray &bytes, const QJsonArray &pages)
{
	return pages.isEmpty() ? parse(bytes) : decodePages(bytes, pages);
}
bool CloudWorkspaceRecord::valid() const
{
	if (!validLogical(value.contains("segments"))) return false;
	if (!value.contains("segments")) return true;
	const auto metadata = value.value("segments").toObject();
	if (!value.value("segments").isObject() || !fields(metadata, {"base", "archives"}) ||
	    !metadata.value("base").isObject() || !metadata.value("archives").isArray()) return false;
	const auto checked = decodePages(json(), archivePages());
	return !checked.value.isEmpty() && checked.value == value;
}
bool CloudWorkspaceRecord::needsArchive() const
{
	if (!valid()) return false;
	if (json().size() >= WireLimit * 3 / 4) return true;
	const auto journal = value.value("journal").toObject();
	const auto current = value.contains("segments") ? delta(logicalValue(), value.value("segments").toObject().value("base").toObject()) :
		QJsonObject{{"journal", journal}, {"copies", value.value("copies")}, {"legacy_acceptances", value.value("legacy_acceptances")}, {"metadata_payloads", value.value("metadata_payloads")}};
	const auto changes = current.value("journal").toObject();
	return changes.value("tickets").toObject().size() >= 448 || changes.value("reservations").toObject().size() >= 448 ||
		current.value("copies").toObject().size() >= 448 || current.value("legacy_acceptances").toArray().size() >= 448 ||
		current.value("metadata_payloads").toObject().size() >= 448;
}
