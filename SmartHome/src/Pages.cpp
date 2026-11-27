#include "Pages.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>
#include <QPushButton>
#include <QSlider>
#include <QProgressBar>
#include <QSizePolicy>
#include <QApplication>
#include <QPainter>
#include <QTimer>
#include <QDateTime>
#include <QStyle>

// ────────────────────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────────────────────

static QLabel* makeLabel(const QString& text, const QString& objName,
                         QWidget* parent = nullptr) {
    auto* l = new QLabel(text, parent);
    l->setObjectName(objName);
    return l;
}

static QFrame* hLine(QWidget* p = nullptr) {
    auto* f = new QFrame(p);
    f->setFrameShape(QFrame::HLine);
    f->setObjectName("HLine");
    return f;
}

static QScrollArea* makeScrollArea(QWidget* inner, QWidget* parent = nullptr) {
    auto* sa = new QScrollArea(parent);
    sa->setWidget(inner);
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setObjectName("ScrollArea");
    return sa;
}

// FIX: hide() before deleteLater() so old widgets don't flash until the event
// loop deletes them; also handles nested layouts and spacers.
static void clearLayout(QLayout* l) {
    while (QLayoutItem* item = l->takeAt(0)) {
        if (auto* w = item->widget()) {
            w->hide();
            w->deleteLater();
        } else if (auto* sub = item->layout()) {
            clearLayout(sub);
        }
        delete item;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// HourlyChart
// ════════════════════════════════════════════════════════════════════════════

HourlyChart::HourlyChart(QWidget* parent) : QWidget(parent) {
    setObjectName("ChartWidget");
    setMinimumHeight(110);
}

void HourlyChart::setData(const QVector<double>& hourly, int currentHour) {
    m_data = hourly;
    m_hour = currentHour;
    update();
}

void HourlyChart::paintEvent(QPaintEvent*) {
    if (m_data.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF area = QRectF(rect()).adjusted(4, 4, -4, -16);
    double mx = 0.1;
    for (double v : m_data) mx = qMax(mx, v);

    const int    n    = m_data.size();
    const double slot = area.width() / n;
    const double bw   = slot * 0.7;

    QFont f = p.font();
    f.setPointSize(7);
    p.setFont(f);

    for (int i = 0; i < n; ++i) {
        const double h = m_data[i] / mx * area.height();
        QRectF bar(area.left() + i * slot + (slot - bw) / 2.0,
                   area.bottom() - h, bw, h);
        p.fillRect(bar, i == m_hour ? QColor("#00ffc2") : QColor(0, 180, 255, 140));

        if (i % 3 == 0) {
            p.setPen(QColor("#7a9bb5"));
            p.drawText(QRectF(area.left() + i * slot, area.bottom() + 2, slot, 12),
                       Qt::AlignCenter, QString::number(i));
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// RoomsPage
// ════════════════════════════════════════════════════════════════════════════

RoomsPage::RoomsPage(SmartHome* home, QWidget* parent)
    : QWidget(parent), m_home(home)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16,16,16,16);
    outer->setSpacing(12);

    outer->addWidget(makeLabel("SMART ROOMS", "SectionTitle"));

    m_roomsContainer = new QWidget;
    m_roomsGrid = new QGridLayout(m_roomsContainer);
    m_roomsGrid->setSpacing(10);
    outer->addWidget(m_roomsContainer);

    outer->addWidget(hLine());

    m_roomFilterLabel = makeLabel("DEVICES — ALL ROOMS", "SectionTitle");
    outer->addWidget(m_roomFilterLabel);

    m_devicesContainer = new QWidget;
    m_devsGrid = new QGridLayout(m_devicesContainer);
    m_devsGrid->setSpacing(8);
    m_devScroll = makeScrollArea(m_devicesContainer);
    outer->addWidget(m_devScroll);

    buildRoomCards();
    buildDeviceCards();
}

void RoomsPage::refresh() {
    buildRoomCards();
    buildDeviceCards(m_selectedRoom);
}

// Used for background updates (temperature drift): never rebuild while the
// user has a mouse button down or is dragging a slider, otherwise the press
// target is destroyed and the click / drag is lost.
void RoomsPage::refreshIfIdle() {
    if (QApplication::mouseButtons() != Qt::NoButton) return;
    for (auto* s : findChildren<QSlider*>())
        if (s->isSliderDown()) return;
    refresh();
}

// In-place update: changing label text does not recreate widgets, so no flicker
void RoomsPage::updateEnvironment() {
    for (auto* card : m_roomsContainer->findChildren<QFrame*>()) {
        const QString rid = card->property("roomId").toString();
        if (rid.isEmpty()) continue;
        auto r = m_home->findRoom(rid);
        if (!r) continue;
        for (auto* l : card->findChildren<QLabel*>()) {
            const QString k = l->property("stat").toString();
            QString t;
            if      (k == "temp") t = QString("%1°").arg(r->temperature(), 0, 'f', 1);
            else if (k == "hum")  t = QString("%1%").arg(r->humidity());
            else if (k == "load") t = QString("%1W").arg((int)r->totalWatts());
            else continue;
            if (l->text() != t) l->setText(t);
        }
    }
}

void RoomsPage::buildRoomCards() {
    clearLayout(m_roomsGrid);
    int col = 0, row = 0;
    for (auto& r : m_home->rooms()) {
        auto* card = makeRoomCard(r);
        m_roomsGrid->addWidget(card, row, col);
        ++col;
        if (col >= 3) { col = 0; ++row; }
    }
}

QWidget* RoomsPage::makeRoomCard(QSharedPointer<Room> r) {
    auto* card = new QFrame;
    bool  sel  = (m_selectedRoom == r->id());
    card->setObjectName(sel ? "RoomCardSelected" : "RoomCard");
    card->setCursor(Qt::PointingHandCursor);
    card->setFixedHeight(130);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(12,10,12,10);

    auto* top = new QHBoxLayout;
    top->addWidget(makeLabel(r->name(), "RoomName"));
    top->addStretch();
    top->addWidget(makeLabel(r->emoji(), "RoomEmoji"));
    lay->addLayout(top);

    auto* stats = new QHBoxLayout;
    auto addStat = [&](const QString& val, const QString& lbl, const QString& key) {
        auto* b  = new QWidget;
        auto* bl = new QVBoxLayout(b);
        bl->setSpacing(1);
        bl->setContentsMargins(0,0,0,0);
        auto* vl = makeLabel(val, "StatValue");
        vl->setProperty("stat", key);
        vl->setAlignment(Qt::AlignCenter);
        auto* ll = makeLabel(lbl, "StatLabel");
        ll->setAlignment(Qt::AlignCenter);
        bl->addWidget(vl);
        bl->addWidget(ll);
        stats->addWidget(b);
    };
    addStat(QString("%1°").arg(r->temperature(),0,'f',1), "TEMP", "temp");
    addStat(QString("%1%").arg(r->humidity()),             "HUM",  "hum");
    addStat(QString("%1W").arg((int)r->totalWatts()),      "LOAD", "load");
    lay->addLayout(stats);

    auto* dots = new QHBoxLayout;
    for (auto& d : r->devices()) {
        auto* dot = new QLabel("●");
        dot->setObjectName(d->isOn() ? "DotOn" : "DotOff");
        dot->setFixedSize(12,12);
        dots->addWidget(dot);
    }
    dots->addStretch();
    auto* devCount = makeLabel(
        QString("%1/%2 ON").arg(r->activeDeviceCount()).arg(r->devices().size()),
        "StatLabel");
    dots->addWidget(devCount);
    lay->addLayout(dots);

    card->installEventFilter(this);
    card->setProperty("roomId", r->id());

    return card;
}

bool RoomsPage::eventFilter(QObject* obj, QEvent* ev) {
    if (ev->type() == QEvent::MouseButtonRelease) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (!w) return QWidget::eventFilter(obj, ev);

        QString rid = w->property("roomId").toString();
        QString did = w->property("deviceId").toString();

        if (!rid.isEmpty()) { onRoomClicked(rid);   return true; }
        if (!did.isEmpty()) { onDeviceClicked(did); return true; }
    }
    return QWidget::eventFilter(obj, ev);
}

void RoomsPage::onRoomClicked(const QString& roomId) {
    m_selectedRoom = (m_selectedRoom == roomId) ? QString() : roomId;

    QString lbl = "ALL ROOMS";
    if (!m_selectedRoom.isEmpty()) {
        auto room = m_home->findRoom(m_selectedRoom);
        if (room) lbl = room->name().toUpper();
        else      m_selectedRoom.clear();
    }
    m_roomFilterLabel->setText("DEVICES — " + lbl);
    buildRoomCards();
    buildDeviceCards(m_selectedRoom);
    if (m_devScroll) {   // different filter: start from the top (runs after the restore above)
        auto* sa = m_devScroll;
        QTimer::singleShot(0, sa, [sa]{ sa->verticalScrollBar()->setValue(0); });
    }
}

void RoomsPage::buildDeviceCards(const QString& roomId) {
    // Remember the scroll position: the grid is rebuilt on every real change,
    // which used to throw the list back to the top.
    const int pos = m_devScroll ? m_devScroll->verticalScrollBar()->value() : 0;
    clearLayout(m_devsGrid);

    QVector<QSharedPointer<Device>> devs;
    if (roomId.isEmpty())
        devs = m_home->allDevices();
    else if (auto r = m_home->findRoom(roomId))
        devs = r->devices();

    int col = 0, row = 0;
    for (auto& d : devs) {
        auto* card = makeDeviceCard(d);
        m_devsGrid->addWidget(card, row, col);
        ++col;
        if (col >= 4) { col = 0; ++row; }
    }

    if (m_devScroll && pos > 0) {
        auto* sa = m_devScroll;
        QTimer::singleShot(0, sa, [sa, pos]{ sa->verticalScrollBar()->setValue(pos); });
    }
}

QWidget* RoomsPage::makeDeviceCard(QSharedPointer<Device> d) {
    auto* card = new QFrame;
    card->setObjectName(d->isOn() ? "DeviceCardOn" : "DeviceCard");
    card->setCursor(Qt::PointingHandCursor);
    card->setFixedSize(150, 120);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(10,8,10,8);
    lay->setSpacing(3);

    auto* hdr = new QHBoxLayout;
    hdr->addStretch();
    auto* badge = new QLabel("●");
    badge->setObjectName(d->isOn() ? "BadgeOn" : "BadgeOff");
    hdr->addWidget(badge);
    lay->addLayout(hdr);

    auto* ico = makeLabel(d->icon(), "DeviceIcon");
    ico->setAlignment(Qt::AlignCenter);
    lay->addWidget(ico);

    auto* nm = makeLabel(d->name(), "DeviceName");
    nm->setAlignment(Qt::AlignCenter);
    nm->setWordWrap(true);
    lay->addWidget(nm);

    auto* st = makeLabel(d->statusText(), "DeviceStatus");
    st->setAlignment(Qt::AlignCenter);
    lay->addWidget(st);

    if (d->isOn() && (d->type() == DeviceType::Light ||
                      d->type() == DeviceType::Fan   ||
                      d->type() == DeviceType::Thermostat)) {
        int mn = (d->type() == DeviceType::Thermostat) ? 16 : 1;
        int mx = (d->type() == DeviceType::Thermostat) ? 30 :
                 (d->type() == DeviceType::Fan)        ? 3  : 100;
        auto* sl = new QSlider(Qt::Horizontal, card);
        sl->setRange(mn, mx);
        sl->setValue(d->value());
        sl->setObjectName("MiniSlider");
        sl->setFixedHeight(14);
        QString did = d->id();
        // FIX: the model now only emits statsChanged for slider moves, so the
        // slider is no longer destroyed mid-drag. Cards are rebuilt once on release.
        connect(sl, &QSlider::valueChanged, this, [this, did](int v){
            onSliderChanged(did, v);
        });
        connect(sl, &QSlider::sliderReleased, this, [this]{ refresh(); });
        lay->addWidget(sl);
    }

    card->installEventFilter(this);
    card->setProperty("deviceId", d->id());

    return card;
}

void RoomsPage::onDeviceClicked(const QString& deviceId) {
    // FIX: no manual rebuild — toggleDevice() emits devicesChanged, and
    // MainWindow refreshes the current page exactly once.
    m_home->toggleDevice(deviceId);
}

void RoomsPage::onSliderChanged(const QString& deviceId, int value) {
    m_home->setDeviceValue(deviceId, value);
}

// ════════════════════════════════════════════════════════════════════════════
// ClimatePage
// ════════════════════════════════════════════════════════════════════════════

ClimatePage::ClimatePage(SmartHome* home, QWidget* parent)
    : QWidget(parent), m_home(home)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16,16,16,16);
    outer->setSpacing(12);
    outer->addWidget(makeLabel("CLIMATE CONTROL", "SectionTitle"));

    m_container = new QWidget;
    m_grid = new QGridLayout(m_container);
    m_grid->setSpacing(10);
    outer->addWidget(makeScrollArea(m_container));

    outer->addWidget(makeLabel("OUTDOOR CONDITIONS", "SectionTitle"));
    auto* outRow = new QHBoxLayout;
    const char* titles[4] = {"TEMPERATURE", "HUMIDITY", "AIR QUALITY", "UV INDEX"};
    for (int i = 0; i < 4; ++i) {
        auto* c  = new QFrame; c->setObjectName("StatCard");
        auto* cl = new QVBoxLayout(c); cl->setContentsMargins(14,10,14,10);
        cl->addWidget(makeLabel(titles[i], "StatLabel"));
        m_outVals[i] = makeLabel("—", "StatValue");
        cl->addWidget(m_outVals[i]);
        m_outFrames[i] = c;
        outRow->addWidget(c);
    }
    outer->addLayout(outRow);
    outer->addStretch();
    buildCards();
    updateOutdoor();
}

// Values come from SmartHome's outdoor simulation (time-of-day driven),
// not from a real weather service.
void ClimatePage::updateOutdoor() {
    auto setFrame = [](QFrame* f, QLabel* l, const QString& name) {
        if (f->objectName() == name) return;
        f->setObjectName(name);
        f->style()->unpolish(f); f->style()->polish(f);
        l->style()->unpolish(l); l->style()->polish(l);
    };
    m_outVals[0]->setText(QString("%1°C").arg(m_home->outdoorTemp(), 0, 'f', 1));
    m_outVals[1]->setText(QString("%1%").arg(m_home->outdoorHumidity()));

    const int aqi = m_home->airQuality();
    m_outVals[2]->setText(aqi <= 50 ? "GOOD" : aqi <= 100 ? "MODERATE" : "POOR");
    setFrame(m_outFrames[2], m_outVals[2], aqi <= 50 ? "StatCardGreen" : "StatCardWarn");

    const double uv = m_home->uvIndex();
    m_outVals[3]->setText(QString("%1 %2").arg(qRound(uv))
                          .arg(uv < 3 ? "LOW" : uv < 6 ? "MOD" : uv < 8 ? "HIGH" : "V.HIGH"));
    setFrame(m_outFrames[3], m_outVals[3], uv < 3 ? "StatCardGreen" : "StatCardWarn");
}

void ClimatePage::buildCards() {
    clearLayout(m_grid);
    int col = 0, row = 0;
    for (auto& r : m_home->rooms()) {
        auto* card = new QFrame; card->setObjectName("ClimateCard");
        card->setProperty("roomId", r->id());
        auto* lay  = new QVBoxLayout(card);
        lay->setContentsMargins(14,12,14,12);
        lay->addWidget(makeLabel(r->emoji() + " " + r->name(), "RoomName"));

        auto* temp = makeLabel(
            QString("%1°C").arg(r->temperature(),0,'f',1), "ThermoDisplay");
        auto* hum  = makeLabel(
            QString("%1% RH").arg(r->humidity()), "StatLabel");
        temp->setProperty("stat", "temp");
        hum ->setProperty("stat", "hum");
        lay->addWidget(temp);
        lay->addWidget(hum);

        auto* btnRow = new QHBoxLayout;
        auto* btnMinus = new QPushButton("−"); btnMinus->setObjectName("ThermoBtn");
        auto* target   = makeLabel(QString("→ %1°C").arg(r->targetTemp()), "ThermoTarget");
        target->setAlignment(Qt::AlignCenter);
        target->setProperty("stat", "target");
        auto* btnPlus  = new QPushButton("+"); btnPlus->setObjectName("ThermoBtn");

        // FIX: go through SmartHome so the room target and its thermostat
        // device stay in sync (single source of truth).
        QString rid = r->id();
        connect(btnMinus, &QPushButton::clicked, this, [this, rid](){
            if (auto room = m_home->findRoom(rid)) {
                m_home->setRoomTargetTemp(rid, room->targetTemp() - 1);
                buildCards();
            }
        });
        connect(btnPlus, &QPushButton::clicked, this, [this, rid](){
            if (auto room = m_home->findRoom(rid)) {
                m_home->setRoomTargetTemp(rid, room->targetTemp() + 1);
                buildCards();
            }
        });

        btnRow->addWidget(btnMinus);
        btnRow->addWidget(target, 1);
        btnRow->addWidget(btnPlus);
        lay->addLayout(btnRow);

        m_grid->addWidget(card, row, col);
        ++col; if (col >= 3) { col = 0; ++row; }
    }
}

void ClimatePage::updateEnvironment() {
    for (auto* card : m_container->findChildren<QFrame*>()) {
        const QString rid = card->property("roomId").toString();
        if (rid.isEmpty()) continue;
        auto r = m_home->findRoom(rid);
        if (!r) continue;
        for (auto* l : card->findChildren<QLabel*>()) {
            const QString k = l->property("stat").toString();
            QString t;
            if      (k == "temp")   t = QString("%1°C").arg(r->temperature(), 0, 'f', 1);
            else if (k == "hum")    t = QString("%1% RH").arg(r->humidity());
            else if (k == "target") t = QString("→ %1°C").arg(r->targetTemp());
            else continue;
            if (l->text() != t) l->setText(t);
        }
    }
}

void ClimatePage::refresh() { buildCards(); updateOutdoor(); }

void ClimatePage::refreshIfIdle() {
    if (QApplication::mouseButtons() != Qt::NoButton) return;
    buildCards();
}

// ════════════════════════════════════════════════════════════════════════════
// EnergyPage
// ════════════════════════════════════════════════════════════════════════════

EnergyPage::EnergyPage(SmartHome* home, QWidget* parent)
    : QWidget(parent), m_home(home)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16,16,16,16);
    outer->setSpacing(12);
    outer->addWidget(makeLabel("ENERGY DASHBOARD", "SectionTitle"));

    auto* statRow = new QHBoxLayout;
    auto makeStat = [&](const QString& lbl, QLabel*& ref, const QString& obj = "StatCard") {
        auto* c  = new QFrame; c->setObjectName(obj);
        auto* cl = new QVBoxLayout(c); cl->setContentsMargins(14,10,14,10);
        cl->addWidget(makeLabel(lbl, "StatLabel"));
        ref = makeLabel("—", "StatValue"); cl->addWidget(ref);
        statRow->addWidget(c);
    };
    makeStat("CURRENT LOAD",  m_lblLoad);
    makeStat("TODAY'S USAGE", m_lblDay);
    makeStat("SOLAR GEN",     m_lblSolar, "StatCardGreen");
    makeStat("COST TODAY",    m_lblCost);
    outer->addLayout(statRow);

    outer->addWidget(makeLabel("CONSUMPTION BY ROOM", "SectionTitle"));
    m_barWidget = new QWidget;
    m_barsLay = new QGridLayout(m_barWidget);
    m_barsLay->setSpacing(6);
    outer->addWidget(m_barWidget);

    outer->addWidget(makeLabel("HOURLY USAGE TODAY (kWh)", "SectionTitle"));
    m_chart = new HourlyChart;          // FIX: real chart that actually paints
    outer->addWidget(m_chart);

    outer->addStretch();
    refresh();
}

