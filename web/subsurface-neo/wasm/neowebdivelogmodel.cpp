// SPDX-License-Identifier: GPL-2.0
#include "neowebdivelogmodel.h"

#include "core/native-divelog-summary.h"
#include "core/native-profile-calculator.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>
#include <QVariantMap>
#include <QXmlStreamWriter>

#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

#ifdef __EMSCRIPTEN__
NeoWebDiveLogModel *webDiveLogModel = nullptr;
#endif

QString durationText(int seconds)
{
	const int minutes = qMax(0, seconds / 60);
	if (minutes >= 60)
		return QStringLiteral("%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
	return QStringLiteral("%1 min").arg(minutes);
}

QString depthText(double meters)
{
	return QStringLiteral("%1 m").arg(QLocale().toString(meters, 'f', 1));
}

QString temperatureText(double celsius)
{
	return QStringLiteral("%1 °C").arg(QLocale().toString(celsius, 'f', 1));
}

QString localPathForUrl(const QUrl &url)
{
	const QString local = url.toLocalFile();
	return local.isEmpty() ? url.path() : local;
}

QString csvField(QString value)
{
	value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
	return QStringLiteral("\"") + value + QStringLiteral("\"");
}

#ifdef __EMSCRIPTEN__
void downloadBrowserText(const QString &name, const QString &mimeType, const QString &content)
{
	const QByteArray fileName = name.toUtf8();
	const QByteArray mime = mimeType.toUtf8();
	const QByteArray bytes = content.toUtf8();
	EM_ASM({
		const name = UTF8ToString($0);
		const mime = UTF8ToString($1);
		const content = UTF8ToString($2, $3);
		const url = URL.createObjectURL(new Blob([content], { type: mime }));
		const anchor = document.createElement('a');
		anchor.href = url;
		anchor.download = name;
		document.body.appendChild(anchor);
		anchor.click();
		anchor.remove();
		setTimeout(() => URL.revokeObjectURL(url), 0);
	}, fileName.constData(), mime.constData(), bytes.constData(), bytes.size());
}
#endif

QVariantMap diveMap(const native_dive_summary &dive, int sourceIndex)
{
	QVariantMap item;
	item.insert(QStringLiteral("sourceIndex"), sourceIndex);
	item.insert(QStringLiteral("number"), dive.number);
	item.insert(QStringLiteral("date"), dive.when.isValid() ?
		QLocale().toString(dive.when.date(), QLocale::ShortFormat) : QObject::tr("Date unavailable"));
	item.insert(QStringLiteral("time"), dive.when.isValid() ?
		QLocale().toString(dive.when.time(), QLocale::ShortFormat) : QString());
	item.insert(QStringLiteral("location"), dive.location.isEmpty() ? QObject::tr("Unnamed dive site") : dive.location);
	item.insert(QStringLiteral("duration"), durationText(dive.duration_seconds));
	item.insert(QStringLiteral("durationSeconds"), dive.duration_seconds);
	item.insert(QStringLiteral("depth"), dive.has_max_depth ? depthText(dive.max_depth_m) : QStringLiteral("—"));
	item.insert(QStringLiteral("depthMeters"), dive.has_max_depth ? dive.max_depth_m : 0.0);
	item.insert(QStringLiteral("temperature"), dive.has_water_temperature ?
		temperatureText(dive.water_temperature_c) : QStringLiteral("—"));
	item.insert(QStringLiteral("temperatureCelsius"), dive.has_water_temperature ? dive.water_temperature_c : 0.0);
	item.insert(QStringLiteral("gas"), dive.gas);
	item.insert(QStringLiteral("mode"), dive.mode);
	item.insert(QStringLiteral("gear"), dive.gear);
	item.insert(QStringLiteral("buddy"), dive.buddy);
	item.insert(QStringLiteral("notes"), dive.notes);
	item.insert(QStringLiteral("suit"), dive.suit);
	item.insert(QStringLiteral("sampleCount"), dive.samples.size());
	return item;
}

QVariantMap sampleMap(const native_sample_summary &sample)
{
	QVariantMap item;
	item.insert(QStringLiteral("timeSeconds"), sample.time_seconds);
	item.insert(QStringLiteral("timeMinutes"), sample.time_seconds / 60.0);
	item.insert(QStringLiteral("depthMeters"), sample.depth_m);
	item.insert(QStringLiteral("temperatureCelsius"), sample.temperature_c);
	item.insert(QStringLiteral("pressureBar"), sample.pressure_bar);
	item.insert(QStringLiteral("ndlSeconds"), sample.ndl_seconds);
	item.insert(QStringLiteral("ndlMinutes"), sample.ndl_seconds / 60.0);
	item.insert(QStringLiteral("ttsSeconds"), sample.tts_seconds);
	item.insert(QStringLiteral("ttsMinutes"), sample.tts_seconds / 60.0);
	item.insert(QStringLiteral("stopDepthMeters"), sample.stop_depth_m);
	item.insert(QStringLiteral("stopTimeSeconds"), sample.stop_time_seconds);
	item.insert(QStringLiteral("cnsPercent"), sample.cns_percent);
	item.insert(QStringLiteral("setpointBar"), sample.setpoint_bar);
	item.insert(QStringLiteral("hasDepth"), sample.has_depth);
	item.insert(QStringLiteral("hasTemperature"), sample.has_temperature);
	item.insert(QStringLiteral("hasPressure"), sample.has_pressure);
	item.insert(QStringLiteral("hasNdl"), sample.has_ndl);
	item.insert(QStringLiteral("hasTts"), sample.has_tts);
	item.insert(QStringLiteral("hasStopDepth"), sample.has_stop_depth);
	item.insert(QStringLiteral("hasStopTime"), sample.has_stop_time);
	item.insert(QStringLiteral("hasCns"), sample.has_cns);
	item.insert(QStringLiteral("hasSetpoint"), sample.has_setpoint);
	item.insert(QStringLiteral("inDeco"), sample.in_deco);
	item.insert(QStringLiteral("currentGfPercent"), sample.current_gf_percent);
	item.insert(QStringLiteral("surfaceGfPercent"), sample.surface_gf_percent);
	item.insert(QStringLiteral("calculatedCeilingMeters"), sample.calculated_ceiling_m);
	item.insert(QStringLiteral("calculatedNdlSeconds"), sample.calculated_ndl_seconds);
	item.insert(QStringLiteral("calculatedNdlMinutes"), sample.calculated_ndl_seconds / 60.0);
	item.insert(QStringLiteral("calculatedTtsSeconds"), sample.calculated_tts_seconds);
	item.insert(QStringLiteral("calculatedTtsMinutes"), sample.calculated_tts_seconds / 60.0);
	item.insert(QStringLiteral("displayNdlMinutes"), sample.has_calculated_ndl ?
		sample.calculated_ndl_seconds / 60.0 : sample.ndl_seconds / 60.0);
	item.insert(QStringLiteral("hasDisplayNdl"), sample.has_calculated_ndl || sample.has_ndl);
	item.insert(QStringLiteral("hasCurrentGf"), sample.has_current_gf);
	item.insert(QStringLiteral("hasSurfaceGf"), sample.has_surface_gf);
	item.insert(QStringLiteral("hasCalculatedCeiling"), sample.has_calculated_ceiling);
	item.insert(QStringLiteral("hasCalculatedNdl"), sample.has_calculated_ndl);
	item.insert(QStringLiteral("hasCalculatedTts"), sample.has_calculated_tts);
	return item;
}

} // namespace

