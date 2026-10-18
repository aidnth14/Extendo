// Copyright 2026 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include "citra_qt/mii_maker/mii_maker_dialog.h"
#include "citra_qt/uisettings.h"

#include <algorithm>
#include <QApplication>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QStyleHints>
#include <QVBoxLayout>
#include "common/file_util.h"

namespace MiiMaker {

namespace {

// Loose files under Contents/Resources/avatar_assets/<category>/, imported
// from the user's own game dump rather than drawn — see dist/avatar_assets.
QString MiiAssetPath(const QString& category, const QString& file_name) {
    const QString app_dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MAC
    return app_dir + QStringLiteral("/../Resources/avatar_assets/") + category +
           QStringLiteral("/") + file_name;
#else
    return app_dir + QStringLiteral("/avatar_assets/") + category +
           QStringLiteral("/") + file_name;
#endif
}

struct AssetOption {
    QString file;
    QString label;
};

const std::vector<AssetOption> HAIR_ASSETS = {
    {QStringLiteral("hair_0.png"), QStringLiteral("Share 00300")},
    {QStringLiteral("hair_1.png"), QStringLiteral("Share 00306")},
    {QStringLiteral("hair_2.png"), QStringLiteral("Share 00639")},
    {QStringLiteral("hair_3.png"), QStringLiteral("Share 00645")},
    {QStringLiteral("hair_4.png"), QStringLiteral("Share 00651")},
    {QStringLiteral("hair_5.png"), QStringLiteral("Share 00657")},
};

const std::vector<AssetOption> EYEBROW_ASSETS = {
    {QStringLiteral("eyebrow_0.png"), QStringLiteral("Share 00535")},
    {QStringLiteral("eyebrow_1.png"), QStringLiteral("Share 00537")},
    {QStringLiteral("eyebrow_2.png"), QStringLiteral("Share 00540")},
    {QStringLiteral("eyebrow_3.png"), QStringLiteral("Share 00542")},
    {QStringLiteral("eyebrow_4.png"), QStringLiteral("Share 00545")},
    {QStringLiteral("eyebrow_5.png"), QStringLiteral("Share 00547")},
};

const std::vector<AssetOption> NOSE_ASSETS = {
    {QStringLiteral("nose_0.png"), QStringLiteral("Share 00583")},
    {QStringLiteral("nose_1.png"), QStringLiteral("Share 00585")},
    {QStringLiteral("nose_2.png"), QStringLiteral("Share 00586")},
    {QStringLiteral("nose_3.png"), QStringLiteral("Share 00587")},
    {QStringLiteral("nose_4.png"), QStringLiteral("Share 00590")},
    {QStringLiteral("nose_5.png"), QStringLiteral("Share 00591")},
};

const std::vector<AssetOption> MUSTACHE_ASSETS = {
    {QStringLiteral("mustache_0.png"), QStringLiteral("ss_000")},
    {QStringLiteral("mustache_1.png"), QStringLiteral("ss_003")},
    {QStringLiteral("mustache_2.png"), QStringLiteral("ss_006")},
    {QStringLiteral("mustache_3.png"), QStringLiteral("ss_012")},
};

const std::vector<AssetOption> BEARD_ASSETS = {
    {QStringLiteral("beard_0.png"), QStringLiteral("ss_001")},
};

bool IsCurrentThemeLight() {
    const QString& theme_name = UISettings::values.theme;
    if (theme_name == QStringLiteral("white")) {
        return true;
    }
    if (theme_name.isEmpty() || theme_name == QStringLiteral("default")) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (const auto* hints = QGuiApplication::styleHints()) {
            return hints->colorScheme() != Qt::ColorScheme::Dark;
        }
#endif
        return QGuiApplication::palette().color(QPalette::Window).lightness() >= 128;
    }
    return false;
}

QIcon MakeGlyphIcon(const QString& glyph, const QColor& bg_color, int size = 22) {
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

} // namespace

MiiTheme ComputeMiiTheme() {
    MiiTheme t;
    t.is_light = IsCurrentThemeLight();

    if (t.is_light) {
        t.backdrop = QColor(20, 22, 26, 140);
        t.panel_bg = QStringLiteral("#FFFFFF");
        t.panel_border = QStringLiteral("rgba(0, 0, 0, 0.10)");
        t.text_primary = QStringLiteral("#16181D");
        t.text_secondary = QStringLiteral("rgba(22, 24, 29, 0.68)");
        t.text_faint = QStringLiteral("rgba(22, 24, 29, 0.45)");

        t.rail_item_bg = QStringLiteral("transparent");
        t.rail_item_hover = QStringLiteral("rgba(0, 0, 0, 0.06)");
        t.rail_item_text = QStringLiteral("rgba(22, 24, 29, 0.70)");
        t.rail_item_sel_bg = QStringLiteral("#16181D");
        t.rail_item_sel_text = QStringLiteral("#FFFFFF");

        t.card_bg = QStringLiteral("rgba(0, 0, 0, 0.045)");
        t.card_hover = QStringLiteral("rgba(0, 0, 0, 0.09)");
        t.card_text = QStringLiteral("#16181D");
        t.card_sel_bg = QStringLiteral("#16181D");
        t.card_sel_text = QStringLiteral("#FFFFFF");

        t.swatch_border = QStringLiteral("rgba(0, 0, 0, 0.15)");
        t.swatch_sel_border = QStringLiteral("#16181D");

        t.input_bg = QStringLiteral("rgba(0, 0, 0, 0.045)");
        t.input_border = QStringLiteral("rgba(0, 0, 0, 0.14)");
        t.input_text = QStringLiteral("#16181D");

        t.accent = QStringLiteral("#2B6FD1");
        t.accent_hover = QStringLiteral("#3C8CE0");
        t.danger = QStringLiteral("#D32F2F");
        t.danger_hover = QStringLiteral("#E0433F");
    } else {
        t.backdrop = QColor(0, 0, 0, 160);
        t.panel_bg = QStringLiteral("#14171F");
        t.panel_border = QStringLiteral("rgba(255, 255, 255, 0.10)");
        t.text_primary = QStringLiteral("#FFFFFF");
        t.text_secondary = QStringLiteral("rgba(255, 255, 255, 0.65)");
        t.text_faint = QStringLiteral("rgba(255, 255, 255, 0.45)");

        t.rail_item_bg = QStringLiteral("transparent");
        t.rail_item_hover = QStringLiteral("rgba(255, 255, 255, 0.08)");
        t.rail_item_text = QStringLiteral("rgba(255, 255, 255, 0.70)");
        t.rail_item_sel_bg = QStringLiteral("#FFFFFF");
        t.rail_item_sel_text = QStringLiteral("#14171F");

        t.card_bg = QStringLiteral("rgba(255, 255, 255, 0.06)");
        t.card_hover = QStringLiteral("rgba(255, 255, 255, 0.14)");
        t.card_text = QStringLiteral("#FFFFFF");
        t.card_sel_bg = QStringLiteral("#FFFFFF");
        t.card_sel_text = QStringLiteral("#14171F");

        t.swatch_border = QStringLiteral("rgba(255, 255, 255, 0.20)");
        t.swatch_sel_border = QStringLiteral("#FFFFFF");

        t.input_bg = QStringLiteral("rgba(255, 255, 255, 0.08)");
        t.input_border = QStringLiteral("rgba(255, 255, 255, 0.15)");
        t.input_text = QStringLiteral("#FFFFFF");

        t.accent = QStringLiteral("#3C8CE0");
        t.accent_hover = QStringLiteral("#529FF0");
        t.danger = QStringLiteral("#E03C3C");
        t.danger_hover = QStringLiteral("#EB5555");
    }
    return t;
}

MiiMakerDialog::MiiMakerDialog(QWidget* parent, const Mii::MiiData* initial_mii)
    : QWidget(ResolveParent(parent)) {
    if (initial_mii) {
        mii = *initial_mii;
    } else {
        mii = MiiManager::CreateDefaultMii(UISettings::values.profile_nickname.isEmpty()
                                               ? QStringLiteral("Hero")
                                               : UISettings::values.profile_nickname);
    }

    theme = ComputeMiiTheme();

    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        parentWidget()->installEventFilter(this);
    } else {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        resize(1040, 700);
    }
    setFocusPolicy(Qt::StrongFocus);