void EnergyPage::refresh() {
    m_lblLoad ->setText(QString("%1 kW").arg(m_home->totalWatts()/1000.0,0,'f',1));
    m_lblDay  ->setText(QString("%1 kWh").arg(m_home->totalKwh(),0,'f',1));
    m_lblSolar->setText(QString("%1 kW").arg(m_home->solarWatts()/1000.0,0,'f',1));
    m_lblCost ->setText(QString("$%1").arg(m_home->estimatedCost(),0,'f',2));
    m_chart->setData(m_home->hourlyUsage(), QTime::currentTime().hour());

    // Build the bar rows once; afterwards only update values (no widget churn)
    if (m_bars.size() != m_home->rooms().size()) {
        clearLayout(m_barsLay);
        m_bars.clear(); m_barVals.clear();
        int row = 0;
        for (auto& r : m_home->rooms()) {
            m_barsLay->addWidget(makeLabel(r->emoji() + " " + r->name().left(8), "BarLabel"), row, 0);
            auto* bar = new QProgressBar;
            bar->setRange(0, 100);
            bar->setTextVisible(false);
            bar->setFixedHeight(8);
            bar->setObjectName("BarGreen");
            m_barsLay->addWidget(bar, row, 1);
            auto* val = makeLabel("0W", "BarVal");
            m_barsLay->addWidget(val, row, 2);
            m_bars << bar; m_barVals << val;
            ++row;
        }
    }

    double maxW = 1.0;
    for (auto& r : m_home->rooms()) maxW = qMax(maxW, r->totalWatts());

    for (int i = 0; i < m_home->rooms().size(); ++i) {
        const double w = m_home->rooms()[i]->totalWatts();
        auto* bar = m_bars[i];
        bar->setValue((int)(w / maxW * 100));
        const QString name = w > 500 ? "BarWarm" : w > 200 ? "BarDefault" : "BarGreen";
        if (bar->objectName() != name) {
            bar->setObjectName(name);
            bar->style()->unpolish(bar);
            bar->style()->polish(bar);
        }
        const QString txt = QString("%1W").arg((int)w);
        if (m_barVals[i]->text() != txt) m_barVals[i]->setText(txt);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// SecurityPage
// ════════════════════════════════════════════════════════════════════════════

SecurityPage::SecurityPage(SmartHome* home, QWidget* parent)
    : QWidget(parent), m_home(home)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16,16,16,16);
    outer->setSpacing(12);
    outer->addWidget(makeLabel("SECURITY STATUS", "SectionTitle"));

    m_statusContainer = new QWidget;
    auto* sg = new QHBoxLayout(m_statusContainer);
    sg->setSpacing(10);
    outer->addWidget(m_statusContainer);

    auto* alHead = new QHBoxLayout;
    alHead->addWidget(makeLabel("ALERTS", "SectionTitle"), 1);
    auto* clr = new QPushButton("CLEAR ALERTS");
    clr->setObjectName("ClearBtn");
    clr->setFixedHeight(24);
    connect(clr, &QPushButton::clicked, this, [this]{ m_home->clearAlerts(); });
    alHead->addWidget(clr);
    outer->addLayout(alHead);

    m_alertContainer = new QWidget;
    auto* alertLay = new QVBoxLayout(m_alertContainer);
    alertLay->setSpacing(5);

    auto* alOuter = new QWidget;
    auto* alLay   = new QVBoxLayout(alOuter);
    alLay->setContentsMargins(0,0,0,0);
    alLay->addWidget(m_alertContainer);
    alLay->addStretch();
    outer->addWidget(makeScrollArea(alOuter));

    refresh();
}

