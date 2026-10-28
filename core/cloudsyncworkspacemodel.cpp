// SPDX-License-Identifier: GPL-2.0
#include "cloudsyncworkspacemodel.h"
#include "cloudsynchistory.h"
#include "cloudsyncmanifest.h"
#include "device.h"
#include "dive.h"
#include "divelog.h"
#include "fulltext.h"
#include "membuffer.h"
#include "parse.h"
#include "save-xml.h"
#include "selection.h"
#include "tag.h"
#include "version.h"
#include <QXmlStreamReader>

namespace {
QByteArray xml(struct divelog &log, const fingerprint_table &table)
{
	membuffer buffer; serialize_divelog_xml(&buffer, log, table);
	return QByteArray(buffer.buffer, int(buffer.len));
}
int diveCount(const QByteArray &bytes)
{
	QXmlStreamReader reader(bytes); int count = 0;
	while (!reader.atEnd()) { reader.readNext(); if (reader.isStartElement() && reader.name() == QLatin1String("dive")) ++count; }
	return reader.hasError() ? -1 : count;
}
}
struct CloudWorkspaceModel::Prepared {
	// Pointer-owning tables must outlive the staged log's references.
	fingerprint_table fingerprints;
	std::vector<std::unique_ptr<divetag>> tags;
	struct divelog log;
	int version = -1;
	QString previousHash;
	QString baselineHash;
	QByteArray normalized;
};
CloudWorkspaceModel::CloudWorkspaceModel() = default;
CloudWorkspaceModel::~CloudWorkspaceModel() = default;
QByteArray CloudWorkspaceModel::currentXml() { return xml(divelog, fingerprints); }
QByteArray CloudWorkspaceModel::normalizedXml() const { return prepared ? prepared->normalized : QByteArray(); }
bool CloudWorkspaceModel::prepare(const QByteArray &payload, const QByteArray &baseline)
{
	prepared.reset();
	if (payload.isEmpty() || payload.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(payload)) return false;
	const auto current = currentXml(), before = baseline.isEmpty() ? current : baseline;
	if (before.isEmpty() || before.size() > 64 * 1024 * 1024 || !CloudHistoryRecord::validXml(before)) return false;
	auto candidate = std::make_unique<Prepared>(); candidate->previousHash = CloudSyncManifest::sha256(current); candidate->baselineHash = CloudSyncManifest::sha256(before);
	{
		std::vector<std::unique_ptr<divetag>> baselineTags; struct divelog baselineLog; int version = -1;
		if (parse_xml_buffer_isolated(before.constData(), int(before.size()), &baselineLog, candidate->fingerprints, baselineTags, version) != 0 ||
		    version > dataformat_version || int(baselineLog.dives.size()) != diveCount(before)) return false;
	}
	if (parse_xml_buffer_isolated(payload.constData(), int(payload.size()), &candidate->log, candidate->fingerprints, candidate->tags, candidate->version) != 0) return false;
	if (candidate->version > dataformat_version || int(candidate->log.dives.size()) != diveCount(payload)) return false;
	candidate->log.prepare_loaded_dives();
	candidate->normalized = xml(candidate->log, candidate->fingerprints);
	if (!CloudHistoryRecord::validXml(candidate->normalized)) return false;
	prepared = std::move(candidate); return true;
}
bool CloudWorkspaceModel::install()
{
	const auto hash = CloudSyncManifest::sha256(currentXml());
	if (!prepared || prepared->previousHash != hash || prepared->baselineHash != hash) return false;
	// Preview remapping against a private copy of the shared tag catalog. An
	// existing translated-name collision can otherwise change the serialized
	// model only after replacing the live log. Refuse before any global mutation.
	{
		std::vector<std::unique_ptr<divetag>> preview;
		for (const auto &tag : g_tag_list) preview.push_back(std::make_unique<divetag>(tag->name, tag->source));
		std::vector<tag_list> previous;
		for (auto &d : prepared->log.dives) {
			previous.push_back(d->tags); tag_list mapped;
			for (const auto *tag : d->tags) taglist_add_tag(mapped, tag->source.empty() ? tag->name : tag->source, preview);
			d->tags = std::move(mapped);
		}
		const bool same = xml(prepared->log, prepared->fingerprints) == prepared->normalized;
		for (size_t i = 0; i < previous.size(); ++i) prepared->log.dives[i]->tags = std::move(previous[i]);
		if (!same) return false;
	}
	auto candidate = std::move(prepared);
	// Only an accepted model can add tags to the shared registry. Remap every
	// private pointer before its staging table is destroyed, keeping old tags.
	for (auto &d : candidate->log.dives) {
		tag_list tags;
		for (const auto *tag : d->tags) taglist_add_tag(tags, tag->source.empty() ? tag->name : tag->source);
		d->tags = std::move(tags);
	}
	fulltext_unregister_all(); select_single_dive(nullptr);
	fingerprints = std::move(candidate->fingerprints);
	last_xml_version = candidate->version; report_datafile_version(last_xml_version);
	divelog = std::move(candidate->log); divelog.process_loaded_dives(); return true;
}