    SetupUI();
    UpdatePreview();
}

MiiMakerDialog::~MiiMakerDialog() {
    if (parentWidget()) {
        parentWidget()->removeEventFilter(this);
    }
    if (loop && loop->isRunning()) {
        loop->quit();
    }
}

QWidget* MiiMakerDialog::ResolveParent(QWidget* parent) {
    if (parent) {
        if (auto* mw = qobject_cast<QMainWindow*>(parent)) {
            if (mw->centralWidget()) {
                return mw->centralWidget();
            }
        }
        if (auto* w = parent->window()) {
            if (auto* mw = qobject_cast<QMainWindow*>(w)) {
                if (mw->centralWidget()) {
                    return mw->centralWidget();
                }
            }
            return w;
        }
        return parent;
    }
    if (auto* w = QApplication::activeWindow()) {
        if (auto* mw = qobject_cast<QMainWindow*>(w)) {
            if (mw->centralWidget()) {
                return mw->centralWidget();
            }
        }
        return w;
    }
    return nullptr;
}

int MiiMakerDialog::Exec() {
    show();
    raise();
    setFocus();

    QEventLoop event_loop;
    loop = &event_loop;
    event_loop.exec();
    loop = nullptr;

    return result;
}

void MiiMakerDialog::Accept() {
    result = 1; // QDialog::Accepted
    hide();
    if (loop && loop->isRunning()) {
        loop->quit();
    }
}

void MiiMakerDialog::Reject() {
    result = 0; // QDialog::Rejected
    hide();
    if (loop && loop->isRunning()) {
        loop->quit();
    }
}

bool MiiMakerDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
    }
    return QWidget::eventFilter(watched, event);
}

void MiiMakerDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), theme.backdrop);
}

void MiiMakerDialog::SetupUI() {
    // Fixed-size card, sharp corners throughout (no border-radius anywhere in Mii Maker,
    // intentionally breaking from the rounded console_ui_taste.md language for this screen).
    card = new QWidget(this);
    card->setFixedSize(1040, 700);
    card->setStyleSheet(QStringLiteral(
        "background-color: %1;"
        "border: 1px solid %2;"
        "font-family: -apple-system, 'SF Pro Display', 'Roboto', 'Helvetica Neue', sans-serif;")
        .arg(theme.panel_bg, theme.panel_border));

    auto* root_layout = new QVBoxLayout(card);
    root_layout->setContentsMargins(24, 18, 24, 16);
    root_layout->setSpacing(14);

    // Top Header
    auto* header_layout = new QHBoxLayout();
    header_layout->setSpacing(10);
    auto* icon_header = new QLabel(card);
    icon_header->setPixmap(MakeGlyphIcon(QStringLiteral("M"), QColor(theme.accent)).pixmap(24, 24));
    header_layout->addWidget(icon_header);

    auto* title_lbl = new QLabel(tr("Avatar Studio"), card);
    title_lbl->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 700; color: %1; background: transparent; letter-spacing: 0.3px;")
        .arg(theme.text_primary));
    header_layout->addWidget(title_lbl);
    header_layout->addStretch();

    auto* status_lbl = new QLabel(tr("Extendo Profile"), card);
    status_lbl->setStyleSheet(QStringLiteral(
        "font-size: 13px; font-weight: 500; color: %1; background: transparent;")
        .arg(theme.text_faint));
    header_layout->addWidget(status_lbl);
    root_layout->addLayout(header_layout);

    // =========================================================================
    // Main layout: [ Category rail ] [ Character stage, with parts underneath ]
    // =========================================================================
    auto* main_layout = new QHBoxLayout();
    main_layout->setSpacing(18);

    // ---- Left: vertical step rail ----
    auto* rail_container = new QWidget(card);
    rail_container->setFixedWidth(168);
    rail_container->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    CreateCategoryRail(rail_container); // installs its own QVBoxLayout on rail_container
    main_layout->addWidget(rail_container);

    // ---- Right: character stage on top, parts grid underneath ----
    auto* stage_column = new QWidget(card);
    stage_column->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto* stage_column_ly = new QVBoxLayout(stage_column);
    stage_column_ly->setContentsMargins(0, 0, 0, 0);
    stage_column_ly->setSpacing(12);

    // Character preview, centered
    auto* stage_row = new QHBoxLayout();
    stage_row->addStretch();

    auto* stage_box = new QWidget(stage_column);
    stage_box->setFixedSize(300, 230);
    stage_box->setStyleSheet(QStringLiteral(
        "background: %1; border: 1px solid %2;").arg(theme.card_bg, theme.panel_border));
    auto* stage_ly = new QVBoxLayout(stage_box);
    stage_ly->setContentsMargins(0, 0, 0, 0);
    preview_label = new QLabel(stage_box);
    preview_label->setFixedSize(300, 230);
    preview_label->setAlignment(Qt::AlignCenter);
    preview_label->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    stage_ly->addWidget(preview_label, 0, Qt::AlignCenter);
    stage_row->addWidget(stage_box);
    stage_row->addStretch();
    stage_column_ly->addLayout(stage_row);

    // Name / gender / angle row, centered under the stage
    auto* identity_row = new QHBoxLayout();
    identity_row->addStretch();
    name_display = new QLabel(stage_column);
    name_display->setStyleSheet(QStringLiteral(
        "font-size: 16px; font-weight: 700; color: %1; background: transparent;").arg(theme.text_primary));
    identity_row->addWidget(name_display);
    gender_badge = new QLabel(stage_column);
    gender_badge->setStyleSheet(QStringLiteral(
        "background: %1; color: %2; font-size: 12px; font-weight: 600; padding: 3px 10px; margin-left: 8px;")
        .arg(theme.card_bg, theme.text_primary));
    identity_row->addWidget(gender_badge);
    identity_row->addStretch();
    stage_column_ly->addLayout(identity_row);

    auto* tilt_box = new QWidget(stage_column);
    tilt_box->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto* tilt_ly = new QHBoxLayout(tilt_box);
    tilt_ly->setContentsMargins(60, 0, 60, 0);
    tilt_ly->setSpacing(10);
    auto* tilt_lbl = new QLabel(tr("Angle:"), tilt_box);
    tilt_lbl->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme.text_secondary));
    tilt_slider = new QSlider(Qt::Horizontal, tilt_box);
    tilt_slider->setRange(-40, 40);
    tilt_slider->setValue(0);
    tilt_slider->setCursor(Qt::PointingHandCursor);
    tilt_slider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 4px; background: %1; }"
        "QSlider::sub-page:horizontal { background: %2; }"
        "QSlider::handle:horizontal { background: %3; width: 14px; margin-top: -5px; margin-bottom: -5px; }")
        .arg(theme.card_bg, theme.accent, theme.text_primary));
    connect(tilt_slider, &QSlider::valueChanged, this, [this](int val) {
        current_tilt = val;
        UpdatePreview();
    });
    tilt_ly->addWidget(tilt_lbl);
    tilt_ly->addWidget(tilt_slider, 1);
    stage_column_ly->addWidget(tilt_box);

    // Divider between the character and the parts grid underneath it
    auto* divider = new QFrame(stage_column);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("background: %1; border: none; max-height: 1px;")
        .arg(theme.panel_border));
    stage_column_ly->addWidget(divider);

    // Parts grid, directly underneath the character
    pages_stack = new QStackedWidget(stage_column);
    pages_stack->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    pages_stack->addWidget(CreateFacePage());
    pages_stack->addWidget(CreateHairPage());
    pages_stack->addWidget(CreateEyebrowsPage());
    pages_stack->addWidget(CreateEyesPage());
    pages_stack->addWidget(CreateNosePage());
    pages_stack->addWidget(CreateMouthPage());
    pages_stack->addWidget(CreateFacialHairPage());
    pages_stack->addWidget(CreateGlassesPage());
    pages_stack->addWidget(CreateBodyPage());
    pages_stack->addWidget(CreateProfilePage());
    stage_column_ly->addWidget(pages_stack, 1);

    main_layout->addWidget(stage_column, 1);
    root_layout->addLayout(main_layout, 1);

    // =========================================================================
    // Bottom docked legend
    // =========================================================================
    auto* footer_layout = new QHBoxLayout();
    footer_layout->setContentsMargins(0, 6, 0, 0);
    footer_layout->setSpacing(14);
    footer_layout->addStretch();

    auto* btn_cancel = new QPushButton(tr("Cancel"), card);
    btn_cancel->setIcon(MakeGlyphIcon(QStringLiteral("B"), QColor(theme.danger)));
    btn_cancel->setIconSize(QSize(22, 22));
    btn_cancel->setCursor(Qt::PointingHandCursor);
    btn_cancel->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none; padding: 8px 18px; font-size: 14px; font-weight: 600; }"
        "QPushButton:hover { background: %3; }")
        .arg(theme.card_bg, theme.text_primary, theme.card_hover));
    connect(btn_cancel, &QPushButton::clicked, this, &MiiMakerDialog::OnCancelClicked);
    footer_layout->addWidget(btn_cancel);

    auto* btn_rand = new QPushButton(tr("Randomize"), card);
    btn_rand->setIcon(MakeGlyphIcon(QStringLiteral("Y"), QColor(0xE6, 0xA1, 0x18)));
    btn_rand->setIconSize(QSize(22, 22));
    btn_rand->setCursor(Qt::PointingHandCursor);
    btn_rand->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none; padding: 8px 18px; font-size: 14px; font-weight: 600; }"
        "QPushButton:hover { background: %3; }")
        .arg(theme.card_bg, theme.text_primary, theme.card_hover));
    connect(btn_rand, &QPushButton::clicked, this, &MiiMakerDialog::OnRandomizeClicked);
    footer_layout->addWidget(btn_rand);

    auto* btn_save = new QPushButton(tr("Save Mii"), card);
    btn_save->setIcon(MakeGlyphIcon(QStringLiteral("A"), QColor(theme.accent)));
    btn_save->setIconSize(QSize(22, 22));
    btn_save->setCursor(Qt::PointingHandCursor);
    btn_save->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: #FFFFFF; border: none; padding: 8px 24px; font-size: 14px; font-weight: 700; }"
        "QPushButton:hover { background: %2; }")
        .arg(theme.accent, theme.accent_hover));
    connect(btn_save, &QPushButton::clicked, this, &MiiMakerDialog::OnSaveClicked);
    footer_layout->addWidget(btn_save);

    root_layout->addLayout(footer_layout);

    // Center the card within the overlay
    auto* overlay_layout = new QVBoxLayout(this);
    overlay_layout->addStretch();
    auto* center_row = new QHBoxLayout();
    center_row->addStretch();
    center_row->addWidget(card);
    center_row->addStretch();
    overlay_layout->addLayout(center_row);
    overlay_layout->addStretch();

    SwitchCategory(0);
}