void SecurityPage::refresh() {
    auto* sg = qobject_cast<QHBoxLayout*>(m_statusContainer->layout());
    if (!sg) return;
    clearLayout(sg);

    auto makeSec = [&](const QString& ico, const QString& lbl,
                       const QString& val, bool armed = false) {
        auto* c  = new QFrame;
        c->setObjectName(armed ? "SecCardArmed" : "SecCardSafe");
        auto* cl = new QVBoxLayout(c);
        cl->setContentsMargins(14,12,14,12);
        cl->setAlignment(Qt::AlignCenter);
        cl->addWidget(makeLabel(ico,  "SecIcon"));
        cl->addWidget(makeLabel(lbl,  "StatLabel"));
        cl->addWidget(makeLabel(val,  armed ? "SecValArmed" : "SecValSafe"));
        sg->addWidget(c);
    };

    int locks=0, locksOn=0, cams=0, camsOn=0;
    int motion=0, motionOn=0, smoke=0, smokeOn=0;
    bool motionAlert=false, smokeAlert=false;
    for (auto& d : m_home->allDevices()) {
        switch (d->type()) {
            case DeviceType::Lock:   ++locks; if (d->isOn()) ++locksOn; break;
            case DeviceType::Camera: ++cams;  if (d->isOn()) ++camsOn;  break;
            case DeviceType::Sensor:
                ++motion; if (d->isOn()) ++motionOn;
                if (d->hasAlert()) motionAlert = true;
                break;
            case DeviceType::SmokeSensor:   // FIX: counted separately from motion sensors
                ++smoke;  if (d->isOn()) ++smokeOn;
                if (d->hasAlert()) smokeAlert = true;
                break;
            default: break;
        }
    }

    const bool siren = m_home->alarmTriggered();
    makeSec((m_home->alarmArmed() || siren) ? "🚨" : "🛡",
            "ALARM SYSTEM",
            siren ? "TRIGGERED" : m_home->alarmArmed() ? "ARMED" : "DISARMED",
            m_home->alarmArmed() || siren);
    makeSec("🔐", "DOOR LOCKS", QString("%1/%2 Locked").arg(locksOn).arg(locks));
    makeSec("📷", "CAMERAS",    QString("%1/%2 Active").arg(camsOn).arg(cams));
    makeSec("👁", "MOTION SENSORS",
            motionAlert ? "MOTION!" : QString("%1/%2 Active").arg(motionOn).arg(motion),
            motionAlert);
    makeSec("🔥", "SMOKE SENSORS",
            smokeAlert ? "SMOKE!" : QString("%1/%2 Active").arg(smokeOn).arg(smoke),
            smokeAlert);

    // Alerts now come from the model instead of hard-coded strings
    auto* alLay = qobject_cast<QVBoxLayout*>(m_alertContainer->layout());
    if (!alLay) return;
    clearLayout(alLay);

    if (m_home->alerts().isEmpty()) {
        alLay->addWidget(makeLabel("No active alerts", "StatLabel"));
        return;
    }
    for (auto& a : m_home->alerts()) {
        const bool danger = a.level == LogEntry::Level::Danger;
        const bool warn   = a.level == LogEntry::Level::Warning;
        auto* row = new QFrame;
        row->setObjectName(danger ? "AlertDanger" : warn ? "AlertWarn" : "AlertInfo");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(10,6,10,6);
        rl->addWidget(makeLabel(danger ? "🔥" : warn ? "⚠" : "●", "AlertDot"));
        rl->addWidget(makeLabel(QString("%1  %2 — %3").arg(a.timestamp, a.room, a.message),
                                "AlertMsg"), 1);
        alLay->addWidget(row);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// ScenesPage
// ════════════════════════════════════════════════════════════════════════════

ScenesPage::ScenesPage(SmartHome* home, QWidget* parent)
    : QWidget(parent), m_home(home)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16,16,16,16);
    outer->setSpacing(12);
    outer->addWidget(makeLabel("SCENE PRESETS", "SectionTitle"));

    auto* container = new QWidget;
    m_grid = new QGridLayout(container);
    m_grid->setSpacing(10);
    outer->addWidget(container);
    outer->addStretch();
    refresh();
}

