// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <vector>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QStandardItemModel>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include "citra_qt/game_list_p.h"
#include "citra_qt/home_screen.h"
#include "citra_qt/uisettings.h"
#include "common/file_util.h"

namespace {

constexpr int TileIconSize = 96;
constexpr int TileSpacing = 12;
constexpr int PillBarHeight = 48;

// Cover art and backgrounds are frequently much lower-res than the area they're stretched to
// fill; plain bilinear scaling (Qt::SmoothTransformation) leaves that soft/pixelated. This
// applies a standard unsharp-style 3x3 convolution after scaling to recover perceived detail.
// Runs once per image load/scale, never per-frame, so the per-pixel cost is a non-issue.
QImage SharpenImage(const QImage& input) {
    const QImage src = input.convertToFormat(QImage::Format_ARGB32);
    QImage out = src;
    const int w = src.width();
    const int h = src.height();
    if (w < 3 || h < 3) {
        return src;
    }

    // Classic edge-enhance kernel: center 5, four-neighbor -1, sums to 1 (preserves brightness).
    for (int y = 1; y < h - 1; ++y) {
        const QRgb* row_prev = reinterpret_cast<const QRgb*>(src.constScanLine(y - 1));
        const QRgb* row_curr = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        const QRgb* row_next = reinterpret_cast<const QRgb*>(src.constScanLine(y + 1));
        QRgb* out_row = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 1; x < w - 1; ++x) {
            const QRgb center = row_curr[x];
            for (int channel = 0; channel < 3; ++channel) {
                const int c = (channel == 0)   ? qRed(center)
                             : (channel == 1) ? qGreen(center)
                                              : qBlue(center);
                const int up = (channel == 0)    ? qRed(row_prev[x])
                              : (channel == 1)   ? qGreen(row_prev[x])
                                                 : qBlue(row_prev[x]);
                const int down = (channel == 0)  ? qRed(row_next[x])
                                : (channel == 1) ? qGreen(row_next[x])
                                                 : qBlue(row_next[x]);
                const int left = (channel == 0)  ? qRed(row_curr[x - 1])
                                : (channel == 1) ? qGreen(row_curr[x - 1])
                                                 : qBlue(row_curr[x - 1]);
                const int right = (channel == 0)  ? qRed(row_curr[x + 1])
                                 : (channel == 1) ? qGreen(row_curr[x + 1])
                                                  : qBlue(row_curr[x + 1]);
                const int sharpened = qBound(0, c * 5 - up - down - left - right, 255);
                if (channel == 0) {
                    out_row[x] = qRgba(sharpened, qGreen(center), qBlue(center), qAlpha(center));
                } else if (channel == 1) {
                    out_row[x] =
                        qRgba(qRed(out_row[x]), sharpened, qBlue(out_row[x]), qAlpha(center));
                } else {
                    out_row[x] =
                        qRgba(qRed(out_row[x]), qGreen(out_row[x]), sharpened, qAlpha(center));
                }
            }
        }
    }
    return out;
}

QPixmap ScaleAndSharpen(const QPixmap& source, const QSize& target_size,
                        Qt::AspectRatioMode aspect_mode) {
    const QPixmap scaled =
        source.scaled(target_size, aspect_mode, Qt::SmoothTransformation);
    // Sharpening a tiny (e.g. pre-scale icon-sized) source barely helps and burns cycles for
    // nothing: only worth it once the image is large enough for softness to be visible.
    if (scaled.width() < 24 || scaled.height() < 24) {
        return scaled;
    }
    return QPixmap::fromImage(SharpenImage(scaled.toImage()));
}

QPixmap RoundedPixmap(const QPixmap& source, int size, int radius) {
    QPixmap scaled =
        ScaleAndSharpen(source, QSize(size, size), Qt::KeepAspectRatioByExpanding);
    QPixmap rounded(size, size);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, size, size, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
    return rounded;
}

QToolButton* MakePillButton(const QString& glyph, const QString& tooltip, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setText(glyph);
    button->setToolTip(tooltip);
    button->setFixedSize(36, 36);
    button->setFocusPolicy(Qt::NoFocus);
    button->setProperty("pillButton", true);
    return button;
}