void MiiMakerDialog::CreateCategoryRail(QWidget* parent_widget) {
    auto* rail_ly = new QVBoxLayout(parent_widget);
    rail_ly->setContentsMargins(0, 0, 0, 0);
    rail_ly->setSpacing(2);

    const QStringList cats = {
        tr("Face"), tr("Hair"), tr("Eyebrows"), tr("Eyes"), tr("Nose"),
        tr("Mouth"), tr("Beard"), tr("Glasses"), tr("Body"), tr("Info"),
    };

    for (int i = 0; i < cats.size(); ++i) {
        auto* btn = new QPushButton(cats[i], parent_widget);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(38);
        cat_buttons.push_back(btn);
        connect(btn, &QPushButton::clicked, this, [this, i] { SwitchCategory(i); });
        rail_ly->addWidget(btn);
    }
    rail_ly->addStretch();
}

void MiiMakerDialog::SwitchCategory(int index) {
    if (pages_stack) {
        pages_stack->setCurrentIndex(index);
    }
    for (int i = 0; i < static_cast<int>(cat_buttons.size()); ++i) {
        const bool sel = (i == index);
        cat_buttons[i]->setStyleSheet(QStringLiteral(
            "QPushButton { text-align: left; padding: 8px 14px; font-weight: %1; font-size: 13px;"
            " background-color: %2; color: %3; border: none; }"
            "QPushButton:hover { background-color: %4; }")
            .arg(sel ? QStringLiteral("700") : QStringLiteral("600"),
                 sel ? theme.rail_item_sel_bg : theme.rail_item_bg,
                 sel ? theme.rail_item_sel_text : theme.rail_item_text,
                 theme.rail_item_hover));
    }
}

void MiiMakerDialog::RestyleOptionCard(QPushButton* btn, const QString& title, bool is_selected) {
    btn->setText(is_selected ? QStringLiteral("%1  ✓").arg(title) : title);
    btn->setStyleSheet(is_selected
        ? QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 13px;"
                         " font-weight: 700; border: none; padding: 8px 14px; }")
              .arg(theme.card_sel_bg, theme.card_sel_text)
        : QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 13px;"
                         " font-weight: 600; border: none; padding: 8px 14px; }"
                         "QPushButton:hover { background-color: %3; }")
              .arg(theme.card_bg, theme.card_text, theme.card_hover));
}

QPushButton* MiiMakerDialog::MakeOptionCard(const QString& title, bool is_selected) {
    auto* btn = new QPushButton();
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(42);
    RestyleOptionCard(btn, title, is_selected);
    return btn;
}

QPushButton* MiiMakerDialog::MakeImageOptionCard(const QString& category, const QString& asset_file,
                                                  const QString& label, bool is_selected) {
    auto* btn = new QPushButton();
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(96);
    btn->setIconSize(QSize(56, 56));
    const QPixmap thumb(MiiAssetPath(category, asset_file));
    if (!thumb.isNull()) {
        btn->setIcon(QIcon(thumb));
    }
    btn->setText(QStringLiteral("  %1%2").arg(label, is_selected ? QStringLiteral("  ✓") : QString()));
    btn->setStyleSheet(is_selected
        ? QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                         " font-weight: 700; border: none; text-align: left; padding: 4px 8px; }")
              .arg(theme.card_sel_bg, theme.card_sel_text)
        : QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                         " font-weight: 600; border: none; text-align: left; padding: 4px 8px; }"
                         "QPushButton:hover { background-color: %3; }")
              .arg(theme.card_bg, theme.card_text, theme.card_hover));
    return btn;
}