NeoWebDiveLogModel::NeoWebDiveLogModel(QObject *parent) : QObject(parent)
{
#ifdef __EMSCRIPTEN__
	webDiveLogModel = this;
	EM_ASM({
		if (!('indexedDB' in window)) {
			_neo_web_session_state(0, 0);
			return;
		}
		const request = indexedDB.open('subsurface-neo', 1);
		request.onupgradeneeded = () => {
			const database = request.result;
			if (!database.objectStoreNames.contains('sessions'))
				database.createObjectStore('sessions', { keyPath: 'key' });
		};
		request.onerror = () => _neo_web_session_state(0, 0);
		request.onsuccess = () => {
			const database = request.result;
			const transaction = database.transaction('sessions', 'readonly');
			const getRequest = transaction.objectStore('sessions').get('current-native-xml');
			getRequest.onerror = () => _neo_web_session_state(1, 0);
			getRequest.onsuccess = () => _neo_web_session_state(1, getRequest.result ? 1 : 0);
		};
	});
#endif
}

int NeoWebDiveLogModel::diveCount() const
{
	return m_diveCount;
}

QString NeoWebDiveLogModel::totalTime() const
{
	const double hours = m_totalSeconds / 3600.0;
	return QStringLiteral("%1 h").arg(QLocale().toString(hours, 'f', 1));
}