// Setting any stylesheet on a widget creates a new local cascade: descendants stop inheriting
// the app-wide dark theme's palette/colors and fall back to Qt's generic default (black text),
// which on a dark background is invisible. Pin text color explicitly so this stays legible.
const QString DefaultStyleQss = QStringLiteral(
    "HomeScreenWidget, HomeScreenWidget QLabel, HomeScreenWidget QToolButton,"
    " HomeScreenWidget QListWidget { color: palette(text); background: transparent; }\n"
    "#pillBar { background-color: palette(button); border-radius: 24px; }\n"
    "#tileList { background: transparent; border: none; }\n"
    "#tileList::item { border-radius: 12px; padding: 4px; }\n"
    "#tileList::item:selected { background-color: palette(highlight); }\n"
    "#tileList::item:hover { background-color: palette(midlight); }\n"
    "#largeAddButton { border: 3px dashed palette(mid); border-radius: 20px;"
    " background: transparent; font-size: 32px; color: palette(text); }\n"
    "#largeAddButton:hover { background-color: palette(midlight); }\n"
    "QToolButton[pillButton=\"true\"] { border-radius: 18px; border: none;"
    " background: transparent; font-size: 16px; }\n"
    "QToolButton[pillButton=\"true\"]:hover { background-color: palette(highlight); }\n");

QString ExternalStylePath() {
    return QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir)) +
           QStringLiteral("home_screen_style.qss");
}

QString CustomCoversPath() {
    return QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir)) +
           QStringLiteral("custom_covers.txt");
}

} // namespace

// Stored as plain tab-separated "game_path\tcover_path" lines rather than QSettings/INI,
// since game paths contain '/' which QSettings would otherwise parse as a group separator.
void HomeScreenWidget::LoadCustomCovers() {
    QFile file(CustomCoversPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const int tab = line.indexOf(QLatin1Char('\t'));
        if (tab <= 0) {
            continue;
        }
        custom_covers.insert(line.left(tab), line.mid(tab + 1));
    }
}

void HomeScreenWidget::SaveCustomCovers() {
    const QString path = CustomCoversPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    for (auto it = custom_covers.constBegin(); it != custom_covers.constEnd(); ++it) {
        stream << it.key() << '\t' << it.value() << '\n';
    }
}