void MiiMakerDialog::RestyleColorSwatch(QPushButton* btn, const QString& hex, bool is_selected, int radius) {
    Q_UNUSED(radius); // Sharp edges throughout Mii Maker, intentionally.
    btn->setText(is_selected ? QStringLiteral("✓") : QString());
    btn->setStyleSheet(is_selected
        ? QStringLiteral("QPushButton { background-color: %1; border: 3px solid %2; color: #FFFFFF; font-weight: bold; }")
              .arg(hex, theme.swatch_sel_border)
        : QStringLiteral("QPushButton { background-color: %1; border: 1px solid %2; }"
                         "QPushButton:hover { border: 2px solid %3; }")
              .arg(hex, theme.swatch_border, theme.swatch_sel_border));
}

QWidget* MiiMakerDialog::CreateFacePage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; }"
        "QScrollBar::handle:vertical { background: %1; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }")
        .arg(theme.swatch_border));
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(12);

    auto make_header = [this, page](const QString& text) {
        auto* hdr = new QLabel(text, page);
        hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
        return hdr;
    };

    ly->addWidget(make_header(tr("Face Shape")));
    auto* shape_grid = new QGridLayout();
    shape_grid->setSpacing(6);
    const QStringList shapes = {
        tr("Classic Round"), tr("Pointed Chin"), tr("Flat Jaw"), tr("Chiseled"),
        tr("Oval"), tr("Chubby Cheeks"), tr("Heart"), tr("Diamond")
    };
    std::vector<QPushButton*> shape_btns;
    const int cur_shape = mii.face_style.type.Value() % 8;
    for (int i = 0; i < shapes.size(); ++i) {
        auto* btn = MakeOptionCard(shapes[i], i == cur_shape);
        shape_btns.push_back(btn);
        shape_grid->addWidget(btn, i / 4, i % 4);
    }
    for (int i = 0; i < static_cast<int>(shape_btns.size()); ++i) {
        connect(shape_btns[i], &QPushButton::clicked, this, [this, i, shape_btns, shapes] {
            mii.face_style.type.Assign(i);
            for (int s = 0; s < static_cast<int>(shape_btns.size()); ++s) {
                RestyleOptionCard(shape_btns[s], shapes[s], s == i);
            }
            UpdatePreview();
        });
    }
    ly->addLayout(shape_grid);

    ly->addWidget(make_header(tr("Skin Tone")));
    auto* skin_row = new QHBoxLayout();
    skin_row->setSpacing(8);
    std::vector<QPushButton*> skin_btns;
    const int cur_skin = mii.face_style.skin_color.Value() % 6;
    for (int i = 0; i < 6; ++i) {
        auto* s_btn = new QPushButton(page);
        s_btn->setFixedSize(40, 34);
        s_btn->setCursor(Qt::PointingHandCursor);
        skin_btns.push_back(s_btn);
        RestyleColorSwatch(s_btn, QString::fromLatin1(SKIN_COLORS[i].hex), i == cur_skin, 0);
        skin_row->addWidget(s_btn);
    }
    for (int i = 0; i < static_cast<int>(skin_btns.size()); ++i) {
        connect(skin_btns[i], &QPushButton::clicked, this, [this, i, skin_btns] {
            mii.face_style.skin_color.Assign(i);
            for (int s = 0; s < 6; ++s) {
                RestyleColorSwatch(skin_btns[s], QString::fromLatin1(SKIN_COLORS[s].hex), s == i, 0);
            }
            UpdatePreview();
        });
    }
    skin_row->addStretch();
    ly->addLayout(skin_row);

    ly->addWidget(make_header(tr("Features & Cheeks")));
    auto* mk_row = new QHBoxLayout();
    mk_row->setSpacing(6);
    const QStringList mk_options = {tr("Clean"), tr("Cheek Blush"), tr("Freckles")};
    std::vector<QPushButton*> mk_btns;
    const int cur_mk = mii.face_details.makeup.Value() % 3;
    for (int i = 0; i < mk_options.size(); ++i) {
        auto* btn = MakeOptionCard(mk_options[i], i == cur_mk);
        mk_btns.push_back(btn);
        mk_row->addWidget(btn);
    }
    for (int i = 0; i < static_cast<int>(mk_btns.size()); ++i) {
        connect(mk_btns[i], &QPushButton::clicked, this, [this, i, mk_btns, mk_options] {
            mii.face_details.makeup.Assign(i);
            for (int s = 0; s < static_cast<int>(mk_btns.size()); ++s) {
                RestyleOptionCard(mk_btns[s], mk_options[s], s == i);
            }
            UpdatePreview();
        });
    }
    ly->addLayout(mk_row);
    ly->addStretch();

    scroll->setWidget(page);
    return scroll;
}

QWidget* MiiMakerDialog::CreateHairPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(12);

    auto* style_hdr = new QLabel(tr("Hairstyle"), page);
    style_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
    ly->addWidget(style_hdr);

    auto* hair_grid = new QGridLayout();
    hair_grid->setSpacing(6);
    std::vector<QPushButton*> hair_btns;
    const int cur_hair = mii.hair_style % static_cast<int>(HAIR_ASSETS.size());
    for (int i = 0; i < static_cast<int>(HAIR_ASSETS.size()); ++i) {
        auto* btn = MakeImageOptionCard(QStringLiteral("hair"), HAIR_ASSETS[i].file,
                                        HAIR_ASSETS[i].label, i == cur_hair);
        hair_btns.push_back(btn);
        hair_grid->addWidget(btn, i / 3, i % 3);
    }
    for (int i = 0; i < static_cast<int>(hair_btns.size()); ++i) {
        connect(hair_btns[i], &QPushButton::clicked, this, [this, i, hair_btns] {
            mii.hair_style = static_cast<u8>(i);
            for (int s = 0; s < static_cast<int>(hair_btns.size()); ++s) {
                const bool sel = (s == i);
                hair_btns[s]->setText(QStringLiteral("  %1%2").arg(
                    HAIR_ASSETS[s].label, sel ? QStringLiteral("  ✓") : QString()));
                hair_btns[s]->setStyleSheet(sel
                    ? QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                     " font-weight: 700; border: none; text-align: left; padding: 4px 8px; }")
                          .arg(theme.card_sel_bg, theme.card_sel_text)
                    : QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                     " font-weight: 600; border: none; text-align: left; padding: 4px 8px; }"
                                     "QPushButton:hover { background-color: %3; }")
                          .arg(theme.card_bg, theme.card_text, theme.card_hover));
            }
            UpdatePreview();
        });
    }
    ly->addLayout(hair_grid);

    auto* color_hdr = new QLabel(tr("Hair Color"), page);
    color_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1; margin-top: 6px;").arg(theme.text_primary));
    ly->addWidget(color_hdr);

    auto* col_row = new QHBoxLayout();
    col_row->setSpacing(6);
    std::vector<QPushButton*> col_btns;
    const int cur_color = mii.hair_details.color.Value() % 8;
    for (int i = 0; i < 8; ++i) {
        auto* c_btn = new QPushButton(page);
        c_btn->setFixedSize(36, 30);
        c_btn->setCursor(Qt::PointingHandCursor);
        col_btns.push_back(c_btn);
        RestyleColorSwatch(c_btn, QString::fromLatin1(HAIR_COLORS[i].hex), i == cur_color, 0);
        col_row->addWidget(c_btn);
    }
    for (int i = 0; i < static_cast<int>(col_btns.size()); ++i) {
        connect(col_btns[i], &QPushButton::clicked, this, [this, i, col_btns] {
            mii.hair_details.color.Assign(i);
            mii.eyebrow_details.color.Assign(i);
            for (int s = 0; s < 8; ++s) {
                RestyleColorSwatch(col_btns[s], QString::fromLatin1(HAIR_COLORS[s].hex), s == i, 0);
            }
            UpdatePreview();
        });
    }
    col_row->addStretch();
    ly->addLayout(col_row);

    auto* flip_btn = MakeOptionCard(tr("Flip Hair Orientation"), mii.hair_details.flip.Value() == 1);
    connect(flip_btn, &QPushButton::clicked, this, [this, flip_btn] {
        const int n_val = (mii.hair_details.flip.Value() == 1) ? 0 : 1;
        mii.hair_details.flip.Assign(n_val);
        RestyleOptionCard(flip_btn, tr("Flip Hair Orientation"), n_val == 1);
        UpdatePreview();
    });
    ly->addWidget(flip_btn);
    ly->addStretch();

    scroll->setWidget(page);
    return scroll;
}

