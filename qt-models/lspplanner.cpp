// SPDX-License-Identifier: GPL-2.0
#include "lspplanner.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJSValue>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

static void initLspResources()
{
	Q_INIT_RESOURCE(neo_lsp_planner);
}

namespace {
QByteArray resource(const QString &name)
{
	static const bool initialized = [] { initLspResources(); return true; }();
	Q_UNUSED(initialized)
	QFile file(QStringLiteral(":/lsp-planner/") + name);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QJsonObject failure(const QString &code, const QString &message)
{
	return {{"ok", false}, {"code", code}, {"error", message}};
}

// QJSEngine::setInterrupted is thread-safe; an event-loop timer alone cannot
// interrupt a synchronous calculation. Never leave a watchdog using a dead VM.
class CalculationDeadline {
public:
	explicit CalculationDeadline(QJSEngine &engine) : watcher([this, &engine] {
		std::unique_lock<std::mutex> lock(mutex);
		if (!wake.wait_for(lock, std::chrono::seconds(10), [this] { return finished; }))
			engine.setInterrupted(true);
	}) {}
	~CalculationDeadline()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			finished = true;
		}
		wake.notify_one();
		watcher.join();
	}
private:
	std::mutex mutex;
	std::condition_variable wake;
	bool finished = false;
	std::thread watcher;
};
}

QJsonObject LspPlanner::identity()
{
	const auto source = QJsonDocument::fromJson(resource(QStringLiteral("SOURCE.json"))).object();
	return {{"id", "lsp-d-planner-plus"}, {"version", source.value("version")},
		{"sourceCommit", source.value("commit")}, {"requestSchema", 1}};
}

QJsonArray LspPlanner::presets()
{
	const auto source = QJsonDocument::fromJson(resource(QStringLiteral("SOURCE.json"))).object();
	const auto data = resource(QStringLiteral("presets.json"));
	if (QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex() != source.value("presetsSha256").toString().toLatin1())
		return {};
	return QJsonDocument::fromJson(data).array();
}

static QJsonObject runLsp(const QJsonObject &request, const QString &method, int inputLimit)
{
	const QByteArray input = QJsonDocument(request).toJson(QJsonDocument::Compact);
	if (input.size() > inputLimit)
		return failure(QStringLiteral("INVALID_REQUEST"), QStringLiteral("LSP request exceeds the native input limit."));
	QJSEngine engine;
	CalculationDeadline deadline(engine);
	// Qt has ES5 global semantics. This aliases the isolated global object only.
	engine.evaluate(QStringLiteral("var globalThis = this;"));
	const auto hashes = QJsonDocument::fromJson(resource(QStringLiteral("qt-runtime/SOURCE.json"))).object().value("files").toObject();
	const QStringList scripts = {QStringLiteral("qt-runtime/zhl-engine-bundle.js"), QStringLiteral("host.js"),
		QStringLiteral("qt-runtime/schedule-runner-core.js"), QStringLiteral("qt-runtime/zhl-headless-adapter.js"),
		QStringLiteral("qt-runtime/vpm-engine-bundle.js"), QStringLiteral("projection.js")};
	for (const auto &name : scripts) {
		const auto source = resource(name);
		if (source.isEmpty())
			return failure(QStringLiteral("ENGINE_RESOURCE"), QStringLiteral("Missing bundled LSP source: ") + name);
		if (name.startsWith(QLatin1String("qt-runtime/"))) {
			const auto expected = hashes.value(name.mid(11)).toString().toLatin1();
			if (expected.size() != 64 || QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex() != expected)
				return failure(QStringLiteral("ENGINE_RESOURCE"), QStringLiteral("Bundled LSP checksum mismatch: ") + name);
		}
		const auto evaluated = engine.evaluate(QString::fromUtf8(source), name);
		if (evaluated.isError())
			return failure(QStringLiteral("ENGINE_LOAD"), name + QStringLiteral(": ") + evaluated.toString());
	}
	const auto object = engine.globalObject().property(QStringLiteral("NeoLspPlanner"));
	auto function = object.property(method);
	// QVariant sequences can be exposed as Qt sequence wrappers rather than
	// genuine JavaScript arrays on some Qt versions. Use the same plain JSON
	// objects/arrays as the upstream engine's request contract on every target.
	auto parse = engine.globalObject().property(QStringLiteral("JSON")).property(QStringLiteral("parse"));
	const auto argument = parse.call({QJSValue(QString::fromUtf8(input))});
	if (argument.isError())
		return failure(QStringLiteral("INVALID_REQUEST"), argument.toString());
	const auto result = function.callWithInstance(object, {argument});
	if (engine.isInterrupted())
		return failure(QStringLiteral("ENGINE_TIMEOUT"), QStringLiteral("LSP calculation exceeded the native deadline."));
	if (result.isError() || !result.isObject())
		return failure(QStringLiteral("ENGINE_ERROR"), result.toString());
	auto stringify = engine.globalObject().property(QStringLiteral("JSON")).property(QStringLiteral("stringify"));
	const auto encoded = stringify.call({result});
	const auto wire = encoded.toString().toUtf8();
	const auto raw = QJsonDocument::fromJson(wire).object();
	if (encoded.isError() || engine.isInterrupted() || raw.isEmpty() || wire.size() > 4 * 1024 * 1024)
		return failure(QStringLiteral("INVALID_RESULT"), QStringLiteral("LSP result exceeds the native output limit."));
	const bool ok = raw.value("error").isNull() && (raw.contains("plan") || raw.contains("profile"));
	return {{"ok", ok}, {"engine", LspPlanner::identity()}, {"model", request.value("model")},
		{"requestSha256", QString::fromLatin1(QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex())},
		{"effectiveRequest", request}, {"result", raw}};
}

QJsonObject LspPlanner::calculate(const QJsonObject &request)
{
	return runLsp(request, QStringLiteral("calculate"), 64 * 1024);
}

QJsonObject LspPlanner::project(const QJsonObject &input)
{
	return runLsp(input, QStringLiteral("project"), 4 * 1024 * 1024);
}