HomeScreenWidget::HomeScreenWidget(QStandardItemModel* source_model_, QWidget* parent)
    : QWidget(parent), source_model(source_model_) {
    SetupUi();
    ApplyAppearanceSettings();
    LoadCustomCovers();
    UpdateClock();
    clock_timer = new QTimer(this);
    connect(clock_timer, &QTimer::timeout, this, &HomeScreenWidget::UpdateClock);
    clock_timer->start(30'000);
    Refresh();

    const QString style_path = ExternalStylePath();
    if (!QFile::exists(style_path)) {
        QDir().mkpath(QFileInfo(style_path).absolutePath());
        QFile file(style_path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream(&file) << DefaultStyleQss;
        }
    }
    ReloadExternalStyle();
    style_watcher = new QFileSystemWatcher(this);
    style_watcher->addPath(style_path);
    connect(style_watcher, &QFileSystemWatcher::fileChanged, this, [this, style_path] {
        // Editors often replace-on-save, which drops the inode from the watch list.
        if (!style_watcher->files().contains(style_path) && QFile::exists(style_path)) {
            style_watcher->addPath(style_path);
        }
        ReloadExternalStyle();
    });
}

void HomeScreenWidget::ReloadExternalStyle() {
    QFile file(ExternalStylePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(DefaultStyleQss);
        return;
    }
    setStyleSheet(QTextStream(&file).readAll());
}

HomeScreenWidget::~HomeScreenWidget() = default;

void HomeScreenWidget::SetupUi() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(16, 12, 16, 12);
    main_layout->setSpacing(10);

    auto* top_bar = new QHBoxLayout();
    title_label = new QLabel(tr("Games"), this);
    QFont title_font = title_label->font();
    title_font.setPointSize(title_font.pointSize() + 4);
    title_font.setBold(true);
    title_label->setFont(title_font);

    clock_label = new QLabel(this);
    clock_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    top_bar->addWidget(title_label);
    top_bar->addStretch();
    top_bar->addWidget(clock_label);
    main_layout->addLayout(top_bar);

    // Empty state: one big "+" prompt, pinned left, vertically centered. Shown when no
    // games are found, mirroring Switch's first-run "insert cartridge" prompt.
    empty_state_container = new QWidget(this);
    auto* empty_v = new QVBoxLayout(empty_state_container);
    empty_v->setContentsMargins(0, 0, 0, 0);
    empty_v->addStretch(1);
    auto* empty_row = new QHBoxLayout();
    large_add_button = new QToolButton(empty_state_container);
    large_add_button->setFixedSize(240, 240);
    large_add_button->setToolTip(tr("Add Game Directory"));
    large_add_button->setFocusPolicy(Qt::NoFocus);
    large_add_button->setText(QStringLiteral("+\n") + tr("Add Game Directory"));
    large_add_button->setObjectName(QStringLiteral("largeAddButton"));
    connect(large_add_button, &QToolButton::clicked, this, &HomeScreenWidget::AddDirectory);
    empty_row->addWidget(large_add_button);
    empty_row->addStretch(1);
    empty_v->addLayout(empty_row);
    empty_v->addStretch(1);
    main_layout->addWidget(empty_state_container, 1);

    // Populated state: bottom-center tile shelf + pill dock, Switch-style.
    populated_container = new QWidget(this);
    auto* populated_v = new QVBoxLayout(populated_container);
    populated_v->setContentsMargins(0, 0, 0, 0);
    populated_v->setSpacing(10);
    populated_v->addStretch(1);

    tile_list = new QListWidget(populated_container);
    tile_list->setViewMode(QListWidget::IconMode);
    tile_list->setFlow(QListWidget::LeftToRight);
    tile_list->setWrapping(false);
    tile_list->setResizeMode(QListWidget::Adjust);
    tile_list->setMovement(QListWidget::Static);
    tile_list->setIconSize(QSize(TileIconSize, TileIconSize));
    tile_list->setGridSize(QSize(TileIconSize + TileSpacing * 2, TileIconSize + 40));
    tile_list->setSpacing(TileSpacing);
    tile_list->setUniformItemSizes(true);
    tile_list->setSelectionMode(QAbstractItemView::SingleSelection);
    tile_list->setFrameShape(QFrame::NoFrame);
    tile_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tile_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tile_list->setFixedHeight(TileIconSize + 56);
    tile_list->setObjectName(QStringLiteral("tileList"));
    tile_list->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tile_list, &QListWidget::itemActivated, this, &HomeScreenWidget::OnTileActivated);
    // The "+" tile is an action button, not a selectable game: a single click should fire it
    // immediately rather than waiting for the platform's double-click "activate" gesture.
    connect(tile_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item && item->data(AddDirectoryRole).toBool()) {
            emit AddDirectory();
        }
    });
    connect(tile_list, &QListWidget::customContextMenuRequested, this,
            &HomeScreenWidget::OnTileContextMenu);

    populated_v->addWidget(tile_list, 0, Qt::AlignHCenter);

    main_layout->addWidget(populated_container, 1);
    populated_container->hide();

    main_layout->addSpacing(14);

    // Floating pill-shaped dock bar, bottom-center. Always visible, independent of whether
    // any games are loaded, since Settings/Power are useful either way.
    pill_bar = new QWidget(this);
    pill_bar->setFixedHeight(PillBarHeight);
    pill_bar->setObjectName(QStringLiteral("pillBar"));
    auto* pill_layout = new QHBoxLayout(pill_bar);
    pill_layout->setContentsMargins(10, 0, 10, 0);
    pill_layout->setSpacing(6);

    auto* profile_button = MakePillButton(QStringLiteral("\U0001F464"), tr("Profile"), pill_bar);
    auto* settings_button = MakePillButton(QStringLiteral("⚙"), tr("Settings"), pill_bar);
    auto* screenshots_button =
        MakePillButton(QStringLiteral("\U0001F4F7"), tr("Screenshots"), pill_bar);
    auto* add_dir_button =
        MakePillButton(QStringLiteral("\U0001F4C1"), tr("Add Game Directory"), pill_bar);
    auto* wallpaper_button =
        MakePillButton(QStringLiteral("\U0001F5BC"), tr("Set Background Image"), pill_bar);
    auto* power_button = MakePillButton(QStringLiteral("⏻"), tr("Quit"), pill_bar);

    connect(profile_button, &QToolButton::clicked, this, &HomeScreenWidget::OpenProfile);
    connect(settings_button, &QToolButton::clicked, this, &HomeScreenWidget::OpenSettings);
    connect(screenshots_button, &QToolButton::clicked, this, &HomeScreenWidget::OpenScreenshots);
    connect(add_dir_button, &QToolButton::clicked, this, &HomeScreenWidget::AddDirectory);
    connect(wallpaper_button, &QToolButton::clicked, this, &HomeScreenWidget::OnChooseWallpaper);
    connect(power_button, &QToolButton::clicked, this, &HomeScreenWidget::PowerOff);

    for (auto* button : {profile_button, settings_button, screenshots_button, add_dir_button,
                         wallpaper_button, power_button}) {
        pill_layout->addWidget(button);
    }

    main_layout->addWidget(pill_bar, 0, Qt::AlignHCenter);
}