QString NeoWebDiveLogModel::maxDepth() const
{
	return m_diveCount > 0 ? depthText(m_maxDepthMeters) : QStringLiteral("—");
}

QVariantList NeoWebDiveLogModel::recentDives() const
{
	return m_recentDives;
}

QVariantList NeoWebDiveLogModel::filteredDives() const
{
	return m_filteredDives;
}

QStringList NeoWebDiveLogModel::availableYears() const
{
	return m_availableYears;
}

QStringList NeoWebDiveLogModel::availableModes() const
{
	return m_availableModes;
}

QString NeoWebDiveLogModel::searchText() const
{
	return m_searchText;
}

QString NeoWebDiveLogModel::yearFilter() const
{
	return m_yearFilter;
}

QString NeoWebDiveLogModel::modeFilter() const
{
	return m_modeFilter;
}

QVariantMap NeoWebDiveLogModel::selectedDive() const
{
	return m_selectedDive;
}

QVariantList NeoWebDiveLogModel::profileSamples() const
{
	return m_profileSamples;
}

bool NeoWebDiveLogModel::hasSelectedDive() const
{
	return !m_selectedDive.isEmpty();
}

QString NeoWebDiveLogModel::fileStatus() const
{
	return m_fileStatus;
}

bool NeoWebDiveLogModel::loaded() const
{
	return m_loaded;
}

bool NeoWebDiveLogModel::error() const
{
	return m_error;
}

bool NeoWebDiveLogModel::selectedDiveDirty() const
{
	return m_selectedDiveDirty;
}

bool NeoWebDiveLogModel::browserStorageAvailable() const
{
	return m_browserStorageAvailable;
}

bool NeoWebDiveLogModel::durableSessionStored() const
{
	return m_durableSessionStored;
}

QString NeoWebDiveLogModel::durableSessionStatus() const
{
	return m_durableSessionStatus;
}

void NeoWebDiveLogModel::chooseLocalFile()
{
#ifdef __EMSCRIPTEN__
	EM_ASM({
		const input = document.createElement('input');
		input.type = 'file';
		input.accept = '.xml,.ssrf,text/xml,application/xml';
		input.style.display = 'none';
		document.body.appendChild(input);
		input.addEventListener('change', async () => {
			try {
				const file = input.files && input.files[0];
				if (!file)
					return;
				if (file.size > 64 * 1024 * 1024)
					throw new Error(file.name + ' is larger than the 64 MB browser import limit.');
				const safeName = file.name.replace(/[^A-Za-z0-9._-]/g, '_') || 'divelog.ssrf';
				const directory = '/tmp/neo-import-' + Date.now();
				const path = directory + '/' + safeName;
				FS.mkdirTree(directory);
				FS.writeFile(path, new Uint8Array(await file.arrayBuffer()));
				const size = lengthBytesUTF8(path) + 1;
				const pointer = _malloc(size);
				stringToUTF8(path, pointer, size);
				_neo_web_file_selected(pointer);
				_free(pointer);
			} catch (error) {
				const message = error instanceof Error ? error.message : String(error);
				const size = lengthBytesUTF8(message) + 1;
				const pointer = _malloc(size);
				stringToUTF8(message, pointer, size);
				_neo_web_file_error(pointer);
				_free(pointer);
			} finally {
				input.remove();
			}
		}, { once: true });
		input.click();
	});
#else
	setBrowserFileError(tr("Local browser file selection is available in the WebAssembly build."));
#endif
}