QWidget* MiiMakerDialog::CreateEyebrowsPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(12);

    auto* style_hdr = new QLabel(tr("Eyebrow Shape"), page);
    style_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
    ly->addWidget(style_hdr);

    auto* brow_grid = new QGridLayout();
    brow_grid->setSpacing(6);
    std::vector<QPushButton*> brow_btns;
    const int cur_b = mii.eyebrow_details.style.Value() % static_cast<int>(EYEBROW_ASSETS.size());
    for (int i = 0; i < static_cast<int>(EYEBROW_ASSETS.size()); ++i) {
        auto* btn = MakeImageOptionCard(QStringLiteral("eyebrow"), EYEBROW_ASSETS[i].file,
                                        EYEBROW_ASSETS[i].label, i == cur_b);
        brow_btns.push_back(btn);
        brow_grid->addWidget(btn, i / 3, i % 3);
    }
    for (int i = 0; i < static_cast<int>(brow_btns.size()); ++i) {
        connect(brow_btns[i], &QPushButton::clicked, this, [this, i, brow_btns] {
            mii.eyebrow_details.style.Assign(i);
            for (int s = 0; s < static_cast<int>(brow_btns.size()); ++s) {
                const bool sel = (s == i);
                brow_btns[s]->setText(QStringLiteral("  %1%2").arg(
                    EYEBROW_ASSETS[s].label, sel ? QStringLiteral("  ✓") : QString()));
                brow_btns[s]->setStyleSheet(sel
                    ? QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                     " font-weight: 700; border: none; text-align: left; padding: 4px 8px; }")
                          .arg(theme.card_sel_bg, theme.card_sel_text)
                    : QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                     " font-weight: 600; border: none; text-align: left; padding: 4px 8px; }"
                                     "QPushButton:hover { background-color: %3; }")
                          .arg(theme.card_bg, theme.card_text, theme.card_hover));
            }
            UpdatePreview();
        });
    }
    ly->addLayout(brow_grid);

    auto* adj_hdr = new QLabel(tr("Position & Size"), page);
    adj_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1; margin-top: 6px;").arg(theme.text_primary));
    ly->addWidget(adj_hdr);

    auto make_slider_label = [this, page](const QString& text) {
        auto* l = new QLabel(text, page);
        l->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme.text_secondary));
        return l;
    };
    auto style_slider = [this](QSlider* s) {
        s->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: %1; }"
            "QSlider::sub-page:horizontal { background: %2; }"
            "QSlider::handle:horizontal { background: %3; width: 14px; margin-top: -5px; margin-bottom: -5px; }")
            .arg(theme.card_bg, theme.accent, theme.text_primary));
    };

    auto* s_y = new QSlider(Qt::Horizontal, page);
    s_y->setRange(6, 16);
    s_y->setValue(mii.eyebrow_details.y.Value());
    style_slider(s_y);
    connect(s_y, &QSlider::valueChanged, this, [this](int v) { mii.eyebrow_details.y.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Vertical Position:")));
    ly->addWidget(s_y);

    auto* s_x = new QSlider(Qt::Horizontal, page);
    s_x->setRange(0, 6);
    s_x->setValue(mii.eyebrow_details.x.Value());
    style_slider(s_x);
    connect(s_x, &QSlider::valueChanged, this, [this](int v) { mii.eyebrow_details.x.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Spacing:")));
    ly->addWidget(s_x);

    auto* s_r = new QSlider(Qt::Horizontal, page);
    s_r->setRange(0, 6);
    s_r->setValue(mii.eyebrow_details.rotate.Value());
    style_slider(s_r);
    connect(s_r, &QSlider::valueChanged, this, [this](int v) { mii.eyebrow_details.rotate.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Rotation:")));
    ly->addWidget(s_r);

    ly->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* MiiMakerDialog::CreateEyesPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(12);

    auto* style_hdr = new QLabel(tr("Eye Shape"), page);
    style_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
    ly->addWidget(style_hdr);

    auto* eye_grid = new QGridLayout();
    eye_grid->setSpacing(6);
    const QStringList eye_types = {
        tr("Classic Mii Oval"), tr("Minimalist Dot"), tr("Happy Arcs (^_^)"),
        tr("Almond Eyes"), tr("Round Open"), tr("Confident Smirk")
    };
    std::vector<QPushButton*> eye_btns;
    const int cur_e = mii.eye_details.type.Value() % 6;
    for (int i = 0; i < eye_types.size(); ++i) {
        auto* btn = MakeOptionCard(eye_types[i], i == cur_e);
        eye_btns.push_back(btn);
        eye_grid->addWidget(btn, i / 3, i % 3);
    }
    for (int i = 0; i < static_cast<int>(eye_btns.size()); ++i) {
        connect(eye_btns[i], &QPushButton::clicked, this, [this, i, eye_btns, eye_types] {
            mii.eye_details.type.Assign(i);
            for (int s = 0; s < static_cast<int>(eye_btns.size()); ++s) {
                RestyleOptionCard(eye_btns[s], eye_types[s], s == i);
            }
            UpdatePreview();
        });
    }
    ly->addLayout(eye_grid);

    auto* color_hdr = new QLabel(tr("Eye Color"), page);
    color_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1; margin-top: 6px;").arg(theme.text_primary));
    ly->addWidget(color_hdr);

    auto* col_row = new QHBoxLayout();
    col_row->setSpacing(6);
    std::vector<QPushButton*> col_btns;
    const int cur_color = mii.eye_details.color.Value() % 6;
    for (int i = 0; i < 6; ++i) {
        auto* c_btn = new QPushButton(page);
        c_btn->setFixedSize(36, 30);
        c_btn->setCursor(Qt::PointingHandCursor);
        col_btns.push_back(c_btn);
        RestyleColorSwatch(c_btn, QString::fromLatin1(EYE_COLORS[i].hex), i == cur_color, 0);
        col_row->addWidget(c_btn);
    }
    for (int i = 0; i < static_cast<int>(col_btns.size()); ++i) {
        connect(col_btns[i], &QPushButton::clicked, this, [this, i, col_btns] {
            mii.eye_details.color.Assign(i);
            for (int s = 0; s < 6; ++s) {
                RestyleColorSwatch(col_btns[s], QString::fromLatin1(EYE_COLORS[s].hex), s == i, 0);
            }
            UpdatePreview();
        });
    }
    col_row->addStretch();
    ly->addLayout(col_row);

    auto style_slider = [this](QSlider* s) {
        s->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: %1; }"
            "QSlider::sub-page:horizontal { background: %2; }"
            "QSlider::handle:horizontal { background: %3; width: 14px; margin-top: -5px; margin-bottom: -5px; }")
            .arg(theme.card_bg, theme.accent, theme.text_primary));
    };
    auto make_slider_label = [this, page](const QString& text) {
        auto* l = new QLabel(text, page);
        l->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme.text_secondary));
        return l;
    };

    auto* s_y = new QSlider(Qt::Horizontal, page);
    s_y->setRange(8, 16);
    s_y->setValue(mii.eye_details.y.Value());
    style_slider(s_y);
    connect(s_y, &QSlider::valueChanged, this, [this](int v) { mii.eye_details.y.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Vertical Position:")));
    ly->addWidget(s_y);

    auto* s_x = new QSlider(Qt::Horizontal, page);
    s_x->setRange(0, 6);
    s_x->setValue(mii.eye_details.x.Value());
    style_slider(s_x);
    connect(s_x, &QSlider::valueChanged, this, [this](int v) { mii.eye_details.x.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Spacing:")));
    ly->addWidget(s_x);

    auto* s_sz = new QSlider(Qt::Horizontal, page);
    s_sz->setRange(1, 6);
    s_sz->setValue(mii.eye_details.scale.Value());
    style_slider(s_sz);
    connect(s_sz, &QSlider::valueChanged, this, [this](int v) { mii.eye_details.scale.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Size:")));
    ly->addWidget(s_sz);

    ly->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* MiiMakerDialog::CreateNosePage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(12);

    auto* style_hdr = new QLabel(tr("Nose Shape"), page);
    style_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
    ly->addWidget(style_hdr);

    auto* nose_grid = new QGridLayout();
    nose_grid->setSpacing(6);
    std::vector<QPushButton*> nose_btns;
    const int cur_n = mii.nose_details.type.Value() % static_cast<int>(NOSE_ASSETS.size());
    for (int i = 0; i < static_cast<int>(NOSE_ASSETS.size()); ++i) {
        auto* btn = MakeImageOptionCard(QStringLiteral("nose"), NOSE_ASSETS[i].file,
                                        NOSE_ASSETS[i].label, i == cur_n);
        nose_btns.push_back(btn);
        nose_grid->addWidget(btn, i / 3, i % 3);
    }
    for (int i = 0; i < static_cast<int>(nose_btns.size()); ++i) {
        connect(nose_btns[i], &QPushButton::clicked, this, [this, i, nose_btns] {
            mii.nose_details.type.Assign(i);
            for (int s = 0; s < static_cast<int>(nose_btns.size()); ++s) {
                const bool sel = (s == i);
                nose_btns[s]->setText(QStringLiteral("  %1%2").arg(
                    NOSE_ASSETS[s].label, sel ? QStringLiteral("  ✓") : QString()));
                nose_btns[s]->setStyleSheet(sel
                    ? QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                     " font-weight: 700; border: none; text-align: left; padding: 4px 8px; }")
                          .arg(theme.card_sel_bg, theme.card_sel_text)
                    : QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                     " font-weight: 600; border: none; text-align: left; padding: 4px 8px; }"
                                     "QPushButton:hover { background-color: %3; }")
                          .arg(theme.card_bg, theme.card_text, theme.card_hover));
            }
            UpdatePreview();
        });
    }
    ly->addLayout(nose_grid);

    auto style_slider = [this](QSlider* s) {
        s->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: %1; }"
            "QSlider::sub-page:horizontal { background: %2; }"
            "QSlider::handle:horizontal { background: %3; width: 14px; margin-top: -5px; margin-bottom: -5px; }")
            .arg(theme.card_bg, theme.accent, theme.text_primary));
    };
    auto make_slider_label = [this, page](const QString& text) {
        auto* l = new QLabel(text, page);
        l->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme.text_secondary));
        return l;
    };

    auto* s_y = new QSlider(Qt::Horizontal, page);
    s_y->setRange(4, 14);
    s_y->setValue(mii.nose_details.y.Value());
    style_slider(s_y);
    connect(s_y, &QSlider::valueChanged, this, [this](int v) { mii.nose_details.y.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Vertical Position:")));
    ly->addWidget(s_y);

    auto* s_sz = new QSlider(Qt::Horizontal, page);
    s_sz->setRange(1, 5);
    s_sz->setValue(mii.nose_details.scale.Value());
    style_slider(s_sz);
    connect(s_sz, &QSlider::valueChanged, this, [this](int v) { mii.nose_details.scale.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Size:")));
    ly->addWidget(s_sz);

    ly->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* MiiMakerDialog::CreateMouthPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(12);

    auto* style_hdr = new QLabel(tr("Mouth Shape"), page);
    style_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
    ly->addWidget(style_hdr);

    auto* mouth_grid = new QGridLayout();
    mouth_grid->setSpacing(6);
    const QStringList mouths = {
        tr("Classic Smile Line"), tr("Happy Open Mouth"), tr("Neutral Line"),
        tr("Grin"), tr("Cat Mouth (:3)"), tr("Broad Smile")
    };
    std::vector<QPushButton*> mouth_btns;
    const int cur_m = mii.mouth_details.type.Value() % 6;
    for (int i = 0; i < mouths.size(); ++i) {
        auto* btn = MakeOptionCard(mouths[i], i == cur_m);
        mouth_btns.push_back(btn);
        mouth_grid->addWidget(btn, i / 3, i % 3);
    }
    for (int i = 0; i < static_cast<int>(mouth_btns.size()); ++i) {
        connect(mouth_btns[i], &QPushButton::clicked, this, [this, i, mouth_btns, mouths] {
            mii.mouth_details.type.Assign(i);
            for (int s = 0; s < static_cast<int>(mouth_btns.size()); ++s) {
                RestyleOptionCard(mouth_btns[s], mouths[s], s == i);
            }
            UpdatePreview();
        });
    }
    ly->addLayout(mouth_grid);

    auto style_slider = [this](QSlider* s) {
        s->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: %1; }"
            "QSlider::sub-page:horizontal { background: %2; }"
            "QSlider::handle:horizontal { background: %3; width: 14px; margin-top: -5px; margin-bottom: -5px; }")
            .arg(theme.card_bg, theme.accent, theme.text_primary));
    };
    auto make_slider_label = [this, page](const QString& text) {
        auto* l = new QLabel(text, page);
        l->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme.text_secondary));
        return l;
    };

    auto* s_y = new QSlider(Qt::Horizontal, page);
    s_y->setRange(9, 17);
    s_y->setValue(mii.mustache_details.mouth_y.Value());
    style_slider(s_y);
    connect(s_y, &QSlider::valueChanged, this, [this](int v) { mii.mustache_details.mouth_y.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Vertical Position:")));
    ly->addWidget(s_y);

    auto* s_sz = new QSlider(Qt::Horizontal, page);
    s_sz->setRange(1, 5);
    s_sz->setValue(mii.mouth_details.scale.Value());
    style_slider(s_sz);
    connect(s_sz, &QSlider::valueChanged, this, [this](int v) { mii.mouth_details.scale.Assign(v); UpdatePreview(); });
    ly->addWidget(make_slider_label(tr("Size:")));
    ly->addWidget(s_sz);

    ly->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* MiiMakerDialog::CreateFacialHairPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(12);

    auto* m_hdr = new QLabel(tr("Mustache"), page);
    m_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
    ly->addWidget(m_hdr);

    auto* m_row = new QHBoxLayout();
    m_row->setSpacing(6);
    std::vector<QPushButton*> m_btns;
    const int cur_mustache = mii.mustache_details.mustache_type.Value() % static_cast<int>(MUSTACHE_ASSETS.size());
    for (int i = 0; i < static_cast<int>(MUSTACHE_ASSETS.size()); ++i) {
        auto* btn = MakeImageOptionCard(QStringLiteral("mustache"), MUSTACHE_ASSETS[i].file,
                                        MUSTACHE_ASSETS[i].label, i == cur_mustache);
        m_btns.push_back(btn);
        m_row->addWidget(btn);
    }
    for (int i = 0; i < static_cast<int>(m_btns.size()); ++i) {
        connect(m_btns[i], &QPushButton::clicked, this, [this, i, m_btns] {
            mii.mustache_details.mustache_type.Assign(i);
            for (int s = 0; s < static_cast<int>(m_btns.size()); ++s) {
                const bool sel = (s == i);
                m_btns[s]->setText(QStringLiteral("  %1%2").arg(
                    MUSTACHE_ASSETS[s].label, sel ? QStringLiteral("  ✓") : QString()));
                m_btns[s]->setStyleSheet(sel
                    ? QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                     " font-weight: 700; border: none; text-align: left; padding: 4px 8px; }")
                          .arg(theme.card_sel_bg, theme.card_sel_text)
                    : QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                     " font-weight: 600; border: none; text-align: left; padding: 4px 8px; }"
                                     "QPushButton:hover { background-color: %3; }")
                          .arg(theme.card_bg, theme.card_text, theme.card_hover));
            }
            UpdatePreview();
        });
    }
    ly->addLayout(m_row);

    auto* b_hdr = new QLabel(tr("Beard"), page);
    b_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1; margin-top: 6px;").arg(theme.text_primary));
    ly->addWidget(b_hdr);

    auto* b_row = new QHBoxLayout();
    b_row->setSpacing(6);
    std::vector<QPushButton*> b_btns;
    const int cur_beard = mii.beard_details.type.Value() % static_cast<int>(BEARD_ASSETS.size() + 1);
    // Index 0 reserved for "None" since we only have 1 real decompiled beard asset.
    auto* none_btn = MakeOptionCard(tr("None"), cur_beard == 0);
    b_btns.push_back(none_btn);
    for (int i = 0; i < static_cast<int>(BEARD_ASSETS.size()); ++i) {
        auto* btn = MakeImageOptionCard(QStringLiteral("beard"), BEARD_ASSETS[i].file,
                                        BEARD_ASSETS[i].label, cur_beard == i + 1);
        b_btns.push_back(btn);
    }
    for (int i = 0; i < static_cast<int>(b_btns.size()); ++i) {
        connect(b_btns[i], &QPushButton::clicked, this, [this, i, b_btns] {
            mii.beard_details.type.Assign(i);
            for (int s = 0; s < static_cast<int>(b_btns.size()); ++s) {
                const bool sel = (s == i);
                if (s == 0) {
                    RestyleOptionCard(b_btns[s], tr("None"), sel);
                } else {
                    const auto& a = BEARD_ASSETS[s - 1];
                    b_btns[s]->setText(QStringLiteral("  %1%2").arg(
                        a.label, sel ? QStringLiteral("  ✓") : QString()));
                    b_btns[s]->setStyleSheet(sel
                        ? QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                         " font-weight: 700; border: none; text-align: left; padding: 4px 8px; }")
                              .arg(theme.card_sel_bg, theme.card_sel_text)
                        : QStringLiteral("QPushButton { background-color: %1; color: %2; font-size: 11px;"
                                         " font-weight: 600; border: none; text-align: left; padding: 4px 8px; }"
                                         "QPushButton:hover { background-color: %3; }")
                              .arg(theme.card_bg, theme.card_text, theme.card_hover));
                }
            }
            UpdatePreview();
        });
        b_row->addWidget(b_btns[i]);
    }
    ly->addLayout(b_row);

    auto* mole_hdr = new QLabel(tr("Beauty Mark / Mole"), page);
    mole_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1; margin-top: 6px;").arg(theme.text_primary));
    ly->addWidget(mole_hdr);

    auto* mole_btn = MakeOptionCard(tr("Enable Beauty Mark"), mii.mole_details.type.Value() > 0);
    connect(mole_btn, &QPushButton::clicked, this, [this, mole_btn] {
        const int n_val = (mii.mole_details.type.Value() > 0) ? 0 : 1;
        mii.mole_details.type.Assign(n_val);
        RestyleOptionCard(mole_btn, tr("Enable Beauty Mark"), n_val > 0);
        UpdatePreview();
    });
    ly->addWidget(mole_btn);
    ly->addStretch();

    scroll->setWidget(page);
    return scroll;
}