void ScenesPage::refresh() {
    clearLayout(m_grid);
    int col = 0, row = 0;
    for (auto& s : m_home->scenes()) {
        auto* card = new QFrame;
        card->setObjectName(s.active ? "SceneCardActive" : "SceneCard");
        card->setCursor(Qt::PointingHandCursor);
        card->setFixedSize(160, 140);
        auto* cl = new QVBoxLayout(card);
        cl->setAlignment(Qt::AlignCenter);
        cl->addWidget(makeLabel(s.icon,        "SceneIcon"));
        cl->addWidget(makeLabel(s.name,        "SceneName"));
        cl->addWidget(makeLabel(s.description, "SceneDesc"));
        card->installEventFilter(this);
        card->setProperty("sceneId", s.id);
        m_grid->addWidget(card, row, col);
        ++col; if (col >= 4) { col = 0; ++row; }
    }
}

bool ScenesPage::eventFilter(QObject* obj, QEvent* ev) {
    if (ev->type() == QEvent::MouseButtonRelease) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (!w) return QWidget::eventFilter(obj, ev);
        QString sid = w->property("sceneId").toString();
        if (!sid.isEmpty()) {
            // activateScene() emits devicesChanged -> MainWindow refreshes this page
            m_home->activateScene(sid);
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

// ════════════════════════════════════════════════════════════════════════════
// AutomationPage
// ════════════════════════════════════════════════════════════════════════════

AutomationPage::AutomationPage(SmartHome* home, QWidget* parent)
    : QWidget(parent), m_home(home)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16,16,16,16);
    outer->setSpacing(12);
    outer->addWidget(makeLabel("AUTOMATIONS", "SectionTitle"));

    auto* inner = new QWidget;
    m_listLay = new QVBoxLayout(inner);
    m_listLay->setSpacing(8);
    m_listLay->setContentsMargins(0,0,0,0);
    outer->addWidget(makeScrollArea(inner));
    refresh();
}

