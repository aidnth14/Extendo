// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include "citra_qt/applets/mii_selector.h"
#include "citra_qt/mii_maker/mii_maker_dialog.h"
#include "citra_qt/mii_maker/mii_manager.h"
#include "citra_qt/mii_maker/mii_render.h"
#include "common/string_util.h"
#include <functional>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

static QIcon MakeGlyphIcon(const QString& glyph, const QColor& bg_color, int size = 22) {
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg_color);
    p.drawEllipse(0, 0, size, size);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(11);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, glyph);
    return QIcon(pix);
}

QtMiiSelectorDialog::QtMiiSelectorDialog(QWidget* parent, QtMiiSelector* mii_selector_)
    : QDialog(parent), mii_selector(mii_selector_) {
    using namespace Frontend;
    const auto config = mii_selector->config;

    setMinimumSize(680, 480);
    setStyleSheet(QStringLiteral(
        "QDialog {"
        "  background-color: #1A1D24;"
        "  color: #FFFFFF;"
        "  font-family: -apple-system, 'SF Pro Display', 'Roboto', sans-serif;"
        "}"));

    const QString win_title = config.title.empty() || config.title.at(0) == '\x0000'
                                  ? tr("Select a Mii")
                                  : QString::fromStdString(config.title);
    setWindowTitle(win_title);

    combobox = new QComboBox(this);
    combobox->setVisible(false);

    // Load available Miis from 3DS NAND
    miis.push_back(HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data);
    combobox->addItem(tr("Standard Mii"));

    for (const auto& mii : Frontend::LoadMiis()) {
        miis.push_back(mii);
        combobox->addItem(QString::fromStdString(Common::UTF16BufferToUTF8(mii.mii_name)));
    }

    if (static_cast<int>(config.initially_selected_mii_index) < static_cast<int>(miis.size())) {
        selected_index = static_cast<int>(config.initially_selected_mii_index);
        combobox->setCurrentIndex(selected_index);
    }

    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(28, 24, 28, 20);
    root_layout->setSpacing(16);

    // Title & Subtitle
    auto* title_lbl = new QLabel(win_title, this);
    title_lbl->setStyleSheet(QStringLiteral(
        "font-size: 22px; font-weight: 700; color: #FFFFFF; background: transparent;"));
    root_layout->addWidget(title_lbl);

    auto* sub_lbl = new QLabel(tr("Choose a character to use in this software:"), this);
    sub_lbl->setStyleSheet(QStringLiteral(
        "font-size: 13px; color: rgba(255, 255, 255, 0.65); background: transparent; margin-top: -8px;"));
    root_layout->addWidget(sub_lbl);

    // Divider
    auto* div = new QFrame(this);
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet(QStringLiteral(
        "background-color: rgba(255, 255, 255, 0.10); height: 1px; min-height: 1px; max-height: 1px; border: none;"));
    root_layout->addWidget(div);

    // Scroll area with Mii Cards
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(255, 255, 255, 0.25); border-radius: 3px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"));

    auto* grid_widget = new QWidget(scroll);
    grid_widget->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto* grid_layout = new QGridLayout(grid_widget);
    grid_layout->setContentsMargins(4, 8, 4, 8);
    grid_layout->setSpacing(14);

    std::vector<QPushButton*> mii_cards;
    std::vector<QLabel*> mii_names;
    std::vector<QLabel*> mii_checks;

    auto update_selection_styles = [&](int sel) {
        selected_index = sel;
        combobox->setCurrentIndex(sel);
        for (int i = 0; i < static_cast<int>(mii_cards.size()); ++i) {
            bool is_sel = (i == sel);
            mii_checks[i]->setVisible(is_sel);
            if (is_sel) {
                // Luminance Inversion: solid white card with dark text
                mii_cards[i]->setStyleSheet(QStringLiteral(
                    "QPushButton {"
                    "  background-color: #FFFFFF;"
                    "  border-radius: 16px;"
                    "  border: none;"
                    "}"));
                mii_names[i]->setStyleSheet(QStringLiteral(
                    "font-size: 14px; font-weight: 700; color: #14171F; background: transparent;"));
            } else {
                mii_cards[i]->setStyleSheet(QStringLiteral(
                    "QPushButton {"
                    "  background-color: rgba(255, 255, 255, 0.08);"
                    "  border-radius: 16px;"
                    "  border: none;"
                    "}"
                    "QPushButton:hover {"
                    "  background-color: rgba(255, 255, 255, 0.16);"
                    "}"));
                mii_names[i]->setStyleSheet(QStringLiteral(
                    "font-size: 14px; font-weight: 600; color: #FFFFFF; background: transparent;"));
            }
        }
    };

    std::function<void()> rebuild_grid;
    rebuild_grid = [&]() {
        // Clear previous grid
        while (QLayoutItem* item = grid_layout->takeAt(0)) {
            if (QWidget* w = item->widget()) {
                w->deleteLater();
            }
            delete item;
        }
        mii_cards.clear();
        mii_names.clear();
        mii_checks.clear();

        constexpr int Columns = 4;
        int row = 0, col = 0;

        for (int i = 0; i < static_cast<int>(miis.size()); ++i) {
            auto* card = new QPushButton(grid_widget);
            card->setFixedSize(140, 150);
            card->setCursor(Qt::PointingHandCursor);

            auto* c_ly = new QVBoxLayout(card);
            c_ly->setContentsMargins(8, 10, 8, 10);
            c_ly->setSpacing(6);
            c_ly->setAlignment(Qt::AlignCenter);

            // Cel-shaded Mii preview
            auto* icon_lbl = new QLabel(card);
            icon_lbl->setFixedSize(88, 88);
            icon_lbl->setAlignment(Qt::AlignCenter);
            icon_lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            QPixmap pix = MiiMaker::RenderMii(miis[i], 88, 88);
            icon_lbl->setPixmap(pix);
            c_ly->addWidget(icon_lbl, 0, Qt::AlignCenter);

            QString nm = (i == 0) ? tr("Standard Mii") : MiiMaker::MiiManager::GetMiiName(miis[i]);
            if (nm.isEmpty()) nm = tr("Mii %1").arg(i + 1);

            auto* name_lbl = new QLabel(nm, card);
            name_lbl->setAlignment(Qt::AlignCenter);
            name_lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            c_ly->addWidget(name_lbl, 0, Qt::AlignCenter);

            auto* chk = new QLabel(QString::fromUtf8("✓"), card);
            chk->setAlignment(Qt::AlignCenter);
            chk->setAttribute(Qt::WA_TransparentForMouseEvents);
            chk->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; color: #14171F;"));
            chk->setVisible(i == selected_index);
            c_ly->addWidget(chk, 0, Qt::AlignCenter);

            mii_cards.push_back(card);
            mii_names.push_back(name_lbl);
            mii_checks.push_back(chk);

            grid_layout->addWidget(card, row, col);
            col++;
            if (col >= Columns) {
                col = 0;
                row++;
            }

            QObject::connect(card, &QPushButton::clicked, [&update_selection_styles, i] {
                update_selection_styles(i);
            });
        }

        // Add [ + Create New Mii ] card
        auto* add_card = new QPushButton(grid_widget);
        add_card->setFixedSize(140, 150);
        add_card->setCursor(Qt::PointingHandCursor);
        add_card->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: rgba(255, 255, 255, 0.04);"
            "  border: 2px dashed rgba(255, 255, 255, 0.25);"
            "  border-radius: 16px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(255, 255, 255, 0.12);"
            "  border-color: #3C8CE0;"
            "}"));

        auto* add_ly = new QVBoxLayout(add_card);
        add_ly->setContentsMargins(8, 12, 8, 12);
        add_ly->setAlignment(Qt::AlignCenter);
        auto* plus_lbl = new QLabel(QString::fromUtf8("＋"), add_card);
        plus_lbl->setAlignment(Qt::AlignCenter);
        plus_lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        plus_lbl->setStyleSheet(QStringLiteral("font-size: 32px; color: #3C8CE0; background: transparent;"));
        add_ly->addWidget(plus_lbl);

        auto* add_text = new QLabel(tr("Create New"), add_card);
        add_text->setAlignment(Qt::AlignCenter);
        add_text->setAttribute(Qt::WA_TransparentForMouseEvents);
        add_text->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600; color: #FFFFFF; background: transparent;"));
        add_ly->addWidget(add_text);

        grid_layout->addWidget(add_card, row, col);

        QObject::connect(add_card, &QPushButton::clicked, this, [this, &rebuild_grid, &update_selection_styles] {
            MiiMaker::MiiMakerDialog mii_dlg(this);
            if (mii_dlg.Exec() == QDialog::Accepted) {
                Mii::MiiData created = mii_dlg.GetMii();
                miis.push_back(created);
                combobox->addItem(MiiMaker::MiiManager::GetMiiName(created));
                rebuild_grid();
                update_selection_styles(static_cast<int>(miis.size()) - 1);
            }
        });

        update_selection_styles(selected_index);
    };

    rebuild_grid();
    scroll->setWidget(grid_widget);
    root_layout->addWidget(scroll, 1);

    // Footer with Docked [A] OK / [B] Cancel Legend
    auto* footer_layout = new QHBoxLayout();
    footer_layout->setContentsMargins(0, 10, 0, 0);
    footer_layout->setSpacing(14);
    footer_layout->addStretch();

    if (config.enable_cancel_button) {
        auto* btn_cancel = new QPushButton(tr("Cancel"), this);
        btn_cancel->setIcon(MakeGlyphIcon(QStringLiteral("B"), QColor(0xE0, 0x3C, 0x3C)));
        btn_cancel->setIconSize(QSize(22, 22));
        btn_cancel->setCursor(Qt::PointingHandCursor);
        btn_cancel->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: rgba(255, 255, 255, 0.08);"
            "  color: #FFFFFF;"
            "  border: none;"
            "  border-radius: 18px;"
            "  padding: 8px 18px;"
            "  font-size: 14px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:hover { background: rgba(255, 255, 255, 0.16); }"));
        connect(btn_cancel, &QPushButton::clicked, this, [this] {
            return_code = 1;
            accept();
        });
        footer_layout->addWidget(btn_cancel);
    }

    auto* btn_ok = new QPushButton(tr("OK"), this);
    btn_ok->setIcon(MakeGlyphIcon(QStringLiteral("A"), QColor(0x3C, 0x8C, 0xE0)));
    btn_ok->setIconSize(QSize(22, 22));
    btn_ok->setCursor(Qt::PointingHandCursor);
    btn_ok->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #3C8CE0;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 18px;"
        "  padding: 8px 24px;"
        "  font-size: 14px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover { background: #529FF0; }"));
    connect(btn_ok, &QPushButton::clicked, this, [this] {
        return_code = 0;
        accept();
    });
    footer_layout->addWidget(btn_ok);

    root_layout->addLayout(footer_layout);
}

QtMiiSelector::QtMiiSelector(QWidget& parent_) : parent(parent_) {}

void QtMiiSelector::Setup(const Frontend::MiiSelectorConfig& config) {
    MiiSelector::Setup(config);
    QMetaObject::invokeMethod(this, "OpenDialog", Qt::BlockingQueuedConnection);
}

void QtMiiSelector::OpenDialog() {
    QtMiiSelectorDialog dialog(&parent, this);
    dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                          Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.exec();

    const auto index = dialog.GetSelectedIndex();
    LOG_INFO(Frontend, "Mii Selector dialog finished (return_code={}, index={})",
             dialog.return_code, index);

    const auto mii_data = (index >= 0 && index < static_cast<int>(dialog.miis.size()))
                              ? dialog.miis.at(index)
                              : dialog.miis.at(0);
    Finalize(dialog.return_code, dialog.return_code == 0 ? std::move(mii_data) : Mii::MiiData{});
}