void HomeScreenWidget::ApplyAppearanceSettings() {
    if (!UISettings::values.home_background_path.isEmpty()) {
        const QPixmap source(UISettings::values.home_background_path);
        if (!source.isNull()) {
            // Pre-scale+sharpen once to a generous cap rather than on every paintEvent: the
            // per-frame cost of a plain downscale from this cached copy is negligible, while
            // redoing the sharpen convolution every repaint would not be.
            const QSize cap(1600, 1000);
            const QSize target = source.size().boundedTo(cap) == source.size()
                                     ? source.size()
                                     : source.size().scaled(cap, Qt::KeepAspectRatioByExpanding);
            background_pixmap = ScaleAndSharpen(source, target, Qt::KeepAspectRatio);
        } else {
            background_pixmap = QPixmap();
        }
    } else {
        background_pixmap = QPixmap();
    }

    QFont font = title_label->font();
    if (!UISettings::values.home_font_family.isEmpty()) {
        font.setFamily(UISettings::values.home_font_family);
    }
    if (UISettings::values.home_font_size > 0) {
        font.setPointSize(UISettings::values.home_font_size);
    }
    setFont(font);

    QFont title_font = font;
    title_font.setPointSize(font.pointSize() + 4);
    title_font.setBold(true);
    title_label->setFont(title_font);
    clock_label->setFont(font);
    tile_list->setFont(font);

    update();
}

void HomeScreenWidget::paintEvent(QPaintEvent* event) {
    if (!background_pixmap.isNull()) {
        QPainter painter(this);
        const QPixmap scaled = background_pixmap.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                                         Qt::SmoothTransformation);
        const int x = (width() - scaled.width()) / 2;
        const int y = (height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
    }
    QWidget::paintEvent(event);
}