void AutomationPage::refresh() {
    clearLayout(m_listLay);
    for (auto& a : m_home->automations()) {
        auto* row = new QFrame;
        row->setObjectName(a.enabled ? "AutoRowOn" : "AutoRow");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(14,10,14,10);
        rl->addWidget(makeLabel(a.icon, "AutoIcon"));
        auto* info = new QWidget;
        auto* il   = new QVBoxLayout(info);
        il->setSpacing(2);
        il->setContentsMargins(0,0,0,0);
        il->addWidget(makeLabel(a.name,        "AutoName"));
        il->addWidget(makeLabel(a.description, "AutoDesc"));
        rl->addWidget(info, 1);
        auto* tog = new QPushButton(a.enabled ? "ON" : "OFF");
        tog->setObjectName(a.enabled ? "ToggleOn" : "ToggleOff");
        tog->setFixedSize(52, 24);
        QString aid = a.id;
        connect(tog, &QPushButton::clicked, this, [this, aid](){
            m_home->toggleAutomation(aid);
            refresh();
        });
        rl->addWidget(tog);
        m_listLay->addWidget(row);
    }
    m_listLay->addStretch();
}

// ════════════════════════════════════════════════════════════════════════════
// LogPage
// ════════════════════════════════════════════════════════════════════════════

