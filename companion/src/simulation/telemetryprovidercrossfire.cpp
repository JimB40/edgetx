/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QStyleFactory>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>

#include "appdata.h"
#include "telemetryprovidercrossfire.h"
#include "ui_telemetryprovidercrossfire.h"
#include "telem_data.h"

template<class t> t LIMIT(t mi, t x, t ma) { return std::min(std::max(mi, x), ma); }

namespace {
class RightAlignedListDelegate : public QStyledItemDelegate
{
 public:
  explicit RightAlignedListDelegate(int rightPadding, QObject * parent = nullptr):
    QStyledItemDelegate(parent),
    m_rightPadding(rightPadding)
  {
  }

  void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override
  {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const QWidget * widget = option.widget;
    QStyle * style = widget ? widget->style() : QApplication::style();

    opt.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    const QString text = index.data(Qt::DisplayRole).toString();
    QRect textRect = option.rect.adjusted(0, 0, -m_rightPadding, 0);
    const QColor textColor = (option.state & QStyle::State_Selected)
      ? option.palette.color(QPalette::HighlightedText)
      : option.palette.color(QPalette::Text);
    painter->save();
    painter->setPen(textColor);
    painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, text);
    painter->restore();
  }

 private:
  int m_rightPadding;
};

class TpwrPopupList : public QListWidget
{
 public:
  explicit TpwrPopupList(QWidget * parent = nullptr):
    QListWidget(parent)
  {
  }

 protected:
  void focusOutEvent(QFocusEvent * event) override
  {
    QListWidget::focusOutEvent(event);
    hide();
  }

  void keyPressEvent(QKeyEvent * event) override
  {
    if (event->key() == Qt::Key_Escape) {
      hide();
      return;
    }
    QListWidget::keyPressEvent(event);
  }

  void showEvent(QShowEvent * event) override
  {
    QListWidget::showEvent(event);
    qApp->installEventFilter(this);
  }

  void hideEvent(QHideEvent * event) override
  {
    qApp->removeEventFilter(this);
    QListWidget::hideEvent(event);
  }

  bool eventFilter(QObject * watched, QEvent * event) override
  {
    if (event->type() == QEvent::MouseButtonPress && isVisible()) {
      auto * me = static_cast<QMouseEvent *>(event);
      const QPoint gp = me->globalPosition().toPoint();
      if (!frameGeometry().contains(gp))
        hide();
    }
    return QListWidget::eventFilter(watched, event);
  }
};

int tpwrIndexFromText(const QString & text)
{
  QString digits;
  for (const QChar ch : text) {
    if (ch.isDigit())
      digits.append(ch);
  }
  if (digits.isEmpty())
    return -1;

  static const QStringList tpwrValues = {"0", "10", "25", "50", "100", "250", "500", "1000", "2000"};
  return tpwrValues.indexOf(digits);
}

void setTpwrFromAnyText(QComboBox * combo, const QString & text)
{
  if (!combo)
    return;

  const int idx = tpwrIndexFromText(text);
  if (idx >= 0)
    combo->setCurrentIndex(idx);
}

void installLargeStepButtons(QAbstractSpinBox * spin)
{
  if (spin->inherits("QDateTimeEdit"))
    return;

  const int spinFrame = 1;
  const int buttonGap = 3;
  const int buttonSize = qMax(14, spin->height() - 8);
  const int buttonAreaWidth = (buttonSize * 2) + buttonGap;

  spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spin->setStyleSheet(
    QString("QAbstractSpinBox { padding-left: 7px; padding-right: %1px; }"
            "QAbstractSpinBox QLineEdit {"
            "  padding-left: 7px; padding-right: 10px;"
            "  border: 1px solid #d9d9d9; border-radius: 8px;"
            "  background: palette(base); color: #111111;"
            "  selection-background-color: #0a84ff; selection-color: #ffffff;"
            "}"
            "QAbstractSpinBox QLineEdit:focus {"
            "  background: #0a84ff; color: #ffffff; border-color: #0a84ff;"
            "  selection-background-color: #066bd4; selection-color: #ffffff;"
            "}")
      .arg(buttonAreaWidth + spinFrame + 10));

  auto * downButton = new QToolButton(spin);
  downButton->setObjectName("stepDownButton");
  downButton->setText(QStringLiteral("▼"));
  downButton->setAutoRepeat(true);
  downButton->setAutoRepeatDelay(250);
  downButton->setAutoRepeatInterval(60);
  downButton->setFocusPolicy(Qt::NoFocus);

  auto * upButton = new QToolButton(spin);
  upButton->setObjectName("stepUpButton");
  upButton->setText(QStringLiteral("▲"));
  upButton->setAutoRepeat(true);
  upButton->setAutoRepeatDelay(250);
  upButton->setAutoRepeatInterval(60);
  upButton->setFocusPolicy(Qt::NoFocus);

  const QString buttonStyle =
    "QToolButton { border: 0; border-radius: 0; margin: 0px; padding: 0px; background: #e4e4e4; color: #111111; font-size: 13px; }"
    "QToolButton#stepDownButton { border: 1px solid #b6b6b6; }"
    "QToolButton#stepUpButton { border: 1px solid #b6b6b6; }"
    "QToolButton:pressed { background: #d2d2d2; color: #000000; }";
  downButton->setStyleSheet(buttonStyle);
  upButton->setStyleSheet(buttonStyle);

  QObject::connect(downButton, &QToolButton::clicked, spin, &QAbstractSpinBox::stepDown);
  QObject::connect(upButton, &QToolButton::clicked, spin, &QAbstractSpinBox::stepUp);

  const int h = spin->height();
  const int w = spin->width();
  const int x = w - buttonAreaWidth - spinFrame;
  const int y = (h - buttonSize) / 2;
  downButton->setGeometry(x, y, buttonSize, buttonSize);
  upButton->setGeometry(x + buttonSize + buttonGap, y, buttonSize, buttonSize);
}