void HomeScreenWidget::UpdateClock() {
    clock_label->setText(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm")));
}

void HomeScreenWidget::UpdateTileListWidth() {
    const int count = tile_list->count();
    const int natural_width = count * (TileIconSize + TileSpacing * 2);
    const int max_width = qMax(width() - 32, TileIconSize + TileSpacing * 2);
    tile_list->setFixedWidth(qMin(natural_width, max_width));
}

namespace {
// GameListWorker nests each discovered game as a CHILD of its directory row (INSTALLED, SYSTEM,
// or a custom folder), not as a top-level model row — so collecting tiles needs to recurse.
void CollectGameItems(QStandardItem* parent, std::vector<QStandardItem*>& out) {
    for (int row = 0; row < parent->rowCount(); ++row) {
        QStandardItem* item = parent->child(row, 0);
        if (!item) {
            continue;
        }
        if (item->data(GameListItem::TypeRole).toInt() ==
            static_cast<int>(GameListItemType::Game)) {
            out.push_back(item);
        }
        if (item->hasChildren()) {
            CollectGameItems(item, out);
        }
    }
}
} // namespace

void HomeScreenWidget::Refresh() {
    if (!source_model) {
        return;
    }
    tile_list->clear();

    std::vector<QStandardItem*> game_items;
    CollectGameItems(source_model->invisibleRootItem(), game_items);

    int games_added = 0;
    for (QStandardItem* item : game_items) {
        const QString full_path = item->data(GameListItemPath::FullPathRole).toString();
        QString title = item->data(GameListItemPath::TitleRole).toString();
        if (title.isEmpty()) {
            title = item->data(Qt::DisplayRole).toString();
        }

        QPixmap icon;
        const auto cover_it = custom_covers.constFind(full_path);
        if (cover_it != custom_covers.constEnd()) {
            icon = QPixmap(cover_it.value());
        }
        if (icon.isNull()) {
            icon = item->data(Qt::DecorationRole).value<QPixmap>();
        }
        if (icon.isNull()) {
            icon = QPixmap(TileIconSize, TileIconSize);
            icon.fill(Qt::darkGray);
        }

        auto* tile = new QListWidgetItem(QIcon(RoundedPixmap(icon, TileIconSize, 16)), title);
        tile->setData(FullPathRole, full_path);
        tile->setTextAlignment(Qt::AlignHCenter);
        tile->setToolTip(title);
        tile_list->addItem(tile);
        ++games_added;
    }

    // Trailing "+" tile to add a new game directory, Switch-cartridge-slot style.
    QPixmap plus_pixmap(TileIconSize, TileIconSize);
    plus_pixmap.fill(Qt::transparent);
    {
        QPainter painter(&plus_pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QPen pen(palette().color(QPalette::Mid));
        pen.setWidth(3);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawRoundedRect(2, 2, TileIconSize - 4, TileIconSize - 4, 16, 16);
        QFont plus_font = font();
        plus_font.setPointSize(28);
        painter.setFont(plus_font);
        painter.drawText(QRect(0, 0, TileIconSize, TileIconSize), Qt::AlignCenter,
                         QStringLiteral("+"));
    }
    auto* add_tile = new QListWidgetItem(QIcon(plus_pixmap), tr("Add Game Directory"));
    add_tile->setData(AddDirectoryRole, true);
    add_tile->setTextAlignment(Qt::AlignHCenter);
    tile_list->addItem(add_tile);

    UpdateTileListWidth();

    empty_state_container->setVisible(games_added == 0);
    populated_container->setVisible(games_added > 0);
}

void HomeScreenWidget::OnChooseWallpaper() {
    const QString image_path = QFileDialog::getOpenFileName(
        this, tr("Select Background Image"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff)"));
    if (image_path.isEmpty()) {
        return;
    }
    UISettings::values.home_background_path = image_path;
    ApplyAppearanceSettings();
    emit BackgroundImageChanged();
}

void HomeScreenWidget::OnTileContextMenu(const QPoint& pos) {
    QListWidgetItem* item = tile_list->itemAt(pos);
    if (!item || item->data(AddDirectoryRole).toBool()) {
        return;
    }
    const QString full_path = item->data(FullPathRole).toString();
    if (full_path.isEmpty()) {
        return;
    }

    QMenu menu(this);
    QAction* set_cover_action = menu.addAction(tr("Set Custom Cover..."));
    QAction* clear_cover_action = nullptr;
    if (custom_covers.contains(full_path)) {
        clear_cover_action = menu.addAction(tr("Remove Custom Cover"));
    }

    QAction* chosen = menu.exec(tile_list->mapToGlobal(pos));
    if (chosen == set_cover_action) {
        const QString image_path = QFileDialog::getOpenFileName(
            this, tr("Select Cover Image"), QString(),
            tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff)"));
        if (!image_path.isEmpty()) {
            custom_covers.insert(full_path, image_path);
            SaveCustomCovers();
            Refresh();
        }
    } else if (chosen == clear_cover_action) {
        custom_covers.remove(full_path);
        SaveCustomCovers();
        Refresh();
    }
}

void HomeScreenWidget::OnTileActivated(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    if (item->data(AddDirectoryRole).toBool()) {
        emit AddDirectory();
        return;
    }
    const QString full_path = item->data(FullPathRole).toString();
    if (!full_path.isEmpty()) {
        emit GameChosen(full_path);
    }
}
