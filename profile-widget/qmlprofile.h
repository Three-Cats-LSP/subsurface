// SPDX-License-Identifier: GPL-2.0
#ifndef QMLPROFILE_H
#define QMLPROFILE_H

#include "core/subsurface-qt/divelistnotifier.h"
#include <QQuickPaintedItem>
#include <QVariantMap>
#include <memory>

class ProfileScene;
struct divecomputer;

class QMLProfile : public QQuickPaintedItem
{
	Q_OBJECT
	Q_PROPERTY(int diveId MEMBER m_diveId WRITE setDiveId)
	Q_PROPERTY(int numDC READ numDC NOTIFY numDCChanged)
	Q_PROPERTY(int currentDC READ currentDC NOTIFY currentDCChanged)
	Q_PROPERTY(QString computerName READ computerName NOTIFY currentDCChanged)
	Q_PROPERTY(QString computerSerial READ computerSerial NOTIFY currentDCChanged)
	Q_PROPERTY(QString diveMode READ diveMode NOTIFY currentDCChanged)
	Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
	Q_PROPERTY(qreal xOffset MEMBER m_xOffset WRITE setXOffset NOTIFY xOffsetChanged)
	Q_PROPERTY(qreal yOffset MEMBER m_yOffset WRITE setYOffset NOTIFY yOffsetChanged)

public:
	explicit QMLProfile(QQuickItem *parent = 0);
	~QMLProfile();

	void paint(QPainter *painter);

	int diveId() const;
	void setDiveId(int diveId);
	int currentDC() const;
	QString computerName() const;
	QString computerSerial() const;
	QString diveMode() const;
	qreal devicePixelRatio() const;
	void setDevicePixelRatio(qreal dpr);
	void setXOffset(qreal value);
	void setYOffset(qreal value);
	Q_INVOKABLE void nextDC();
	Q_INVOKABLE void prevDC();
	Q_INVOKABLE QVariantMap sampleAtFraction(qreal fraction) const;

public slots:
	void setMargin(int margin);
	void screenChanged(QScreen *screen);
	void triggerUpdate();

private:
	int m_diveId;
	int m_dc;
	qreal m_devicePixelRatio;
	int m_margin;
	qreal m_xOffset, m_yOffset;
	std::unique_ptr<ProfileScene> m_profileWidget;
	void createProfileView();
	void rotateDC(int dir);
	int numDC() const;
	const divecomputer *currentDiveComputer() const;

private slots:
	void divesChanged(const QVector<dive *> &dives, DiveField);

signals:
	void rightAlignedChanged();
	void devicePixelRatioChanged();
	void xOffsetChanged();
	void yOffsetChanged();
	void numDCChanged();
	void currentDCChanged();
};

#endif // QMLPROFILE_H