void tuneTelemetryInputWidgets(QWidget * root)
{
  const int fieldWidth = 120;

  auto * grid = qobject_cast<QGridLayout *>(root->layout());
  if (grid) {
    grid->setHorizontalSpacing(0);
    grid->setColumnMinimumWidth(1, fieldWidth);
    grid->setColumnStretch(1, 0);
    grid->setColumnStretch(2, 0);
  }

  const auto spinBoxes = root->findChildren<QAbstractSpinBox *>();
  for (QAbstractSpinBox * spin : spinBoxes) {
    spin->setFixedWidth(fieldWidth);
    spin->setFixedHeight(24);
    spin->setSizePolicy(QSizePolicy::Fixed, spin->sizePolicy().verticalPolicy());
    spin->setAccelerated(true);
    spin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    installLargeStepButtons(spin);
    if (grid)
      grid->setAlignment(spin, Qt::AlignLeft | Qt::AlignVCenter);
  }

  const auto lineEdits = root->findChildren<QLineEdit *>();
  for (QLineEdit * edit : lineEdits) {
    if (qobject_cast<QAbstractSpinBox *>(edit->parentWidget()))
      continue;
    edit->setFixedWidth(fieldWidth);
    edit->setFixedHeight(24);
    edit->setSizePolicy(QSizePolicy::Fixed, edit->sizePolicy().verticalPolicy());
    edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    edit->setStyleSheet("QLineEdit:focus { background: #0a84ff; color: #ffffff; border-color: #0a84ff; selection-background-color: #066bd4; selection-color: #ffffff; }");
    if (grid)
      grid->setAlignment(edit, Qt::AlignLeft | Qt::AlignVCenter);
  }

  const auto comboBoxes = root->findChildren<QComboBox *>();
  for (QComboBox * combo : comboBoxes) {
    combo->setFixedWidth(fieldWidth);
    combo->setFixedHeight(24);
    combo->setSizePolicy(QSizePolicy::Fixed, combo->sizePolicy().verticalPolicy());
    if (grid)
      grid->setAlignment(combo, Qt::AlignLeft | Qt::AlignVCenter);
  }

  const auto labels = root->findChildren<QLabel *>();
  for (QLabel * label : labels) {
    const QString txt = label->text().trimmed();
    const bool looksLikeUnit = label->objectName().contains("_unit") ||
                               txt == "dB" || txt == "%" || txt == "mw" || txt == "mW" ||
                               txt == "dBm" || txt == "V" || txt == "A" || txt == "mAh" ||
                               txt == "km/h" || txt == "kmh" || txt == "Degrees" || txt == "Radians" ||
                               txt == "m/s" || txt == "m" || txt == "kts" || txt == "rpm" ||
                               txt == "ml" || txt == "g" || txt == "°C";
    if (!looksLikeUnit)
      continue;
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setStyleSheet("padding-left: 0px; margin-left: 0px;");
    if (grid)
      grid->setAlignment(label, Qt::AlignLeft | Qt::AlignVCenter);
  }

  const auto checkBoxes = root->findChildren<QCheckBox *>();
  for (QCheckBox * cb : checkBoxes) {
    cb->setStyleSheet(
      "QCheckBox { font-weight: 600; spacing: 5px; margin-top: 3px; margin-bottom: 3px; }"
    );
    if (!grid)
      continue;
    int row = 0, col = 0, rowSpan = 1, colSpan = 1;
    const int idx = grid->indexOf(cb);
    if (idx < 0)
      continue;
    grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);
    if (col != 0) {
      grid->removeWidget(cb);
      grid->addWidget(cb, row, 0, rowSpan, 2, Qt::AlignLeft | Qt::AlignVCenter);
    } else {
      grid->setAlignment(cb, Qt::AlignLeft | Qt::AlignVCenter);
    }
  }
}
}