QWidget* MiiMakerDialog::CreateGlassesPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(12);

    auto* style_hdr = new QLabel(tr("Glasses Style"), page);
    style_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
    ly->addWidget(style_hdr);

    auto* g_grid = new QGridLayout();
    g_grid->setSpacing(6);
    const QStringList glasses = {
        tr("None"), tr("Rectangular Frames"), tr("Round Frames"), tr("Sunglasses")
    };
    std::vector<QPushButton*> g_btns;
    const int cur_g = mii.glasses_details.type.Value() % 4;
    for (int i = 0; i < glasses.size(); ++i) {
        auto* btn = MakeOptionCard(glasses[i], i == cur_g);
        g_btns.push_back(btn);
        g_grid->addWidget(btn, i / 4, i % 4);
    }
    for (int i = 0; i < static_cast<int>(g_btns.size()); ++i) {
        connect(g_btns[i], &QPushButton::clicked, this, [this, i, g_btns, glasses] {
            mii.glasses_details.type.Assign(i);
            for (int s = 0; s < static_cast<int>(g_btns.size()); ++s) {
                RestyleOptionCard(g_btns[s], glasses[s], s == i);
            }
            UpdatePreview();
        });
    }
    ly->addLayout(g_grid);

    auto* color_hdr = new QLabel(tr("Glasses Color"), page);
    color_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1; margin-top: 6px;").arg(theme.text_primary));
    ly->addWidget(color_hdr);

    auto* col_row = new QHBoxLayout();
    col_row->setSpacing(6);
    std::vector<QPushButton*> col_btns;
    const int cur_color = mii.glasses_details.color.Value() % 6;
    for (int i = 0; i < 6; ++i) {
        auto* c_btn = new QPushButton(page);
        c_btn->setFixedSize(36, 30);
        c_btn->setCursor(Qt::PointingHandCursor);
        col_btns.push_back(c_btn);
        RestyleColorSwatch(c_btn, QString::fromLatin1(GLASSES_COLORS[i].hex), i == cur_color, 0);
        col_row->addWidget(c_btn);
    }
    for (int i = 0; i < static_cast<int>(col_btns.size()); ++i) {
        connect(col_btns[i], &QPushButton::clicked, this, [this, i, col_btns] {
            mii.glasses_details.color.Assign(i);
            for (int s = 0; s < 6; ++s) {
                RestyleColorSwatch(col_btns[s], QString::fromLatin1(GLASSES_COLORS[s].hex), s == i, 0);
            }
            UpdatePreview();
        });
    }
    col_row->addStretch();
    ly->addLayout(col_row);
    ly->addStretch();

    scroll->setWidget(page);
    return scroll;
}