LogPage::LogPage(SmartHome* home, QWidget* parent)
    : QWidget(parent), m_home(home)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16,16,16,16);
    outer->setSpacing(12);
    outer->addWidget(makeLabel("ACTIVITY LOG", "SectionTitle"));

    auto* inner = new QWidget;
    m_listLay = new QVBoxLayout(inner);
    m_listLay->setSpacing(3);
    m_listLay->setContentsMargins(0,0,0,0);
    outer->addWidget(makeScrollArea(inner));
    refresh();
}

void LogPage::refresh() {
    // FIX: remember the scroll position so a rebuild doesn't jump back to the top
    auto* sa = findChild<QScrollArea*>();
    const int pos = sa ? sa->verticalScrollBar()->value() : 0;

    clearLayout(m_listLay);
    for (auto& e : m_home->log()) {
        QString msgName = "LogMsg";                       // FIX: colour by level
        switch (e.level) {
            case LogEntry::Level::Warning: msgName = "LogMsgWarn";    break;
            case LogEntry::Level::Danger:  msgName = "LogMsgDanger";  break;
            case LogEntry::Level::Success: msgName = "LogMsgSuccess"; break;
            default: break;
        }
        auto* row = new QHBoxLayout;
        row->setSpacing(12);
        row->addWidget(makeLabel(e.timestamp, "LogTime"));
        row->addWidget(makeLabel(e.room,      "LogRoom"));
        row->addWidget(makeLabel(e.message,   msgName), 1);
        auto* wrapper = new QWidget;
        wrapper->setLayout(row);
        m_listLay->addWidget(wrapper);
    }
    m_listLay->addStretch();

    if (sa && pos > 0)
        QTimer::singleShot(0, sa, [sa, pos]{ sa->verticalScrollBar()->setValue(pos); });
}