TelemetryProviderCrossfire::TelemetryProviderCrossfire(QWidget * parent):
  QWidget(parent),
  ui(new Ui::TelemetryProviderCrossfire)
{
  ui->setupUi(this);
  constexpr int kTpwrFieldWidth = 100;
  tuneTelemetryInputWidgets(this);

  // Keep TPWR combo aligned with numeric fields (same left/right edges and vertical centering).
  ui->input_tpwr->setFixedSize(kTpwrFieldWidth, ui->input_1rss->height());
  ui->label_tpwr_unit->setText("mW");
  ui->input_tpwr->clear();
  ui->input_tpwr->addItem("0");
  ui->input_tpwr->addItem("10");
  ui->input_tpwr->addItem("25");
  ui->input_tpwr->addItem("50");
  ui->input_tpwr->addItem("100");
  ui->input_tpwr->addItem("250");
  ui->input_tpwr->addItem("500");
  ui->input_tpwr->addItem("1000");
  ui->input_tpwr->addItem("2000");
  ui->input_tpwr->setCurrentText("25");
  if (auto * grid = qobject_cast<QGridLayout *>(layout())) {
    int row = 0, col = 0, rowSpan = 1, colSpan = 1;
    const int idx = grid->indexOf(ui->input_tpwr);
    if (idx >= 0) {
      grid->getItemPosition(idx, &row, &col, &rowSpan, &colSpan);

      QWidget * tpwrField = new QWidget(this);
      tpwrField->setObjectName("tpwrField");
      tpwrField->setFixedSize(kTpwrFieldWidth, ui->input_1rss->height());

      const int tpwrButtonSize = qMax(14, tpwrField->height() - 8);
      const int tpwrButtonRightGap = 7;
      const int tpwrButtonX = tpwrField->width() - tpwrButtonRightGap - tpwrButtonSize;
      const int tpwrButtonY = (tpwrField->height() - tpwrButtonSize) / 2;
      const int tpwrTextRightPadding = (tpwrField->width() - tpwrButtonX) + 2;

      QLineEdit * tpwrDisplay = new QLineEdit(tpwrField);
      tpwrDisplay->setObjectName("tpwrDisplay");
      tpwrDisplay->setReadOnly(true);
      tpwrDisplay->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      tpwrDisplay->setFrame(false);
      tpwrDisplay->setText(ui->input_tpwr->currentText());
      tpwrDisplay->setGeometry(0, 0, tpwrField->width(), tpwrField->height());
      tpwrDisplay->setStyleSheet(
        QString("QLineEdit { border: 1px solid #d9d9d9; border-radius: 8px; background: palette(base); color: #111111; padding-left: 6px; padding-right: %1px; }"
                "QLineEdit:focus { border-color: #0a84ff; background: #0a84ff; color: #ffffff; }")
          .arg(tpwrTextRightPadding)
      );

      QToolButton * tpwrDropDownButton = new QToolButton(tpwrField);
      tpwrDropDownButton->setObjectName("tpwrDropDownButton");
      tpwrDropDownButton->setText(QStringLiteral("▼"));
      tpwrDropDownButton->setFocusPolicy(Qt::NoFocus);
      tpwrDropDownButton->setGeometry(tpwrButtonX, tpwrButtonY, tpwrButtonSize, tpwrButtonSize);
      tpwrDropDownButton->setStyleSheet(
        "QToolButton { border: 1px solid #b6b6b6; border-radius: 0px; margin: 0px; padding: 0px; background: #e4e4e4; color: #111111; font-size: 13px; }"
        "QToolButton:pressed { background: #d2d2d2; color: #000000; }"
      );

      TpwrPopupList * tpwrPopup = new TpwrPopupList(this);
      tpwrPopup->setObjectName("tpwrPopup");
      tpwrPopup->setWindowFlag(Qt::Popup, true);
      tpwrPopup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      tpwrPopup->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      tpwrPopup->setSelectionMode(QAbstractItemView::SingleSelection);
      tpwrPopup->setItemDelegate(new RightAlignedListDelegate(10, tpwrPopup));
      tpwrPopup->setStyleSheet(
        "QListWidget { border: 1px solid #b6b6b6; background: palette(base); }"
      );

      for (int i = 0; i < ui->input_tpwr->count(); ++i) {
        auto * item = new QListWidgetItem(ui->input_tpwr->itemText(i), tpwrPopup);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item->setData(Qt::UserRole, i);
      }

      const auto syncTpwrDisplay = [tpwrDisplay, this](int index) {
        if (index >= 0)
          tpwrDisplay->setText(ui->input_tpwr->itemText(index));
      };
      syncTpwrDisplay(ui->input_tpwr->currentIndex());
      connect(ui->input_tpwr, QOverload<int>::of(&QComboBox::currentIndexChanged), this, syncTpwrDisplay);

      connect(tpwrPopup, &QListWidget::itemClicked, this, [tpwrPopup, this](QListWidgetItem * item) {
        if (!item)
          return;
        ui->input_tpwr->setCurrentIndex(item->data(Qt::UserRole).toInt());
        tpwrPopup->hide();
      });

      connect(tpwrDropDownButton, &QToolButton::clicked, this, [tpwrPopup, tpwrDropDownButton, tpwrField, this]() {
        const int currentIdx = ui->input_tpwr->currentIndex();
        if (currentIdx >= 0)
          tpwrPopup->setCurrentRow(currentIdx);

        int rowH = tpwrPopup->sizeHintForRow(0);
        if (rowH < 1)
          rowH = tpwrPopup->fontMetrics().height() + 8;
        const int popupHeight = qMax(1, rowH) * tpwrPopup->count() + 2;
        int maxTextWidth = 0;
        for (int i = 0; i < tpwrPopup->count(); ++i)
          maxTextWidth = qMax(maxTextWidth, tpwrPopup->fontMetrics().horizontalAdvance(tpwrPopup->item(i)->text()));
        const int popupWidth = qMax(78, maxTextWidth + 24);

        tpwrPopup->setFixedSize(popupWidth, popupHeight);
        const QPoint buttonTopLeft = tpwrDropDownButton->mapToGlobal(QPoint(0, 0));
        const QPoint fieldBottomLeft = tpwrField->mapToGlobal(QPoint(0, tpwrField->height()));
        const int popupX = buttonTopLeft.x() + tpwrDropDownButton->width() - popupWidth;
        const int popupY = fieldBottomLeft.y() + 2;
        const QPoint popupPos(popupX, popupY);
        tpwrPopup->move(popupPos);
        tpwrPopup->show();
        tpwrPopup->raise();
        tpwrPopup->setFocus();

        // On first show, macOS/Qt may adjust popup geometry after painting.
        // Re-apply desired coordinates in the next event loop cycle.
        QTimer::singleShot(0, tpwrPopup, [tpwrPopup, popupPos]() {
          if (!tpwrPopup->isVisible())
            return;
          tpwrPopup->move(popupPos);
          tpwrPopup->raise();
        });
      });

      grid->removeWidget(ui->input_tpwr);
      ui->input_tpwr->hide();
      grid->addWidget(tpwrField, row, col, rowSpan, colSpan, Qt::AlignLeft | Qt::AlignVCenter);
    }
  }
  if (auto * grid = qobject_cast<QGridLayout *>(layout())) {
    grid->setAlignment(ui->input_tpwr, Qt::AlignLeft | Qt::AlignVCenter);
    grid->setAlignment(ui->label_tpwr, Qt::AlignLeft | Qt::AlignVCenter);
    grid->setAlignment(ui->label_tpwr_unit, Qt::AlignLeft | Qt::AlignVCenter);
  }

  // Set default values from UI definition into GPS
  gps.setLatLon(ui->input_gps->text());
  gps.setCourseDegrees(ui->input_hdg->value());
  gps.setSpeedKMH(ui->input_gspd->value());
  gps.setSatelliteCount(ui->input_sats->value());
  gps.setAltitude(ui->input_alt->value());

  connect(ui->input_hdg,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), &gps, &SimulatedGPS::setCourseDegrees);
  connect(ui->input_gps,  &QLineEdit::textChanged,                              &gps, &SimulatedGPS::setLatLon);
  connect(ui->input_gspd, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &gps, &SimulatedGPS::setSpeedKMH);
  connect(ui->input_alt,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), &gps, &SimulatedGPS::setAltitude);
  connect(ui->input_sats, QOverload<int>::of(&QSpinBox::valueChanged),          &gps, &SimulatedGPS::setSatelliteCount);

  connect(&gps, &SimulatedGPS::positionChanged,      ui->input_gps, &QLineEdit::setText);
  connect(&gps, &SimulatedGPS::courseDegreesChanged, ui->input_hdg, QOverload<double>::of(&QDoubleSpinBox::setValue));

  auto * grid = qobject_cast<QGridLayout *>(layout());
  const auto bindGroup = [this](QCheckBox * toggle, const std::initializer_list<QWidget *> & widgets) {
    const QList<QWidget *> groupWidgets(widgets.begin(), widgets.end());
    const auto updateVisibility = [groupWidgets](bool on) {
      for (QWidget * w : groupWidgets) {
        if (w)
          w->setVisible(on);
      }
    };
    connect(toggle, &QCheckBox::toggled, this, updateVisibility);
    updateVisibility(toggle->isChecked());
  };

  bindGroup(ui->enabled_battery, {
    ui->label_rxbt, ui->input_rxbt, ui->label_rxbt_unit,
    ui->label_curr, ui->input_curr, ui->label_curr_unit,
    ui->label_capa, ui->input_capa, ui->label_capa_unit,
    ui->label_batpercent, ui->input_batpercent, ui->label_batpercent_unit
  });
  bindGroup(ui->enabled_gps, {
    ui->label_gps_sim, ui->button_gpsRunStop,
    ui->label_gps, ui->input_gps, ui->label_gps_unit,
    ui->label_gspd, ui->input_gspd, ui->label_gspd_unit,
    ui->label_hdg, ui->input_hdg, ui->label_hdg_unit,
    ui->label_sats, ui->input_sats
  });
  bindGroup(ui->enabled_attitude, {
    ui->label_ptch, ui->input_ptch, ui->label_ptch_unit,
    ui->label_roll, ui->input_roll, ui->label_roll_unit,
    ui->label_yaw, ui->input_yaw, ui->label_yaw_unit
  });
  bindGroup(ui->enabled_flightcontroller, {
    ui->label_fm, ui->input_fm
  });
  bindGroup(ui->enabled_barometer, {
    ui->label_vspd, ui->input_vspd, ui->label_vspd_unit,
    ui->label_alt, ui->input_alt, ui->label_alt_unit
  });

  const auto relayoutLowerGroups = [grid, this](bool attitudeOn) {
    if (!grid)
      return;

    const int flightRow = attitudeOn ? 29 : 25;
    const int fmRow = flightRow + 1;
    const int barometerRow = fmRow + 1;
    const int vspdRow = barometerRow + 1;
    const int altRow = vspdRow + 1;

    grid->removeWidget(ui->enabled_flightcontroller);
    grid->removeWidget(ui->label_fm);
    grid->removeWidget(ui->input_fm);
    grid->removeWidget(ui->enabled_barometer);
    grid->removeWidget(ui->label_vspd);
    grid->removeWidget(ui->input_vspd);
    grid->removeWidget(ui->label_vspd_unit);
    grid->removeWidget(ui->label_alt);
    grid->removeWidget(ui->input_alt);
    grid->removeWidget(ui->label_alt_unit);

    grid->addWidget(ui->enabled_flightcontroller, flightRow, 0, 1, 2, Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(ui->label_fm, fmRow, 0);
    grid->addWidget(ui->input_fm, fmRow, 1);
    grid->addWidget(ui->enabled_barometer, barometerRow, 0, 1, 2, Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(ui->label_vspd, vspdRow, 0);
    grid->addWidget(ui->input_vspd, vspdRow, 1);
    grid->addWidget(ui->label_vspd_unit, vspdRow, 2);
    grid->addWidget(ui->label_alt, altRow, 0);
    grid->addWidget(ui->input_alt, altRow, 1);
    grid->addWidget(ui->label_alt_unit, altRow, 2);
  };
  connect(ui->enabled_attitude, &QCheckBox::toggled, this, relayoutLowerGroups);
  relayoutLowerGroups(ui->enabled_attitude->isChecked());

  // Create this once
  supportedLogItems.clear();
  supportedLogItems.insert("1RSS", "dB");
  supportedLogItems.insert("2RSS", "dB");
  supportedLogItems.insert("RQly", "%");
  supportedLogItems.insert("RSNR", "dB");
  supportedLogItems.insert("ANT", "");
  supportedLogItems.insert("RFMD", "");
  supportedLogItems.insert("TPWR", "mW");
  supportedLogItems.insert("TRSS", "dB");
  supportedLogItems.insert("TQly", "%");
  supportedLogItems.insert("TSNR", "dB");
  supportedLogItems.insert("RxBt", "V");
  supportedLogItems.insert("Curr", "A");
  supportedLogItems.insert("Capa", "mAh");
  supportedLogItems.insert("Bat%", "%");
  supportedLogItems.insert("GPS", "");
  supportedLogItems.insert("GSpd", "kmh");
  supportedLogItems.insert("Hdg", "°");
  supportedLogItems.insert("Alt", "m");
  supportedLogItems.insert("Sats", "");
  supportedLogItems.insert("Ptch", "rad");
  supportedLogItems.insert("Roll", "rad");
  supportedLogItems.insert("Yaw", "rad");
  supportedLogItems.insert("FM", "");
  supportedLogItems.insert("Alt", "m");
  supportedLogItems.insert("VSpd", "m/s");
}

TelemetryProviderCrossfire::~TelemetryProviderCrossfire()
{
  delete ui;
}

QHash<QString, QString> * TelemetryProviderCrossfire::getSupportedLogItems()
{
  return &supportedLogItems;
}

QString TelemetryProviderCrossfire::getLogfileIdentifier()
{
  return QString("TELEMETRY_DATA: CRSF");
}

void TelemetryProviderCrossfire::loadItemFromLog(QString item, QString value)
{
  if (item == "1RSS") ui->input_1rss->setValue(value.toInt());
  if (item == "2RSS") ui->input_2rss->setValue(value.toInt());
  if (item == "RQly") ui->input_rqly->setValue(value.toInt());
  if (item == "RSNR") ui->input_rsnr->setValue(value.toInt());
  if (item == "TRSS") ui->input_trss->setValue(value.toInt());
  if (item == "TPWR") setTpwrFromAnyText(ui->input_tpwr, value);
  if (item == "RFMD") ui->input_rfmd->setValue(value.toInt());
  if (item == "ANT") ui->input_ant->setValue(value.toInt());
  if (item == "TQly") ui->input_tqly->setValue(value.toInt());
  if (item == "TSNR") ui->input_tsnr->setValue(value.toInt());
  if (item == "RRSP") ui->input_rrsp->setValue(value.toInt());
  if (item == "RPWR") ui->input_rpwr->setValue(value.toInt());
  if (item == "TRSP") ui->input_trsp->setValue(value.toInt());
  if (item == "RxBt") ui->input_rxbt->setValue(value.toDouble());
  if (item == "Curr") ui->input_curr->setValue(value.toDouble());
  if (item == "Capa") ui->input_capa->setValue(value.toInt());
  if (item == "Bat%") ui->input_batpercent->setValue(value.toInt());
  if (item == "GPS") ui->input_gps->setText(value);
  if (item == "GSpd") ui->input_gspd->setValue(value.toDouble());
  if (item == "Hdg") ui->input_hdg->setValue(value.toDouble());
  if (item == "Sats") ui->input_sats->setValue(value.toInt());
  if (item == "Ptch") ui->input_ptch->setValue(value.toDouble());
  if (item == "Roll") ui->input_roll->setValue(value.toDouble());
  if (item == "Yaw") ui->input_yaw->setValue(value.toDouble());
  if (item == "FM") ui->input_fm->setText(value.remove(QChar('"')));
  if (item == "VSpd") ui->input_vspd->setValue(value.toDouble());
  if (item == "Alt") ui->input_alt->setValue(value.toDouble());
}

void TelemetryProviderCrossfire::resetRssi()
{
  uint8_t buffer[CROSSFIRE_PACKET_SIZE] = {0};
  generateTelemetryLinkStatisticsFrame(buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  emit telemetryDataChanged(SIMU_TELEMETRY_PROTOCOL_CROSSFIRE, QByteArray((char *)buffer, CROSSFIRE_PACKET_SIZE));
}

void TelemetryProviderCrossfire::loadUiFromSimulator(SimulatorInterface * simulator)
{
  // Nothing to do for crossfire
}

uint8_t dropdownToTPWRMap[] = { 0, 1, 2, 8, 3, 7, 4, 5, 6, };

void TelemetryProviderCrossfire::generateTelemetryFrame(SimulatorInterface *simulator)
{
  static int item = 0;
  uint8_t buffer[CROSSFIRE_PACKET_SIZE] = {0};

  switch (item++) {
  case 0:
    // Always generate link stats
    if (true) {
      // just want a block here to put these variables in
      uint8_t rssi1 = ui->input_1rss->value();
      uint8_t rssi2 = ui->input_2rss->value();
      uint8_t rqly = ui->input_rqly->value();
      int8_t rsnr = ui->input_rsnr->value();
      uint8_t ant = ui->input_ant->value();
      uint8_t rfmd = ui->input_rfmd->value();
      uint8_t tpwr = dropdownToTPWRMap[ui->input_tpwr->currentIndex()];
      uint8_t trss = ui->input_trss->value();
      uint8_t tqly = ui->input_tqly->value();
      int8_t tsnr = ui->input_tsnr->value();

      generateTelemetryLinkStatisticsFrame(buffer, rssi1, rssi2, rqly, rsnr, ant, rfmd, tpwr, trss, tqly, tsnr);
    }
    break;
  case 1:
    if (ui->enabled_battery->isChecked()) {
      double voltage = ui->input_rxbt->value();
      double current = ui->input_curr->value();
      int32_t used_capacity = ui->input_capa->value();
      int8_t battery_percent = ui->input_batpercent->value();

      generateTelemetryBatterySensorFrame(buffer, voltage, current, used_capacity, battery_percent);
    }
    break;
  case 2:
    if (ui->enabled_gps->isChecked()) {
      generateTelemetryGPSFrame(buffer, gps.lat, gps.lon, gps.speedKMH, gps.courseDegrees, gps.altitude, gps.satellites);
    }
    break;
  case 3:
    if (ui->enabled_attitude->isChecked()) {
      double pitch = ui->input_ptch->value();
      double roll = ui->input_roll->value();
      double yaw = ui->input_yaw->value();

      generateTelemetryAttitudeFrame(buffer, pitch, roll, yaw);
    }
    break;
  case 4:
    if (ui->enabled_flightcontroller->isChecked()) {
      QString mode = ui->input_fm->text();

      generateTelemetryFlightModeFrame(buffer, mode);
    }
    break;
  case 5:
    if (ui->enabled_barometer->isChecked()) {
      double altitude = ui->input_alt->value();
      double vspeed = ui->input_vspd->value();

      generateTelemetryBarometerFrame(buffer, altitude, vspeed);
    }
    break;
  default:
    item = 0;
    return;
  }

  if (buffer[0]) {
    // If we put anything in the buffer, send it
    QByteArray ba((char *)buffer, CROSSFIRE_PACKET_SIZE);
    emit telemetryDataChanged(SIMU_TELEMETRY_PROTOCOL_CROSSFIRE, ba);
  }
}

void TelemetryProviderCrossfire::generateTelemetryLinkStatisticsFrame(uint8_t *packet, uint8_t rss1, uint8_t rss2, uint8_t rqly, int8_t rsnr, uint8_t ant, uint8_t rfmd, uint8_t tpwr, uint8_t trss, uint8_t tqly, int8_t tsnr)
{
  packet[0] = 0xc8; // SYNC
  packet[1] = 13; // LEN (11 + 2 CRC)
  packet[2] = 0x14; // CRSF_FRAMETYPE_LINK_STATISTICS
  packet[3] = rss1;
  packet[4] = rss2;
  packet[5] = rqly;
  packet[6] = rsnr;
  packet[7] = ant;
  packet[8] = rfmd;
  packet[9] = tpwr;
  packet[10] = trss;
  packet[11] = tqly;
  packet[12] = tsnr;
  // Don't bother calculating the CRC, the telemetry consumer doesn't check it
}

void TelemetryProviderCrossfire::generateTelemetryBatterySensorFrame(uint8_t *packet, double voltage, double current, int32_t used_capacity, int8_t battery_percent)
{
  int32_t tmp;

  packet[0] = 0xc8; // SYNC
  packet[1] = 10; // LEN (8 + 2 CRC)
  packet[2] = 0x08; // CRSF_FRAMETYPE_BATTERY_SENSOR

  tmp = (voltage * 10);
  packet[3] = (tmp & 0xff00) >> 8;
  packet[4] = tmp & 0xff;                                                         

  tmp = (current * 10);
  packet[5] = (tmp & 0xff00) >> 8;
  packet[6] = tmp & 0xff;

  packet[7] = (used_capacity & 0xff0000) >> 16;
  packet[8] = (used_capacity & 0xff00) >> 8;
  packet[9] = (used_capacity & 0xff);

  packet[10] = battery_percent;
  // Don't bother calculating the CRC, the telemetry consumer doesn't check it
}

void TelemetryProviderCrossfire::generateTelemetryAttitudeFrame(uint8_t *packet, double pitch, double roll, double yaw)
{
  int16_t tmp;

  packet[0] = 0xc8; // SYNC
  packet[1] = 8; // LEN (6 + 2 CRC)
  packet[2] = 0x1e; // CRSF_FRAMETYPE_ATTITUDE

  tmp = pitch * 10000;
  packet[3] = (tmp & 0xff00) >> 8;
  packet[4] = tmp & 0xff;

  tmp = roll * 10000;
  packet[5] = (tmp & 0xff00) >> 8;
  packet[6] = tmp & 0xff;

  tmp = yaw * 10000;
  packet[7] = (tmp & 0xff00) >> 8;
  packet[8] = tmp & 0xff;
  // Don't bother calculating the CRC, the telemetry consumer doesn't check it
}

void TelemetryProviderCrossfire::generateTelemetryFlightModeFrame(uint8_t *packet, const QString &mode)
{
  QByteArray snipped = mode.toUtf8().left(13);
  int len = snipped.length();

  packet[0] = 0xc8; // SYNC
  packet[1] = 3 + len; // LEN (string + 2 CRC)
  packet[2] = 0x21; // CRSF_FRAMETYPE_FLIGHT_MODE

  int i;
  for (i = 0; i < len; i++) {
    packet[3+i] = snipped[i];
  }
  // Null terminate
  packet[3+i] = 0;
  // Don't bother calculating the CRC, the telemetry consumer doesn't check it
}

uint16_t encodeAltitude(double altitude)
{
  uint16_t res;

  if (altitude > 2276.7) {
    res = altitude;
    res |= 0x8000; // set high bit to indicate it's integer meters
  } else {
    res = (altitude * 10) + 10000;
    res &= 0x7fff; // clear high bit to indicate it's decimeters above -1000m
  }

  return res;
}

void TelemetryProviderCrossfire::generateTelemetryBarometerFrame(uint8_t *packet, double altitude, double vspeed)
{
  uint16_t tmp;

  packet[0] = 0xc8; // SYNC
  packet[1] = 6; // LEN (4 + 2 CRC)
  packet[2] = 0x09; // CRSF_FRAMETYPE_BARO_ALTITUDE

  tmp = encodeAltitude(altitude);
  packet[3] = (tmp & 0xff00) >> 8;
  packet[4] = tmp & 0xff;

  tmp = vspeed * 100; // cm/sec
  packet[5] = (tmp & 0xff00) >> 8;
  packet[6] = tmp & 0xff;
  // Don't bother calculating the CRC, the telemetry consumer doesn't check it
}

void TelemetryProviderCrossfire::generateTelemetryGPSFrame(uint8_t *packet, double latitude, double longitude, double ground_speed, double ground_course, double altitude, int satellite_count)
{
  int32_t tmp;
  int16_t tmp2;
  uint16_t tmp3;

  packet[0] = 0xc8;
  packet[1] = 17; // LEN(15 + 2 CRC)
  packet[2] = 0x02; // CRSF_FRAMETYPE_GPS

  tmp = latitude * 10000000;
  packet[3] = (tmp & 0xff000000) >> 24;
  packet[4] = (tmp & 0xff0000) >> 16;
  packet[5] = (tmp & 0xff00) >> 8;
  packet[6] = tmp & 0xff;

  tmp = longitude * 10000000;
  packet[7] = (tmp & 0xff000000) >> 24;
  packet[8] = (tmp & 0xff0000) >> 16;
  packet[9] = (tmp & 0xff00) >> 8;
  packet[10] = tmp & 0xff;

  tmp2 = ground_speed * 10; // 10ths of a kph
  packet[11] = (tmp2 & 0xff00) >> 8;
  packet[12] = tmp2 & 0xff;

  tmp2 = ground_course;
  if (ground_course > 180) 
    tmp2 = (ground_course - 360); // -180..180, not 0-360
  tmp2 *= 100; // 100ths of a degree

  packet[13] = (tmp2 & 0xff00) >> 8;
  packet[14] = tmp2 & 0xff;
  
  tmp3 = altitude + 1000;
  packet[15] = (tmp3 & 0xff00) >> 8;
  packet[16] = tmp3 & 0xff;

  packet[17] = satellite_count & 0xff;
  // Don't bother calculating the CRC, the telemetry consumer doesn't check it
}

void TelemetryProviderCrossfire::on_button_gpsRunStop_clicked()
{
  if (gps.running) {
    gps.stop();
    ui->button_gpsRunStop->setText(tr("Run"));
  } else {
    gps.start();
    ui->button_gpsRunStop->setText(tr("Stop"));
  }
}

void TelemetryProviderCrossfire::on_button_saveTelemetryValues_clicked()
{
  QString fldr = g.backupDir().trimmed();
  if (fldr.isEmpty())
    fldr = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

  QString idFileNameAndPath = QFileDialog::getSaveFileName(this, tr("Save Telemetry"), fldr % "/telemetry.tlm", tr(".tlm Files (*.tlm)"));
  if (idFileNameAndPath.isEmpty())
    return;

  QFile file(idFileNameAndPath);
  if (!file.open(QIODevice::WriteOnly)){
    QMessageBox::critical(this, CPN_STR_APP_NAME, tr("Unable to open file for writing.\n%1").arg(file.errorString()));
    return;
  }
  QTextStream out(&file);

  out << getLogfileIdentifier();
  out << "\r\n";
  out << ui->input_1rss->text();
  out << "\r\n";
  out << ui->input_2rss->text();
  out << "\r\n";
  out << ui->input_rqly->text();
  out << "\r\n";
  out << ui->input_rsnr->text();
  out << "\r\n";
  out << ui->input_trss->text();
  out << "\r\n";
  out << ui->input_tpwr->currentText();
  out << "\r\n";
  out << ui->input_rfmd->text();
  out << "\r\n";
  out << ui->input_ant->text();
  out << "\r\n";
  out << ui->input_tqly->text();
  out << "\r\n";
  out << ui->input_tsnr->text();
  out << "\r\n";
  out << ui->input_rrsp->text();
  out << "\r\n";
  out << ui->input_rpwr->text();
  out << "\r\n";
  out << ui->input_trsp->text();
  out << "\r\n";
  out << ui->enabled_battery->isChecked();
  out << "\r\n";
  out << ui->input_rxbt->text();
  out << "\r\n";
  out << ui->input_curr->text();
  out << "\r\n";
  out << ui->input_capa->text();
  out << "\r\n";
  out << ui->input_batpercent->text();
  out << "\r\n";
  out << ui->enabled_gps->isChecked();
  out << "\r\n";
  out << ui->input_gps->text();
  out << "\r\n";
  out << ui->input_gspd->text();
  out << "\r\n";
  out << ui->input_hdg->text();
  out << "\r\n";
  out << ui->input_sats->text();
  out << "\r\n";
  out << ui->enabled_attitude->isChecked();
  out << "\r\n";
  out << ui->input_ptch->text();
  out << "\r\n";
  out << ui->input_roll->text();
  out << "\r\n";
  out << ui->input_yaw->text();
  out << "\r\n";
  out << ui->enabled_flightcontroller->isChecked();
  out << "\r\n";
  out << ui->input_fm->text();
  out << "\r\n";
  out << ui->enabled_barometer->isChecked();
  out << "\r\n";
  out << ui->input_vspd->text();
  out << "\r\n";
  out << ui->input_alt->text();
  out << "\r\n";
  file.flush();
  file.close();
}

void TelemetryProviderCrossfire::on_button_loadTelemetryValues_clicked()
{
  QString fldr = g.backupDir().trimmed();
  if (fldr.isEmpty())
    fldr = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

  QString idFileNameAndPath = QFileDialog::getOpenFileName(this, tr("Open Telemetry File"), fldr % "/telemetry.tlm", tr(".tlm Files (*.tlm)"));
  if (idFileNameAndPath.isEmpty())
    return;

  QFile file(idFileNameAndPath);

  if (!file.open(QIODevice::ReadOnly)){
    QMessageBox::critical(this, CPN_STR_APP_NAME, tr("Unable to open file for reading.\n%1").arg(file.errorString()));
    return;
  }

  QTextStream in(&file);

  QString inputText;
  double inputDouble;
  int inputInt;

  inputText = in.readLine();
  if (inputText != getLogfileIdentifier()) {
    QMessageBox::critical(this, CPN_STR_APP_NAME, tr("Not a CRSF telemetry values file."));
    return;
  }

  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_1rss->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_2rss->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_rqly->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_rsnr->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_trss->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); setTpwrFromAnyText(ui->input_tpwr, inputText);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_rfmd->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_ant->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_tqly->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_tsnr->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_rrsp->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_rpwr->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_trsp->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->enabled_battery->setChecked(inputInt);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_rxbt->setValue(inputDouble);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_curr->setValue(inputDouble);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_capa->setValue(inputDouble);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_batpercent->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->enabled_gps->setChecked(inputInt);
  inputText = in.readLine(); ui->input_gps->setText(inputText);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_gspd->setValue(inputDouble);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_hdg->setValue(inputDouble);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->input_sats->setValue(inputInt);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->enabled_attitude->setChecked(inputInt);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_ptch->setValue(inputDouble);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_roll->setValue(inputDouble);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_yaw->setValue(inputDouble);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->enabled_flightcontroller->setChecked(inputInt);
  inputText = in.readLine(); ui->input_fm->setText(inputText);
  inputText = in.readLine(); inputInt = inputText.toInt(); ui->enabled_barometer->setChecked(inputInt);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_vspd->setValue(inputDouble);
  inputText = in.readLine(); inputDouble = inputText.toDouble(); ui->input_alt->setValue(inputDouble);

  file.close();
}