QWidget* MiiMakerDialog::CreateBodyPage() {
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(14);

    auto* body_hdr = new QLabel(tr("Body Build & Height"), page);
    body_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
    ly->addWidget(body_hdr);

    auto style_slider = [this](QSlider* s) {
        s->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: %1; }"
            "QSlider::sub-page:horizontal { background: %2; }"
            "QSlider::handle:horizontal { background: %3; width: 14px; margin-top: -5px; margin-bottom: -5px; }")
            .arg(theme.card_bg, theme.accent, theme.text_primary));
    };
    auto make_slider_label = [this, page](const QString& text) {
        auto* l = new QLabel(text, page);
        l->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme.text_secondary));
        return l;
    };

    auto* h_slider = new QSlider(Qt::Horizontal, page);
    h_slider->setRange(0, 128);
    h_slider->setValue(mii.height);
    style_slider(h_slider);
    connect(h_slider, &QSlider::valueChanged, this, [this](int v) { mii.height = static_cast<u8>(v); });
    ly->addWidget(make_slider_label(tr("Height (Short ◄―► Tall):")));
    ly->addWidget(h_slider);

    auto* w_slider = new QSlider(Qt::Horizontal, page);
    w_slider->setRange(0, 128);
    w_slider->setValue(mii.width);
    style_slider(w_slider);
    connect(w_slider, &QSlider::valueChanged, this, [this](int v) { mii.width = static_cast<u8>(v); });
    ly->addWidget(make_slider_label(tr("Build (Slim ◄―► Broad):")));
    ly->addWidget(w_slider);

    ly->addStretch();
    return page;
}