void NeoWebDiveLogModel::setBrowserFileError(const QString &message)
{
	m_error = true;
	m_loaded = false;
	m_fileStatus = message;
	emit changed();
}

void NeoWebDiveLogModel::selectDive(int sourceIndex)
{
	if (sourceIndex < 0 || sourceIndex >= m_summary.dives.size())
		return;
	QString profileError;
	calculate_native_profile(m_summary, sourceIndex, &profileError);
	const native_dive_summary &dive = m_summary.dives.at(sourceIndex);
	m_selectedDive = diveMap(dive, sourceIndex);
	m_profileSamples.clear();
	m_profileSamples.reserve(dive.samples.size());
	for (const native_sample_summary &sample : dive.samples)
		m_profileSamples.push_back(sampleMap(sample));
	emit changed();
}

void NeoWebDiveLogModel::clearSelectedDive()
{
	if (m_selectedDive.isEmpty())
		return;
	m_selectedDive.clear();
	m_profileSamples.clear();
	emit changed();
}

bool NeoWebDiveLogModel::updateSelectedDive(const QString &location, const QString &buddy, const QString &notes,
	const QString &mode, const QString &gas, const QString &gear)
{
	const int sourceIndex = m_selectedDive.value(QStringLiteral("sourceIndex"), -1).toInt();
	if (sourceIndex < 0 || sourceIndex >= m_summary.dives.size())
		return false;
	native_dive_summary &dive = m_summary.dives[sourceIndex];
	dive.location = location.trimmed();
	dive.buddy = buddy.trimmed();
	dive.notes = notes.trimmed();
	dive.mode = mode.trimmed().isEmpty() ? QStringLiteral("OC") : mode.trimmed();
	dive.gas = gas.trimmed();
	dive.gear = gear.trimmed();
	m_selectedDive = diveMap(dive, sourceIndex);
	m_selectedDiveDirty = true;
	rebuildDiveLists();
	emit changed();
#ifdef __EMSCRIPTEN__
	saveBrowserSession();
#endif
	return true;
}

QString NeoWebDiveLogModel::selectedDiveJson() const
{
	return m_selectedDive.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument::fromVariant(m_selectedDive).toJson(QJsonDocument::Indented));
}

