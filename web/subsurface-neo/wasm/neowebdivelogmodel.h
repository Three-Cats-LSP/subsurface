// SPDX-License-Identifier: GPL-2.0
#ifndef NEO_WEB_DIVELOG_MODEL_H
#define NEO_WEB_DIVELOG_MODEL_H

#include "core/native-divelog-summary.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class NeoWebDiveLogModel : public QObject {
	Q_OBJECT
	Q_PROPERTY(int diveCount READ diveCount NOTIFY changed)
	Q_PROPERTY(QString totalTime READ totalTime NOTIFY changed)
	Q_PROPERTY(QString maxDepth READ maxDepth NOTIFY changed)
	Q_PROPERTY(QVariantList recentDives READ recentDives NOTIFY changed)
	Q_PROPERTY(QVariantList filteredDives READ filteredDives NOTIFY changed)
	Q_PROPERTY(QStringList availableYears READ availableYears NOTIFY changed)
	Q_PROPERTY(QStringList availableModes READ availableModes NOTIFY changed)
	Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY changed)
	Q_PROPERTY(QString yearFilter READ yearFilter WRITE setYearFilter NOTIFY changed)
	Q_PROPERTY(QString modeFilter READ modeFilter WRITE setModeFilter NOTIFY changed)
	Q_PROPERTY(QVariantMap selectedDive READ selectedDive NOTIFY changed)
	Q_PROPERTY(QVariantList profileSamples READ profileSamples NOTIFY changed)
	Q_PROPERTY(bool hasSelectedDive READ hasSelectedDive NOTIFY changed)
	Q_PROPERTY(QString fileStatus READ fileStatus NOTIFY changed)
	Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
	Q_PROPERTY(bool error READ error NOTIFY changed)
	Q_PROPERTY(bool selectedDiveDirty READ selectedDiveDirty NOTIFY changed)

public:
	explicit NeoWebDiveLogModel(QObject *parent = nullptr);

	int diveCount() const;
	QString totalTime() const;
	QString maxDepth() const;
	QVariantList recentDives() const;
	QVariantList filteredDives() const;
	QStringList availableYears() const;
	QStringList availableModes() const;
	QString searchText() const;
	QString yearFilter() const;
	QString modeFilter() const;
	QVariantMap selectedDive() const;
	QVariantList profileSamples() const;
	bool hasSelectedDive() const;
	QString fileStatus() const;
	bool loaded() const;
	bool error() const;
	bool selectedDiveDirty() const;

	Q_INVOKABLE void chooseLocalFile();
	Q_INVOKABLE void openLocalFile(const QUrl &url);
	Q_INVOKABLE void selectDive(int sourceIndex);
	Q_INVOKABLE void clearSelectedDive();
	Q_INVOKABLE bool updateSelectedDive(const QString &location, const QString &buddy, const QString &notes,
		const QString &mode, const QString &gas, const QString &gear);
	Q_INVOKABLE QString selectedDiveJson() const;
	Q_INVOKABLE QString diveListCsv() const;
	Q_INVOKABLE QString nativeXmlBackup() const;
	Q_INVOKABLE void exportSelectedDiveJson();
	Q_INVOKABLE void exportDiveListCsv();
	Q_INVOKABLE void exportNativeXmlBackup();
	void setSearchText(const QString &searchText);
	void setYearFilter(const QString &yearFilter);
	void setModeFilter(const QString &modeFilter);
	void setBrowserFileError(const QString &message);

signals:
	void changed();

private:
	int m_diveCount = 0;
	int m_totalSeconds = 0;
	double m_maxDepthMeters = 0.0;
	QVariantList m_recentDives;
	QVariantList m_filteredDives;
	QStringList m_availableYears;
	QStringList m_availableModes;
	QString m_searchText;
	QString m_yearFilter;
	QString m_modeFilter;
	QVariantMap m_selectedDive;
	QVariantList m_profileSamples;
	native_divelog_summary m_summary;
	QString m_fileStatus;
	bool m_loaded = false;
	bool m_error = false;
	bool m_selectedDiveDirty = false;

	void rebuildDiveLists();
};

#endif // NEO_WEB_DIVELOG_MODEL_H