QWidget* MiiMakerDialog::CreateProfilePage() {
    auto* page = new QWidget();
    auto* ly = new QVBoxLayout(page);
    ly->setContentsMargins(0, 8, 0, 8);
    ly->setSpacing(14);

    auto* info_hdr = new QLabel(tr("Mii Profile Information"), page);
    info_hdr->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(theme.text_primary));
    ly->addWidget(info_hdr);

    auto* name_lbl = new QLabel(tr("Mii Name:"), page);
    name_lbl->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme.text_secondary));
    ly->addWidget(name_lbl);

    name_input = new QLineEdit(page);
    name_input->setText(MiiManager::GetMiiName(mii));
    name_input->setMaxLength(10);
    name_input->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; padding: 10px 14px; font-size: 15px; font-weight: 600; }"
        "QLineEdit:focus { border-color: %4; }")
        .arg(theme.input_bg, theme.input_text, theme.input_border, theme.accent));
    connect(name_input, &QLineEdit::textChanged, this, [this](const QString& text) {
        MiiManager::SetMiiName(mii, text);
        name_display->setText(text.isEmpty() ? tr("Hero") : text);
    });
    ly->addWidget(name_input);

    auto* gender_lbl = new QLabel(tr("Gender:"), page);
    gender_lbl->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(theme.text_secondary));
    ly->addWidget(gender_lbl);

    auto* g_row = new QHBoxLayout();
    g_row->setSpacing(8);
    const int cur_gender = mii.mii_details.gender.Value();
    auto* btn_male = MakeOptionCard(tr("♂ Male"), cur_gender == 0);
    auto* btn_female = MakeOptionCard(tr("♀ Female"), cur_gender == 1);

    auto update_gender_ui = [this, btn_male, btn_female](int g) {
        mii.mii_details.gender.Assign(g);
        gender_badge->setText(g == 0 ? tr("♂ Male") : tr("♀ Female"));
        RestyleOptionCard(btn_male, tr("♂ Male"), g == 0);
        RestyleOptionCard(btn_female, tr("♀ Female"), g == 1);
    };
    connect(btn_male, &QPushButton::clicked, this, [update_gender_ui] { update_gender_ui(0); });
    connect(btn_female, &QPushButton::clicked, this, [update_gender_ui] { update_gender_ui(1); });

    g_row->addWidget(btn_male);
    g_row->addWidget(btn_female);
    ly->addLayout(g_row);

    auto* color_lbl = new QLabel(tr("Shirt Color:"), page);
    color_lbl->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; margin-top: 6px;").arg(theme.text_secondary));
    ly->addWidget(color_lbl);

    auto* shirt_grid = new QGridLayout();
    shirt_grid->setSpacing(6);
    const int cur_shirt = mii.mii_details.favorite_color.Value();
    for (int i = 0; i < 12; ++i) {
        auto* btn = new QPushButton(page);
        btn->setFixedSize(60, 30);
        btn->setCursor(Qt::PointingHandCursor);
        shirt_swatches.push_back(btn);
        RestyleColorSwatch(btn, QString::fromLatin1(SHIRT_COLORS[i].hex), i == cur_shirt, 0);
        connect(btn, &QPushButton::clicked, this, [this, i] {
            mii.mii_details.favorite_color.Assign(i);
            for (int s = 0; s < 12; ++s) {
                RestyleColorSwatch(shirt_swatches[s], QString::fromLatin1(SHIRT_COLORS[s].hex), s == i, 0);
            }
            UpdatePreview();
        });
        shirt_grid->addWidget(btn, i / 6, i % 6);
    }
    ly->addLayout(shirt_grid);

    ly->addStretch();
    return page;
}

void MiiMakerDialog::UpdatePreview() {
    QPixmap pix = RenderMii(mii, 280, 220, QStringLiteral("normal"), current_tilt);
    preview_label->setPixmap(pix);

    QString name = MiiManager::GetMiiName(mii);
    if (name.isEmpty()) name = tr("Hero");
    if (name_display) {
        name_display->setText(name);
    }
    if (gender_badge) {
        gender_badge->setText(mii.mii_details.gender.Value() == 0 ? tr("♂ Male") : tr("♀ Female"));
    }
}

void MiiMakerDialog::OnRandomizeClicked() {
    QString current_name = MiiManager::GetMiiName(mii);
    mii = MiiManager::CreateRandomMii(current_name.isEmpty() ? QStringLiteral("Hero") : current_name);
    if (name_input) {
        name_input->setText(MiiManager::GetMiiName(mii));
    }
    const int new_c = mii.mii_details.favorite_color.Value() % 12;
    for (int s = 0; s < static_cast<int>(shirt_swatches.size()); ++s) {
        RestyleColorSwatch(shirt_swatches[s], QString::fromLatin1(SHIRT_COLORS[s].hex), s == new_c, 0);
    }
    UpdatePreview();
}

void MiiMakerDialog::OnSaveClicked() {
    MiiManager::SaveMiiToNAND(mii);

    const QString avatar_dir = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::UserDir)) + QStringLiteral("/avatars");
    QDir().mkpath(avatar_dir);
    const QString avatar_file = avatar_dir + QStringLiteral("/profile_mii.png");
    MiiManager::ExportMiiAvatar(mii, avatar_file);

    UISettings::values.profile_avatar_path = avatar_file;
    const QString saved_name = MiiManager::GetMiiName(mii);
    if (!saved_name.isEmpty()) {
        UISettings::values.profile_nickname = saved_name;
    }

    emit MiiSaved(mii, avatar_file);
    Accept();
}

void MiiMakerDialog::OnCancelClicked() {
    Reject();
}

} // namespace MiiMaker