QString NeoWebDiveLogModel::diveListCsv() const
{
	QStringList rows{QStringLiteral("number,date,time,location,duration,max_depth,water_temperature,gas,mode,gear,buddy,notes")};
	for (const native_dive_summary &dive : m_summary.dives) {
		const QVariantMap item = diveMap(dive, 0);
		rows.push_back(QStringList{
			QString::number(dive.number), csvField(item.value(QStringLiteral("date")).toString()),
			csvField(item.value(QStringLiteral("time")).toString()), csvField(dive.location),
			csvField(item.value(QStringLiteral("duration")).toString()), csvField(item.value(QStringLiteral("depth")).toString()),
			csvField(item.value(QStringLiteral("temperature")).toString()), csvField(dive.gas), csvField(dive.mode),
			csvField(dive.gear), csvField(dive.buddy), csvField(dive.notes)
		}.join(QLatin1Char(',')));
	}
	return rows.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString NeoWebDiveLogModel::nativeXmlBackup() const
{
	QString output;
	QXmlStreamWriter writer(&output);
	writer.setAutoFormatting(true);
	writer.writeStartDocument();
	writer.writeStartElement(QStringLiteral("divelog"));
	writer.writeAttribute(QStringLiteral("program"), QStringLiteral("subsurface"));
	writer.writeAttribute(QStringLiteral("version"), QStringLiteral("3"));
	writer.writeStartElement(QStringLiteral("dives"));
	for (const native_dive_summary &dive : m_summary.dives) {
		writer.writeStartElement(QStringLiteral("dive"));
		writer.writeAttribute(QStringLiteral("number"), QString::number(dive.number));
		if (dive.when.isValid()) {
			writer.writeAttribute(QStringLiteral("date"), dive.when.date().toString(Qt::ISODate));
			writer.writeAttribute(QStringLiteral("time"), dive.when.time().toString(QStringLiteral("HH:mm:ss")));
		}
		writer.writeAttribute(QStringLiteral("duration"), QStringLiteral("%1:%2 min").arg(dive.duration_seconds / 60).arg(dive.duration_seconds % 60, 2, 10, QLatin1Char('0')));
		if (!dive.location.isEmpty()) writer.writeTextElement(QStringLiteral("location"), dive.location);
		if (!dive.buddy.isEmpty()) writer.writeTextElement(QStringLiteral("buddy"), dive.buddy);
		if (!dive.notes.isEmpty()) writer.writeTextElement(QStringLiteral("notes"), dive.notes);
		if (!dive.suit.isEmpty()) writer.writeTextElement(QStringLiteral("suit"), dive.suit);
		writer.writeStartElement(QStringLiteral("cylinder"));
		writer.writeAttribute(QStringLiteral("description"), dive.gear);
		if (dive.gas.startsWith(QStringLiteral("EAN")))
			writer.writeAttribute(QStringLiteral("o2"), dive.gas.mid(3) + QLatin1Char('%'));
		else
			writer.writeAttribute(QStringLiteral("o2"), QStringLiteral("21%"));
		writer.writeEndElement();
		writer.writeStartElement(QStringLiteral("divecomputer"));
		writer.writeAttribute(QStringLiteral("dctype"), dive.mode);
		for (const native_sample_summary &sample : dive.samples) {
			writer.writeStartElement(QStringLiteral("sample"));
			writer.writeAttribute(QStringLiteral("time"), QStringLiteral("%1:%2 min").arg(sample.time_seconds / 60).arg(sample.time_seconds % 60, 2, 10, QLatin1Char('0')));
			if (sample.has_depth) writer.writeAttribute(QStringLiteral("depth"), QString::number(sample.depth_m, 'f', 2) + QStringLiteral(" m"));
			if (sample.has_temperature) writer.writeAttribute(QStringLiteral("temp"), QString::number(sample.temperature_c, 'f', 1) + QStringLiteral(" C"));
			if (sample.has_pressure) writer.writeAttribute(QStringLiteral("pressure0"), QString::number(sample.pressure_bar, 'f', 1) + QStringLiteral(" bar"));
			if (sample.has_ndl) writer.writeAttribute(QStringLiteral("ndl"), QStringLiteral("%1:%2 min").arg(sample.ndl_seconds / 60).arg(sample.ndl_seconds % 60, 2, 10, QLatin1Char('0')));
			writer.writeEndElement();
		}
		writer.writeEndElement();
		writer.writeEndElement();
	}
	writer.writeEndElement();
	writer.writeEndElement();
	writer.writeEndDocument();
	return output;
}

void NeoWebDiveLogModel::exportSelectedDiveJson()
{
#ifdef __EMSCRIPTEN__
	if (!m_selectedDive.isEmpty())
		downloadBrowserText(QStringLiteral("subsurface-neo-dive.json"), QStringLiteral("application/json"), selectedDiveJson());
#endif
}

void NeoWebDiveLogModel::exportDiveListCsv()
{
#ifdef __EMSCRIPTEN__
	if (m_loaded)
		downloadBrowserText(QStringLiteral("subsurface-neo-dives.csv"), QStringLiteral("text/csv;charset=utf-8"), diveListCsv());
#endif
}

void NeoWebDiveLogModel::exportNativeXmlBackup()
{
#ifdef __EMSCRIPTEN__
	if (m_loaded)
		downloadBrowserText(QStringLiteral("subsurface-neo-browser-backup.xml"), QStringLiteral("application/xml"), nativeXmlBackup());
#endif
}

void NeoWebDiveLogModel::saveBrowserSession()
{
#ifdef __EMSCRIPTEN__
	if (!m_loaded)
		return;
	m_durableSessionStatus = tr("Saving this dive-log workspace in browser storage…");
	emit changed();
	const QByteArray xml = nativeXmlBackup().toUtf8();
	EM_ASM({
		const xml = UTF8ToString($0, $1);
		const request = indexedDB.open('subsurface-neo', 1);
		request.onupgradeneeded = () => {
			const database = request.result;
			if (!database.objectStoreNames.contains('sessions'))
				database.createObjectStore('sessions', { keyPath: 'key' });
		};
		request.onerror = () => _neo_web_session_saved(0);
		request.onsuccess = () => {
			const database = request.result;
			const transaction = database.transaction('sessions', 'readwrite');
			transaction.oncomplete = () => _neo_web_session_saved(1);
			transaction.onerror = () => _neo_web_session_saved(0);
			transaction.objectStore('sessions').put({
				key: 'current-native-xml',
				xml,
				savedAt: new Date().toISOString()
			});
		};
	}, xml.constData(), xml.size());
#else
	m_durableSessionStatus = tr("Durable browser storage is available only in the WebAssembly build.");
	emit changed();
#endif
}

void NeoWebDiveLogModel::restoreBrowserSession()
{
#ifdef __EMSCRIPTEN__
	m_durableSessionStatus = tr("Restoring the saved browser workspace…");
	emit changed();
	EM_ASM({
		const request = indexedDB.open('subsurface-neo', 1);
		request.onerror = () => _neo_web_session_state(0, 0);
		request.onsuccess = () => {
			const database = request.result;
			const transaction = database.transaction('sessions', 'readonly');
			const getRequest = transaction.objectStore('sessions').get('current-native-xml');
			getRequest.onerror = () => _neo_web_session_state(1, 0);
			getRequest.onsuccess = () => {
				const record = getRequest.result;
				if (!record || typeof record.xml !== 'string') {
					_neo_web_session_state(1, 0);
					return;
				}
				FS.mkdirTree('/tmp');
				const path = '/tmp/neo-restored-' + Date.now() + '.xml';
				FS.writeFile(path, new TextEncoder().encode(record.xml));
				const pathSize = lengthBytesUTF8(path) + 1;
				const pathPointer = _malloc(pathSize);
				stringToUTF8(path, pathPointer, pathSize);
				const savedAt = record.savedAt || '';
				const savedAtSize = lengthBytesUTF8(savedAt) + 1;
				const savedAtPointer = _malloc(savedAtSize);
				stringToUTF8(savedAt, savedAtPointer, savedAtSize);
				_neo_web_session_restored(pathPointer, savedAtPointer);
				_free(pathPointer);
				_free(savedAtPointer);
			};
		};
	});
#else
	m_durableSessionStatus = tr("Durable browser storage is available only in the WebAssembly build.");
	emit changed();
#endif
}

void NeoWebDiveLogModel::clearBrowserSession()
{
#ifdef __EMSCRIPTEN__
	EM_ASM({
		const request = indexedDB.open('subsurface-neo', 1);
		request.onerror = () => _neo_web_session_cleared(0);
		request.onsuccess = () => {
			const transaction = request.result.transaction('sessions', 'readwrite');
			transaction.oncomplete = () => _neo_web_session_cleared(1);
			transaction.onerror = () => _neo_web_session_cleared(0);
			transaction.objectStore('sessions').delete('current-native-xml');
		};
	});
#else
	m_durableSessionStatus = tr("Durable browser storage is available only in the WebAssembly build.");
	emit changed();
#endif
}

void NeoWebDiveLogModel::setBrowserSessionState(bool available, bool stored)
{
	m_browserStorageAvailable = available;
	m_durableSessionStored = stored;
	if (!available)
		m_durableSessionStatus = tr("This browser does not provide durable IndexedDB storage.");
	else if (stored)
		m_durableSessionStatus = tr("A saved dive-log workspace is available on this device.");
	else
		m_durableSessionStatus = tr("No dive-log workspace is saved in this browser yet.");
	emit changed();
}

void NeoWebDiveLogModel::browserSessionSaved(bool success)
{
	m_browserStorageAvailable = success || m_browserStorageAvailable;
	m_durableSessionStored = success;
	m_durableSessionStatus = success ?
		tr("Browser workspace saved. Later edits will be saved automatically.") :
		tr("The browser workspace could not be saved.");
	emit changed();
}

void NeoWebDiveLogModel::browserSessionCleared(bool success)
{
	if (success)
		m_durableSessionStored = false;
	m_durableSessionStatus = success ? tr("Saved browser workspace removed.") :
		tr("The saved browser workspace could not be removed.");
	emit changed();
}

void NeoWebDiveLogModel::openRestoredBrowserSession(const QString &path, const QString &savedAt)
{
	openLocalFile(QUrl::fromLocalFile(path));
	QFile::remove(path);
	if (m_loaded) {
		m_browserStorageAvailable = true;
		m_durableSessionStored = true;
		m_fileStatus = savedAt.isEmpty() ? tr("Restored the saved browser workspace.") :
			tr("Restored the browser workspace saved at %1.").arg(savedAt);
		m_durableSessionStatus = tr("Edits to this workspace are saved automatically on this device.");
		emit changed();
	}
}

void NeoWebDiveLogModel::setSearchText(const QString &searchText)
{
	const QString normalized = searchText.trimmed();
	if (m_searchText == normalized)
		return;
	m_searchText = normalized;
	rebuildDiveLists();
	emit changed();
}

void NeoWebDiveLogModel::setYearFilter(const QString &yearFilter)
{
	if (m_yearFilter == yearFilter)
		return;
	m_yearFilter = yearFilter;
	rebuildDiveLists();
	emit changed();
}

void NeoWebDiveLogModel::setModeFilter(const QString &modeFilter)
{
	if (m_modeFilter == modeFilter)
		return;
	m_modeFilter = modeFilter;
	rebuildDiveLists();
	emit changed();
}

void NeoWebDiveLogModel::rebuildDiveLists()
{
	m_recentDives.clear();
	m_filteredDives.clear();
	for (int index = m_summary.dives.size() - 1; index >= 0; --index) {
		const native_dive_summary &dive = m_summary.dives.at(index);
		const QVariantMap mapped = diveMap(dive, index);
		if (m_recentDives.size() < 5)
			m_recentDives.push_back(mapped);

		if (!m_yearFilter.isEmpty() && (!dive.when.isValid() || QString::number(dive.when.date().year()) != m_yearFilter))
			continue;
		if (!m_modeFilter.isEmpty() && dive.mode.compare(m_modeFilter, Qt::CaseInsensitive) != 0)
			continue;
		if (!m_searchText.isEmpty()) {
			const QString searchable = QStringLiteral("%1 %2 %3 %4 %5 %6 %7 %8")
				.arg(dive.number)
				.arg(dive.location, dive.buddy, dive.notes, dive.gas, dive.gear, dive.suit, dive.mode);
			if (!searchable.contains(m_searchText, Qt::CaseInsensitive))
				continue;
		}
		m_filteredDives.push_back(mapped);
	}
}

void NeoWebDiveLogModel::openLocalFile(const QUrl &url)
{
	const QString localPath = localPathForUrl(url);
	QFile file(localPath);
	const QFileInfo info(file);
	if (info.size() > 64 * 1024 * 1024) {
		m_error = true;
		m_loaded = false;
		m_fileStatus = tr("%1 is larger than the 64 MB browser import limit.").arg(info.fileName());
		emit changed();
		return;
	}
	if (!file.open(QIODevice::ReadOnly)) {
		m_error = true;
		m_loaded = false;
		m_fileStatus = tr("The selected file could not be opened.");
		emit changed();
		return;
	}

	const native_divelog_summary summary = read_native_divelog_summary(file, info.fileName());
	if (!summary.ok) {
		m_error = true;
		m_loaded = false;
		m_fileStatus = summary.error;
		emit changed();
		return;
	}

	m_summary = summary;
	m_selectedDive.clear();
	m_selectedDiveDirty = false;
	m_profileSamples.clear();
	m_diveCount = m_summary.dives.size();
	m_totalSeconds = 0;
	m_maxDepthMeters = 0.0;
	m_recentDives.clear();
	m_filteredDives.clear();
	m_availableYears.clear();
	m_availableModes.clear();
	QSet<int> years;
	QSet<QString> modes;
	for (const native_dive_summary &dive : m_summary.dives) {
		m_totalSeconds += dive.duration_seconds;
		if (dive.has_max_depth)
			m_maxDepthMeters = std::max(m_maxDepthMeters, dive.max_depth_m);
		if (dive.when.isValid())
			years.insert(dive.when.date().year());
		if (!dive.mode.trimmed().isEmpty())
			modes.insert(dive.mode.trimmed());
	}
	QList<int> sortedYears = years.values();
	std::sort(sortedYears.begin(), sortedYears.end(), std::greater<int>());
	for (int year : sortedYears)
		m_availableYears.push_back(QString::number(year));
	m_availableModes = modes.values();
	m_availableModes.sort(Qt::CaseInsensitive);
	if (!m_availableYears.contains(m_yearFilter))
		m_yearFilter.clear();
	if (!m_availableModes.contains(m_modeFilter, Qt::CaseInsensitive))
		m_modeFilter.clear();
	rebuildDiveLists();

	m_error = false;
	m_loaded = true;
	m_fileStatus = tr("Loaded %1 dives from %2 using Subsurface's native XML reader.")
		.arg(m_diveCount)
		.arg(info.fileName());
	emit changed();
}

#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void neo_web_file_selected(const char *path)
{
	if (!webDiveLogModel || !path)
		return;
	const QString localPath = QString::fromUtf8(path);
	webDiveLogModel->openLocalFile(QUrl::fromLocalFile(localPath));
	QFile::remove(localPath);
}

extern "C" EMSCRIPTEN_KEEPALIVE void neo_web_file_error(const char *message)
{
	if (webDiveLogModel)
		webDiveLogModel->setBrowserFileError(QString::fromUtf8(message ? message : "Browser file import failed."));
}

extern "C" EMSCRIPTEN_KEEPALIVE void neo_web_session_state(int available, int stored)
{
	if (webDiveLogModel)
		webDiveLogModel->setBrowserSessionState(available != 0, stored != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE void neo_web_session_saved(int success)
{
	if (webDiveLogModel)
		webDiveLogModel->browserSessionSaved(success != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE void neo_web_session_cleared(int success)
{
	if (webDiveLogModel)
		webDiveLogModel->browserSessionCleared(success != 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE void neo_web_session_restored(const char *path, const char *savedAt)
{
	if (webDiveLogModel && path)
		webDiveLogModel->openRestoredBrowserSession(QString::fromUtf8(path), QString::fromUtf8(savedAt ? savedAt : ""));
}
#endif
