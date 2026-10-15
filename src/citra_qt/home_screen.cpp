// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <functional>
#include <utility>
#include <vector>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWheelEvent>
#include "citra_qt/game_list_p.h"
#include "citra_qt/home_screen.h"
#include "citra_qt/uisettings.h"
#include "common/file_util.h"

#ifdef Q_OS_MAC
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#elif defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr int TileSpacing = 12;
constexpr int PillBarHeight = 50;

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
                const int up = (channel == 0)   ? qRed(row_prev[x])
                               : (channel == 1) ? qGreen(row_prev[x])
                                                : qBlue(row_prev[x]);
                const int down = (channel == 0)   ? qRed(row_next[x])
                                 : (channel == 1) ? qGreen(row_next[x])
                                                  : qBlue(row_next[x]);
                const int left = (channel == 0)   ? qRed(row_curr[x - 1])
                                 : (channel == 1) ? qGreen(row_curr[x - 1])
                                                  : qBlue(row_curr[x - 1]);
                const int right = (channel == 0)   ? qRed(row_curr[x + 1])
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

// target_size is in logical (CSS) pixels; dpr is the screen's device pixel ratio (2.0 on
// Retina). Without this, a pixmap rendered at exactly the logical tile size gets stretched by
// Qt's compositor to fill the actual physical pixels on a HiDPI screen, which is what reads as
// "pixelated" even when the source image is high quality.
QPixmap ScaleAndSharpen(const QPixmap& source, const QSize& target_size,
                        Qt::AspectRatioMode aspect_mode, qreal dpr = 1.0) {
    const QSize physical_size(qRound(target_size.width() * dpr),
                              qRound(target_size.height() * dpr));
    QPixmap scaled = source.scaled(physical_size, aspect_mode, Qt::SmoothTransformation);
    // Sharpening a tiny (e.g. pre-scale icon-sized) source barely helps and burns cycles for
    // nothing: only worth it once the image is large enough for softness to be visible.
    if (scaled.width() >= 24 && scaled.height() >= 24) {
        scaled = QPixmap::fromImage(SharpenImage(scaled.toImage()));
    }
    scaled.setDevicePixelRatio(dpr);
    return scaled;
}

// Perceptual (ITU-R BT.601) luminance of a pixmap, 0 (black) to 1 (white). Downscales to a tiny
// size first since only the overall brightness matters here, not per-pixel detail.
qreal AverageLuminance(const QPixmap& pixmap) {
    if (pixmap.isNull()) {
        return 0.0;
    }
    const QImage image = pixmap.scaled(16, 16, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                             .toImage()
                             .convertToFormat(QImage::Format_RGB32);
    double sum = 0.0;
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb px = row[x];
            sum += 0.299 * qRed(px) + 0.587 * qGreen(px) + 0.114 * qBlue(px);
            ++count;
        }
    }
    return count > 0 ? (sum / count) / 255.0 : 0.0;
}

// Separable box blur, run three times to approximate a gaussian. Used only on wallpapers that
// have to be upscaled: a small, JPEG-compressed source blown up several times over magnifies
// its own compression artifacts into visible grain, and no resampler can invent the missing
// detail. Softening in proportion to the upscale turns that grain into a deliberate-looking
// out-of-focus backdrop. Runs once per resize, never per frame.
QImage BoxBlur(const QImage& source, int radius) {
    if (radius < 1) {
        return source;
    }
    QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int w = image.width();
    const int h = image.height();
    if (w < 2 || h < 2) {
        return image;
    }
    const int span = radius * 2 + 1;

    for (int pass = 0; pass < 3; ++pass) {
        // Horizontal, then vertical — a box blur is separable, so two 1D passes per round cost
        // O(radius) instead of the O(radius^2) a 2D kernel would.
        for (int axis = 0; axis < 2; ++axis) {
            const int outer = axis == 0 ? h : w;
            const int inner = axis == 0 ? w : h;
            QImage scratch = image;
            for (int o = 0; o < outer; ++o) {
                for (int i = 0; i < inner; ++i) {
                    int r = 0, g = 0, b = 0, a = 0;
                    for (int k = -radius; k <= radius; ++k) {
                        const int idx = qBound(0, i + k, inner - 1);
                        const QRgb px = axis == 0 ? scratch.pixel(idx, o) : scratch.pixel(o, idx);
                        r += qRed(px);
                        g += qGreen(px);
                        b += qBlue(px);
                        a += qAlpha(px);
                    }
                    const QRgb out = qRgba(r / span, g / span, b / span, a / span);
                    if (axis == 0) {
                        image.setPixel(i, o, out);
                    } else {
                        image.setPixel(o, i, out);
                    }
                }
            }
        }
    }
    return image;
}

// Average luminance of just one sub-rectangle of a pixmap, given in normalized (0..1)
// coordinates. A single whole-image average is what makes light wallpapers fail: a photo that
// is bright where the text sits but dark elsewhere averages out to "dark", so white text gets
// chosen and then disappears into the bright region. Sampling only the area a given UI element
// actually covers is what makes the choice correct for that element.
qreal RegionLuminance(const QPixmap& pixmap, const QRectF& region) {
    if (pixmap.isNull()) {
        return 0.0;
    }
    const QRect pixel_region =
        QRectF(region.x() * pixmap.width(), region.y() * pixmap.height(),
               region.width() * pixmap.width(), region.height() * pixmap.height())
            .toRect()
            .intersected(pixmap.rect());
    if (pixel_region.isEmpty()) {
        return 0.0;
    }
    return AverageLuminance(pixmap.copy(pixel_region));
}

QPixmap RoundedPixmap(const QPixmap& source, int size, int radius, qreal dpr = 1.0) {
    QPixmap scaled =
        ScaleAndSharpen(source, QSize(size, size), Qt::KeepAspectRatioByExpanding, dpr);
    QPixmap rounded(qRound(size * dpr), qRound(size * dpr));
    rounded.setDevicePixelRatio(dpr);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, size, size, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(
        QPointF((size - scaled.width() / dpr) / 2.0, (size - scaled.height() / dpr) / 2.0), scaled);
    return rounded;
}

QPixmap RoundedLivePixmap(const QPixmap& source, int size, int radius, qreal dpr = 1.0) {
    const QSize physical_size(qRound(size * dpr), qRound(size * dpr));
    QPixmap scaled =
        source.scaled(physical_size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);

    QPixmap rounded(qRound(size * dpr), qRound(size * dpr));
    rounded.setDevicePixelRatio(dpr);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, size, size, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(
        QPointF((size - scaled.width() / dpr) / 2.0, (size - scaled.height() / dpr) / 2.0), scaled);
    return rounded;
}

// Gives a QToolButton a springy hover/press response by animating its iconSize. Animating the
// *icon* rather than the button's geometry is deliberate: the buttons live inside a layout, so
// any geometry animation would be fought and overwritten on the next relayout, whereas iconSize
// is purely visual and leaves the button's own footprint untouched.
class ButtonPulse : public QObject {
public:
    ButtonPulse(QToolButton* button, int base, int hovered, int pressed)
        : QObject(button), base_size(base), hover_size(hovered), press_size(pressed) {
        animation = new QPropertyAnimation(button, "iconSize", this);
        animation->setDuration(130);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        button->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        auto* button = qobject_cast<QToolButton*>(watched);
        if (!button) {
            return QObject::eventFilter(watched, event);
        }
        switch (event->type()) {
        case QEvent::Enter:
            AnimateTo(button, hover_size);
            break;
        case QEvent::Leave:
            AnimateTo(button, base_size);
            break;
        case QEvent::MouseButtonPress:
            AnimateTo(button, press_size);
            break;
        case QEvent::MouseButtonRelease:
            // Settle on the hover size if the cursor is still over the button, so releasing a
            // click doesn't snap straight back down to the idle size while still hovered.
            AnimateTo(button, button->underMouse() ? hover_size : base_size);
            break;
        default:
            break;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void AnimateTo(QToolButton* button, int size) {
        animation->stop();
        animation->setStartValue(button->iconSize());
        animation->setEndValue(QSize(size, size));
        animation->start();
    }

    QPropertyAnimation* animation;
    int base_size;
    int hover_size;
    int press_size;
};

// Host battery charge as a percentage, plus whether it's on mains power. Desktops have no
// battery at all, so they report a permanent full charge rather than an empty readout.
struct BatteryState {
    int percent = 100;
    bool charging = true;
};

BatteryState ReadBatteryState() {
    BatteryState state;
#ifdef Q_OS_MAC
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (!info) {
        return state;
    }
    CFArrayRef sources = IOPSCopyPowerSourcesList(info);
    if (sources) {
        for (CFIndex i = 0; i < CFArrayGetCount(sources); ++i) {
            CFDictionaryRef source =
                IOPSGetPowerSourceDescription(info, CFArrayGetValueAtIndex(sources, i));
            if (!source) {
                continue;
            }
            int current = 0;
            int max = 0;
            if (auto num = static_cast<CFNumberRef>(
                    CFDictionaryGetValue(source, CFSTR(kIOPSCurrentCapacityKey)))) {
                CFNumberGetValue(num, kCFNumberIntType, &current);
            }
            if (auto num = static_cast<CFNumberRef>(
                    CFDictionaryGetValue(source, CFSTR(kIOPSMaxCapacityKey)))) {
                CFNumberGetValue(num, kCFNumberIntType, &max);
            }
            if (max > 0) {
                state.percent = qBound(0, qRound(100.0 * current / max), 100);
                const auto charging = static_cast<CFBooleanRef>(
                    CFDictionaryGetValue(source, CFSTR(kIOPSIsChargingKey)));
                state.charging = charging && CFBooleanGetValue(charging);
            }
            break; // first internal battery is the one we care about
        }
        CFRelease(sources);
    }
    CFRelease(info);
#elif defined(Q_OS_WIN)
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (sps.BatteryLifePercent != 255) {
            state.percent = qBound(0, static_cast<int>(sps.BatteryLifePercent), 100);
        }
        state.charging = (sps.BatteryFlag & 8) != 0 || sps.ACLineStatus == 1;
    }
#endif
    return state;
}

// Battery pill: rounded body, nub on the right, fill proportional to charge.
QPixmap RenderBatteryIcon(const BatteryState& state, const QColor& color, qreal dpr) {
    constexpr int Width = 26;
    constexpr int Height = 13;
    QPixmap pixmap(qRound(Width * dpr), qRound(Height * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF body(0.5, 0.5, Width - 6.0, Height - 1.0);
    QPen outline(color);
    outline.setWidthF(1.4);
    painter.setPen(outline);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, 3, 3);

    // Terminal nub on the right-hand end.
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(body.right() + 1.5, Height / 2.0 - 2.5, 2.5, 5.0), 1, 1);

    // Fill. Keep a sliver visible at very low charge so the bar never looks simply absent.
    const qreal inset = 2.5;
    const qreal usable = body.width() - inset * 2;
    const qreal filled = qMax(usable * state.percent / 100.0, state.percent > 0 ? 1.5 : 0.0);
    if (filled > 0.0) {
        painter.setBrush(state.percent <= 15 && !state.charging ? QColor(0xe0, 0x3c, 0x3c) : color);
        painter.drawRoundedRect(
            QRectF(body.left() + inset, body.top() + inset, filled, body.height() - inset * 2), 1.5,
            1.5);
    }
    return pixmap;
}

// Taste rule 4: the docked legend's letter glyphs. Filled circle, letter knocked out in the
// surface colour — monochrome and inheriting the foreground, per rule 5.
QPixmap RenderGlyphBadge(const QString& letter, const QColor& fg, const QColor& bg, qreal dpr) {
    constexpr int Size = 19;
    QPixmap pixmap(qRound(Size * dpr), qRound(Size * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fg);
    painter.drawEllipse(QRectF(0, 0, Size, Size));

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSizeF(Size * 0.52);
    painter.setFont(font);
    painter.setPen(bg);
    painter.drawText(QRectF(0, 0, Size, Size), Qt::AlignCenter, letter);
    return pixmap;
}

QToolButton* MakePillButton(const QString& glyph, const QString& tooltip, QWidget* parent) {
    Q_UNUSED(tooltip);
    auto* button = new QToolButton(parent);
    button->setText(glyph);
    // Native QToolTip popups render corrupted over this window's Vulkan/MoltenVK surface, so
    // intentionally not setting one here.
    button->setFixedSize(51, 51);
    button->setFocusPolicy(Qt::NoFocus);
    button->setProperty("pillButton", true);
    return button;
}

// Bounding box of everything that isn't fully transparent. Source icons each ship a different
// amount of built-in padding (measured: content fills anywhere from 41% to 86% of the canvas),
// so scaling whole canvases to one size makes the actual glyphs render at visibly different
// sizes. Cropping to this box first is what lets them be normalized to a common visual size.
QRect OpaqueBounds(const QImage& image) {
    int min_x = image.width();
    int min_y = image.height();
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(row[x]) > 8) { // ignore near-invisible antialiasing fringe
                min_x = qMin(min_x, x);
                min_y = qMin(min_y, y);
                max_x = qMax(max_x, x);
                max_y = qMax(max_y, y);
            }
        }
    }
    if (max_x < 0) {
        return QRect(); // fully transparent
    }
    return QRect(QPoint(min_x, min_y), QPoint(max_x, max_y));
}

QToolButton* MakeIconPillButton(const QString& icon_resource_path, const QString& tooltip,
                                QWidget* parent) {
    Q_UNUSED(tooltip);
    auto* button = new QToolButton(parent);

    // QToolButton + a QSS border-radius clip region doesn't reliably center QIcon's own
    // auto-scaled pixmap when icon size < button size (observed: visibly off-center/clipped
    // against the rounded button edge, regardless of how well-centered the source icon itself
    // is). Removing all ambiguity by making the icon canvas exactly match the button's full
    // size, so Qt has no separate icon-vs-button margin calculation left to get wrong.
    constexpr int ButtonSize = 51;
    // The icon canvas is deliberately smaller than the button. If it matched the button exactly,
    // the style's own internal button margins would shrink the icon rect below the pixmap's
    // size, and QStyle centres-and-clips rather than rescaling in that case — which chopped the
    // edges off the taller icons.
    constexpr int CanvasSize = 36;
    constexpr int ContentSize = 26; // normalized glyph size, measured edge-to-edge of real ink
    const qreal dpr = parent ? parent->devicePixelRatioF() : qApp->devicePixelRatio();
    QImage source_image(icon_resource_path);
    QPixmap canvas(qRound(CanvasSize * dpr), qRound(CanvasSize * dpr));
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(Qt::transparent);
    if (!source_image.isNull()) {
        if (source_image.format() != QImage::Format_ARGB32) {
            source_image = source_image.convertToFormat(QImage::Format_ARGB32);
        }
        // Crop away the icon's own padding so ContentSize applies to the glyph itself, making
        // every button's artwork optically the same size no matter how its PNG was authored.
        const QRect bounds = OpaqueBounds(source_image);
        if (bounds.isValid()) {
            source_image = source_image.copy(bounds);
        }
        // Fit the longest edge to ContentSize so wide and tall glyphs carry equal visual weight
        // and neither can overflow the button's rounded clip region.
        QImage scaled = source_image.scaled(qRound(ContentSize * dpr), qRound(ContentSize * dpr),
                                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap scaled_logical = QPixmap::fromImage(scaled);
        scaled_logical.setDevicePixelRatio(dpr);
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawPixmap(QPointF((CanvasSize - scaled_logical.width() / dpr) / 2.0,
                                   (CanvasSize - scaled_logical.height() / dpr) / 2.0),
                           scaled_logical);
    }

    button->setIcon(QIcon(canvas));
    button->setIconSize(QSize(CanvasSize, CanvasSize));
    // Grow on hover, dip on press. The canvas has ~15px of slack inside the button, so the
    // hover size can overshoot without the icon ever reaching the button's rounded edge.
    new ButtonPulse(button, CanvasSize, CanvasSize + 5, CanvasSize - 3);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    // Native QToolTip popups render corrupted over this window's Vulkan/MoltenVK surface, so
    // intentionally not setting one here.
    button->setFixedSize(51, 51);
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
    "#pillBar { background-color: palette(button); border-radius: 25px; }\n"
    "#tileList { background: transparent; border: none; }\n"
    "#tileList::item { border: none; padding: 0px; background: transparent; }\n"
    "#tileList::item:selected { background: transparent; }\n"
    "#tileList::item:hover { background: transparent; }\n"
    "#largeAddButton { border: 3px dashed palette(mid); border-radius: 20px;"
    " background: transparent; font-size: 32px; color: palette(text); }\n"
    "#largeAddButton:hover { background-color: palette(midlight); }\n"
    "QToolButton[pillButton=\"true\"] { border-radius: 25px; border: none;"
    " background: transparent; font-size: 16px; }\n"
    // No hover fill: the icons sit directly on the pill bar with no highlight disc behind them.
    "QToolButton[pillButton=\"true\"]:hover { background: transparent; }\n");

// All home-screen-specific user data (custom covers, live style, etc.) lives under its own
// "user" subfolder, a sibling of Azahar's existing config/sdmc/nand dirs, rather than loose
// inside config/.
QString UserDataDir() {
    const QString config_dir =
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir));
    QDir dir(config_dir);
    dir.cdUp();
    const QString user_dir = dir.absoluteFilePath(QStringLiteral("user"));
    QDir().mkpath(user_dir);
    return user_dir + QStringLiteral("/");
}

QString ExternalStylePath() {
    return UserDataDir() + QStringLiteral("home_screen_style.qss");
}

QString CustomCoversPath() {
    return UserDataDir() + QStringLiteral("custom_covers.txt");
}

// Icons are loose files inside the app bundle's Resources/home_icons/ folder rather than
// compiled into the binary via a Qt resource (.qrc), so they can be swapped without a rebuild.
QString HomeIconPath(const QString& file_name) {
    const QString app_dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MAC
    return app_dir + QStringLiteral("/../Resources/home_icons/") + file_name;
#else
    return app_dir + QStringLiteral("/home_icons/") + file_name;
#endif
}

} // namespace

class TileShelfWidget : public QListWidget {
public:
    explicit TileShelfWidget(QWidget* parent = nullptr) : QListWidget(parent) {}

    void SetNaturalWidth(int w) {
        if (natural_width == w) {
            return;
        }
        natural_width = w;
        updateGeometry();
    }

    void SetCellWidth(int w) {
        if (cell_width == w) {
            return;
        }
        cell_width = w;
        updateGeometry();
    }

    QSize sizeHint() const override {
        const QSize base = QListWidget::sizeHint();
        return QSize(natural_width > 0 ? natural_width : base.width(),
                     maximumHeight() < QWIDGETSIZE_MAX ? maximumHeight() : base.height());
    }

    QSize minimumSizeHint() const override {
        const QSize base = QListWidget::minimumSizeHint();
        const int min_w = (cell_width > 0) ? qMin(natural_width, cell_width) : 0;
        return QSize(min_w, minimumHeight() > 0 ? minimumHeight() : base.height());
    }

private:
    int natural_width = 0;
    int cell_width = 0;
};

// QSS has no transition/animation support, so a smooth per-tile hover effect (gentle scale-up +
// glow) needs an actual item delegate driving a QVariantAnimation and repainting on each tick.
// Hover enter/leave is tracked via a viewport event filter installed by the owning widget.
class TileHoverDelegate : public QStyledItemDelegate {
public:
    // Title text isn't set via QListWidgetItem::setText() (that's always rendered by Qt's
    // default delegate); it's stashed in this role instead and only painted in manually, on
    // hover/select, so idle tiles show square art with no label at all.
    static constexpr int TitleRole = Qt::UserRole + 103;
    /// Band reserved *above* the art for the hover title. The art never enters this band, so
    /// the label can't collide with the cover image the way a centered overlay would.
    static constexpr int TitleStripHeight = 56;
    /// Clear space between the title band and the top edge of the cover art.
    static constexpr int TitleGap = 6;
    /// Slack around the art inside its own cell. The hover animation scales the art up about
    /// its centre, and the view clips painting to the cell rect — without this margin the
    /// enlarged art would get its edges shaved off mid-animation.
    static constexpr int HoverPadding = 8;
    /// How much the art grows at full hover.
    static constexpr qreal HoverScale = 0.06;
    /// Mirrors HomeScreenWidget::AddDirectoryRole (private there, needed here to tell the
    /// trailing "+" tile apart from real games so it can skip the hover border).
    static constexpr int AddTileRole = Qt::UserRole + 100;
    static constexpr int FullPathRole = Qt::UserRole + 101;

    void SetLiveCoverProvider(std::function<QPixmap(const QString&)> provider) {
        live_cover_provider = std::move(provider);
    }

    explicit TileHoverDelegate(QListWidget* view) : QStyledItemDelegate(view), tile_list(view) {
        animation.setDuration(140);
        animation.setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(&animation, &QVariantAnimation::valueChanged, view,
                         [this](const QVariant& value) {
                             progress = value.toReal();
                             if (tile_list) {
                                 tile_list->viewport()->update();
                             }
                         });
    }

    void SetHoveredIndex(const QModelIndex& index) {
        if (index == hovered_index) {
            return;
        }
        hovered_index = index;
        animation.stop();
        // Moving straight from one tile to another restarts the fade from 0 rather than
        // carrying the old tile's progress over, so the newly hovered tile animates in
        // properly instead of snapping to fully-highlighted.
        progress = hovered_index.isValid() ? 0.0 : progress;
        animation.setStartValue(progress);
        animation.setEndValue(hovered_index.isValid() ? 1.0 : 0.0);
        animation.start();
        // Repaint unconditionally. If the animation's start and end values happen to match it
        // emits no valueChanged at all, so nothing else would request a repaint and the tile
        // just left would keep its stale highlight painted — showing two hovered tiles at once.
        if (tile_list) {
            tile_list->viewport()->update();
        }
    }

    // Overrides the title text color (instead of the inherited palette color) so it can track
    // the background's brightness: white text on a dark wallpaper, black text on a bright one.
    void SetTextColor(const QColor& color) {
        text_color = color;
        if (tile_list) {
            tile_list->viewport()->update();
        }
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        const QSize icon_size = tile_list->iconSize();
        return QSize(icon_size.width() + HoverPadding * 2,
                     icon_size.height() + HoverPadding * 2 + TitleStripHeight + TitleGap);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        // Only the live-hovered tile gets emphasis. Qt's native State_Selected persists on
        // whatever was last clicked even after the pointer moves elsewhere, which would show
        // two highlighted tiles at once (the old selection and the new hover) if used here.
        const qreal emphasis = (index == hovered_index) ? progress : 0.0;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

        // Cell layout, top to bottom: [HoverPadding][art][HoverPadding][title band].
        // Giving the art and the label disjoint bands is what stops the title from being
        // painted over the cover art.
        const QRect cell = option.rect;
        const QSize icon_size = tile_list->iconSize();
        // Title band sits ABOVE the art, separated by TitleGap, so the label never touches or
        // overlaps the cover image.
        QRect icon_rect(0, 0, icon_size.width(), icon_size.height());
        icon_rect.moveCenter(QPoint(cell.center().x(), cell.top() + TitleStripHeight + TitleGap +
                                                           HoverPadding + icon_size.height() / 2));

        // Taste rule 1: focus reads as scale + luminance inversion, never an outline. A plate
        // in the opposite luminance to the foreground is laid under the art and inflated past
        // it, so the focused tile gains a clean, bright (or dark) surround.
        if (emphasis > 0.0) {
            QColor plate = text_color;
            plate.setAlphaF(0.92 * emphasis);
            painter->setPen(Qt::NoPen);
            painter->setBrush(plate);
            const qreal grow = (icon_rect.width() * HoverScale) / 2.0 + 4.0 * emphasis;
            painter->drawRoundedRect(QRectF(icon_rect).adjusted(-grow, -grow, grow, grow), 7, 7);
        }

        // The art is the only thing that scales; the label stays put so it doesn't jitter.
        painter->save();
        const qreal scale = 1.0 + HoverScale * emphasis;
        painter->translate(icon_rect.center());
        painter->scale(scale, scale);
        painter->translate(-icon_rect.center());

        // If a live animated cover is playing for this item, paint its current frame;
        // otherwise fall back to the item's static decoration icon.
        const QString full_path = index.data(FullPathRole).toString();
        QPixmap live_art;
        if (live_cover_provider && !full_path.isEmpty()) {
            live_art = live_cover_provider(full_path);
        }
        if (!live_art.isNull()) {
            painter->drawPixmap(icon_rect, live_art);
        } else {
            const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
            if (!icon.isNull()) {
                painter->drawPixmap(icon_rect, icon.pixmap(icon_rect.size()));
            }
        }

        painter->restore();

        // Title fades in above the art, inside its own reserved band, supporting text stacking
        // for long game titles without truncation.
        if (emphasis > 0.0) {
            const QString title = index.data(TitleRole).toString();
            if (!title.isEmpty()) {
                const QRect title_rect(cell.left() + 4, cell.top(), cell.width() - 8,
                                       TitleStripHeight);
                QColor title_color = text_color;
                title_color.setAlphaF(emphasis);
                painter->setPen(title_color);

                QFont title_font = painter->font();
                title_font.setWeight(QFont::Bold);
                qreal font_scale = 1.55;
                int flags = Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWordWrap;

                // Adjust font scale down if needed so the stacked title fits inside title_rect
                while (font_scale > 1.0) {
                    title_font.setPointSizeF(painter->font().pointSizeF() * font_scale);
                    const QFontMetrics fm(title_font);
                    int current_flags = flags;
                    if (fm.boundingRect(title_rect, current_flags, title).width() >
                        title_rect.width()) {
                        current_flags |= Qt::TextWrapAnywhere;
                    }
                    const QRect bound = fm.boundingRect(title_rect, current_flags, title);
                    if (bound.height() <= title_rect.height() &&
                        bound.width() <= title_rect.width()) {
                        flags = current_flags;
                        break;
                    }
                    font_scale -= 0.05;
                }

                title_font.setPointSizeF(painter->font().pointSizeF() * font_scale);
                const QFontMetrics fm(title_font);
                if (fm.boundingRect(title_rect, flags, title).width() > title_rect.width()) {
                    flags |= Qt::TextWrapAnywhere;
                }
                painter->setFont(title_font);
                painter->save();
                painter->setClipRect(title_rect);
                painter->drawText(title_rect, flags, title);
                painter->restore();
            }
        }

        painter->restore();
    }

private:
    QListWidget* tile_list;
    QModelIndex hovered_index;
    QVariantAnimation animation;
    qreal progress = 0.0;
    QColor text_color = Qt::white;
    std::function<QPixmap(const QString&)> live_cover_provider;
};

// Full on-screen footprint of one tile, derived from the delegate's own layout constants so the
// view's grid/width math can never drift out of sync with what paint() actually draws.
static int TileCellWidth(int icon_size) {
    return icon_size + TileHoverDelegate::HoverPadding * 2 + TileSpacing * 2;
}

static int TileCellHeight(int icon_size) {
    return icon_size + TileHoverDelegate::HoverPadding * 2 + TileHoverDelegate::TitleStripHeight +
           TileHoverDelegate::TitleGap;
}

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
    UpdateBattery();
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
        external_style_qss = DefaultStyleQss;
    } else {
        external_style_qss = QTextStream(&file).readAll();
    }
    ApplyCombinedStylesheet();
}

void HomeScreenWidget::ApplyCombinedStylesheet() {
    // Appended after the loaded QSS so it wins the cascade (QSS applies later rules last),
    // overriding whatever "color: palette(text)" the base/user stylesheet set, without
    // requiring the user's own home_screen_style.qss to know about brightness detection.
    // The empty-state "Add Game Directory" tile is styled by #largeAddButton, whose ID selector
    // out-specifies the generic QToolButton rule above — so recolor its "+" glyph (color) and
    // its square border (border-color) here too, following the same background detection: black
    // on a light wallpaper, white on a dark one.
    // Harmony rule: the dock and the legend badges are surfaces that sit *on* the wallpaper, so
    // they must answer to the same measurement the text does — not to the theme palette. A dark
    // theme on a bright photo was producing black text on a black pill, because the text asked
    // the wallpaper and the pill asked the theme. Deriving the surface as the inverse of the
    // text colour keeps the pair in step for any wallpaper/theme combination.
    const bool dark_text = home_text_color == QColor(Qt::black);
    QColor surface = dark_text ? QColor(0xff, 0xff, 0xff) : QColor(0x16, 0x18, 0x1d);
    surface.setAlphaF(0.78); // translucent so the wallpaper still reads through
    const QString surface_rgba = QStringLiteral("rgba(%1, %2, %3, %4)")
                                     .arg(surface.red())
                                     .arg(surface.green())
                                     .arg(surface.blue())
                                     .arg(surface.alphaF(), 0, 'f', 2);

    const QString text_color_override =
        QStringLiteral("\nHomeScreenWidget, HomeScreenWidget QLabel, HomeScreenWidget "
                       "QToolButton, HomeScreenWidget QListWidget { color: %1; }\n"
                       // Plain QWidget containers (the populated/empty-state wrappers) aren't
                       // matched by the class-specific rules above, so they fall through to the
                       // active theme's app-wide stylesheet and paint its lighter panel colour —
                       // showing up as a second, mismatched grey slab behind the shelf. Force
                       // every descendant transparent; #pillBar keeps its fill via its ID rule,
                       // which out-specifies this.
                       "HomeScreenWidget QWidget { background: transparent; }\n"
                       "#largeAddButton { color: %1; border-color: %1; }\n"
                       // Dock fill tracks the wallpaper, not the theme (see above).
                       "#pillBar { background-color: %2; }\n")
            .arg(home_text_color.name(), surface_rgba);
    setStyleSheet(external_style_qss + text_color_override);
}

// Fully custom white/black theme toggle (no native system palette involved).
bool IsWhiteTheme() {
    return UISettings::values.theme == QStringLiteral("white");
}

void HomeScreenWidget::UpdateTextColorForBackground() {
    // No wallpaper means paintEvent falls back to the solid theme base fill — white theme is a
    // light base (black text), every other theme is a dark base (white text). With a wallpaper,
    // judge legibility by its actual brightness instead.
    const qreal base_luminance = IsWhiteTheme() ? 1.0 : 0.0;
    // Sample the two bands that actually carry text (top status strip and bottom dock/legend
    // strip) and take the brighter of the two, so a wallpaper that is bright in either place
    // forces dark text rather than averaging that brightness away against the rest of the
    // image. This is what the plain whole-image average got wrong on light photos.
    qreal luminance = base_luminance;
    if (!background_scaled.isNull()) {
        const qreal top = RegionLuminance(background_scaled, QRectF(0.0, 0.0, 1.0, 0.16));
        const qreal bottom = RegionLuminance(background_scaled, QRectF(0.0, 0.84, 1.0, 0.16));
        luminance = qMax(top, bottom);
    }
    const QColor new_color = luminance > 0.6 ? QColor(Qt::black) : QColor(Qt::white);
    if (new_color == home_text_color) {
        return;
    }
    home_text_color = new_color;
    if (tile_hover_delegate) {
        tile_hover_delegate->SetTextColor(home_text_color);
    }
    ApplyCombinedStylesheet();
    UpdateBattery();   // the icon is painted, so it needs re-rendering in the new colour
    UpdateLegendBar(); // ditto for the legend's glyph badges
    // The "+" add-directory tile is a painted pixmap, not QSS, so repaint it with the new color.
    // Guarded by the early-return above, this can't loop (Refresh doesn't change the color).
    if (tile_list) {
        Refresh();
    }
    update(); // scrim colour is derived from home_text_color
}

HomeScreenWidget::~HomeScreenWidget() {
    if (background_movie) {
        background_movie->stop();
        delete background_movie;
        background_movie = nullptr;
    }
    for (auto* movie : cover_movies) {
        if (movie) {
            movie->stop();
            delete movie;
        }
    }
    cover_movies.clear();
}

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
    QFont clock_font = clock_label->font();
    clock_font.setPointSizeF(clock_font.pointSizeF() * 1.2);
    clock_label->setFont(clock_font);

    battery_label = new QLabel(this);
    battery_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    battery_percent_label = new QLabel(this);
    battery_percent_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    top_bar->addWidget(title_label);
    top_bar->addStretch();
    top_bar->addWidget(battery_label);
    top_bar->addSpacing(4);
    top_bar->addWidget(battery_percent_label);
    top_bar->addSpacing(12);
    top_bar->addWidget(clock_label);
    main_layout->addLayout(top_bar);

    // Empty state: one big "+" prompt, pinned left, vertically centered. Shown when no
    // games are found.
    empty_state_container = new QWidget(this);
    auto* empty_v = new QVBoxLayout(empty_state_container);
    empty_v->setContentsMargins(0, 0, 0, 0);
    empty_v->addStretch(1);
    auto* empty_row = new QHBoxLayout();
    large_add_button = new QToolButton(empty_state_container);
    large_add_button->setFixedSize(240, 240);
    large_add_button->setFocusPolicy(Qt::NoFocus);
    large_add_button->setText(QStringLiteral("+\n") + tr("Add Game Directory"));
    large_add_button->setObjectName(QStringLiteral("largeAddButton"));
    connect(large_add_button, &QToolButton::clicked, this, &HomeScreenWidget::AddDirectory);
    empty_row->addWidget(large_add_button);
    empty_row->addStretch(1);
    empty_v->addLayout(empty_row);
    empty_v->addStretch(1);
    main_layout->addWidget(empty_state_container, 1);

    // Populated state: bottom-center tile shelf + pill dock.
    populated_container = new QWidget(this);
    auto* populated_v = new QVBoxLayout(populated_container);
    populated_v->setContentsMargins(0, 0, 0, 0);
    populated_v->setSpacing(10);
    populated_v->addStretch(1);

    tile_list = new TileShelfWidget(populated_container);
    tile_list->setViewMode(QListWidget::IconMode);
    tile_list->setFlow(QListWidget::LeftToRight);
    tile_list->setWrapping(false);
    tile_list->setResizeMode(QListWidget::Adjust);
    tile_list->setMovement(QListWidget::Static);
    tile_list->setIconSize(QSize(tile_icon_size, tile_icon_size));
    tile_list->setGridSize(QSize(TileCellWidth(tile_icon_size), TileCellHeight(tile_icon_size)));
    tile_list->setSpacing(TileSpacing);
    tile_list->setUniformItemSizes(true);
    tile_list->setSelectionMode(QAbstractItemView::SingleSelection);
    tile_list->setFrameShape(QFrame::NoFrame);
    // Hidden, not disabled: ScrollBarAlwaysOff only suppresses the visible scrollbar widget,
    // wheel/trackpad scrolling through QAbstractItemView still works normally.
    tile_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tile_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tile_list->setFixedHeight(TileCellHeight(tile_icon_size) + TileSpacing * 2);
    tile_list->setObjectName(QStringLiteral("tileList"));
    tile_list->setContextMenuPolicy(Qt::CustomContextMenu);
    tile_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    tile_list->setMouseTracking(true);

    tile_hover_delegate = new TileHoverDelegate(tile_list);
    tile_hover_delegate->SetLiveCoverProvider([this](const QString& full_path) -> QPixmap {
        auto it = cover_movies.find(full_path);
        if (it != cover_movies.end() && it.value() && it.value()->isValid()) {
            const QPixmap frame = it.value()->currentPixmap();
            if (!frame.isNull()) {
                return RoundedLivePixmap(frame, tile_icon_size, 3, devicePixelRatioF());
            }
        }
        return QPixmap();
    });
    tile_list->setItemDelegate(tile_hover_delegate);
    tile_list->viewport()->installEventFilter(this);
    connect(tile_list, &QListWidget::entered, this,
            [this](const QModelIndex& index) { tile_hover_delegate->SetHoveredIndex(index); });

    // Console-style activation: a single click launches, matching the home menu. Qt's
    // itemActivated only fires on the platform's double-click gesture, so itemClicked is the
    // one wired up here; itemActivated stays connected as well so Enter/keyboard (and later
    // controller) activation still works. OnTileActivated debounces so the two can't both fire
    // for one gesture and boot the same game twice.
    connect(tile_list, &QListWidget::itemClicked, this, &HomeScreenWidget::OnTileActivated);
    connect(tile_list, &QListWidget::itemActivated, this, &HomeScreenWidget::OnTileActivated);
    connect(tile_list, &QListWidget::customContextMenuRequested, this,
            &HomeScreenWidget::OnTileContextMenu);

    populated_v->addWidget(tile_list, 0, Qt::AlignHCenter);

    main_layout->addWidget(populated_container, 1);
    populated_container->hide();

    // Floating pill-shaped dock. Always visible, independent of whether any games are loaded,
    // since Settings/Power are useful either way. It is deliberately NOT placed in main_layout:
    // it's positioned by hand in ApplyPillBarPlacement() so the user can park it along the
    // bottom or down the left edge without restructuring the widget tree.
    pill_bar = new QWidget(this);
    pill_bar->setObjectName(QStringLiteral("pillBar"));
    pill_layout = new QBoxLayout(QBoxLayout::LeftToRight, pill_bar);
    pill_layout->setSpacing(6);

    profile_button =
        MakeIconPillButton(HomeIconPath(QStringLiteral("profile.png")), tr("Profile"), pill_bar);
    UpdateProfileButtonIcon();
    auto* settings_button =
        MakeIconPillButton(HomeIconPath(QStringLiteral("settings.png")), tr("Settings"), pill_bar);
    auto* add_dir_button = MakeIconPillButton(HomeIconPath(QStringLiteral("adddir.png")),
                                              tr("Add Game Directory"), pill_bar);
    auto* wallpaper_button = MakeIconPillButton(HomeIconPath(QStringLiteral("gallery.png")),
                                                tr("Set Background Image"), pill_bar);
    auto* power_button =
        MakeIconPillButton(HomeIconPath(QStringLiteral("power.png")), tr("Quit"), pill_bar);

    connect(profile_button, &QToolButton::clicked, this, &HomeScreenWidget::OpenProfile);
    connect(settings_button, &QToolButton::clicked, this, &HomeScreenWidget::OpenSettings);
    connect(add_dir_button, &QToolButton::clicked, this, &HomeScreenWidget::AddDirectory);
    connect(wallpaper_button, &QToolButton::clicked, this, &HomeScreenWidget::OnChooseWallpaper);
    connect(power_button, &QToolButton::clicked, this, &HomeScreenWidget::PowerOff);

    // The buttons are a fixed 51px inside a 61px-thick pill, so without an explicit alignment
    // they settle against the top-left corner of their slot instead of sitting on the pill's
    // centre line. AlignCenter keeps them centred in whichever direction the dock is running.
    for (auto* button :
         {profile_button, settings_button, add_dir_button, wallpaper_button, power_button}) {
        pill_layout->addWidget(button, 0, Qt::AlignCenter);
    }

    // Taste rule 4: a persistent, docked button legend. It is the primary affordance on this
    // screen, which is why scrollbars and tooltips are absent elsewhere in this UI.
    legend_bar = new QWidget(this);
    legend_bar->setObjectName(QStringLiteral("legendBar"));
    auto* legend_layout = new QHBoxLayout(legend_bar);
    legend_layout->setContentsMargins(0, 0, 0, 0);
    legend_layout->setSpacing(6);
    for (const auto& entry : {std::pair<const char*, const char*>{"Y", QT_TR_NOOP("Options")},
                              std::pair<const char*, const char*>{"A", QT_TR_NOOP("Open")}}) {
        auto* glyph = new QLabel(legend_bar);
        glyph->setObjectName(QStringLiteral("legendGlyph"));
        glyph->setProperty("letter", QString::fromUtf8(entry.first));
        auto* label = new QLabel(tr(entry.second), legend_bar);
        label->setObjectName(QStringLiteral("legendText"));
        legend_layout->addWidget(glyph);
        legend_layout->addWidget(label);
        legend_layout->addSpacing(10);
    }

    ApplyPillBarPlacement();
    UpdateLegendBar();
}

// Repaints the legend glyphs in the current foreground colour and re-docks the bar.
void HomeScreenWidget::UpdateLegendBar() {
    if (!legend_bar) {
        return;
    }
    // Same harmony rule as the dock: knock the letter out in the inverse of the glyph colour,
    // which is itself derived from the wallpaper — so the badges never end up dark-on-dark.
    const QColor surface =
        home_text_color == QColor(Qt::black) ? QColor(0xff, 0xff, 0xff) : QColor(0x16, 0x18, 0x1d);
    for (QLabel* glyph : legend_bar->findChildren<QLabel*>(QStringLiteral("legendGlyph"))) {
        glyph->setPixmap(RenderGlyphBadge(glyph->property("letter").toString(), home_text_color,
                                          surface, devicePixelRatioF()));
    }
    for (QLabel* label : legend_bar->findChildren<QLabel*>(QStringLiteral("legendText"))) {
        QFont font = label->font();
        font.setWeight(QFont::DemiBold);
        label->setFont(font);
    }
    legend_bar->adjustSize();
    constexpr int EdgeMargin = 14;
    legend_bar->move(width() - legend_bar->width() - EdgeMargin,
                     height() - legend_bar->height() - EdgeMargin - 4);
    legend_bar->raise();
}

// Positions the dock by hand (it isn't in any layout) and reserves room for it in the content
// margins so the tile shelf never slides underneath it.
void HomeScreenWidget::ApplyPillBarPlacement() {
    if (!pill_bar || !pill_layout) {
        return;
    }
    constexpr int EdgeMargin = 14;
    const bool vertical = UISettings::values.home_pill_position == 1;

    pill_layout->setDirection(vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    pill_layout->setContentsMargins(vertical ? 0 : 10, vertical ? 10 : 0, vertical ? 0 : 10,
                                    vertical ? 10 : 0);
    // Lock the cross-axis to the pill's thickness and let the main axis size to its buttons, so
    // the rounded-rect background stays a true pill in either orientation.
    pill_bar->setFixedSize(QSize(vertical ? PillBarHeight : pill_bar->sizeHint().width(),
                                 vertical ? pill_bar->sizeHint().height() : PillBarHeight));

    auto* main_layout = qobject_cast<QVBoxLayout*>(layout());
    if (main_layout) {
        // Reserve the dock's lane so content is laid out beside it rather than behind it.
        main_layout->setContentsMargins(vertical ? 16 + PillBarHeight + EdgeMargin : 16, 12, 16,
                                        vertical ? 12 : PillBarHeight + EdgeMargin);
    }

    if (vertical) {
        pill_bar->move(EdgeMargin, (height() - pill_bar->height()) / 2);
    } else {
        pill_bar->move((width() - pill_bar->width()) / 2,
                       height() - pill_bar->height() - EdgeMargin);
    }
    pill_bar->raise();
}

void HomeScreenWidget::ApplyAppearanceSettings() {
    if (background_movie) {
        background_movie->stop();
        delete background_movie;
        background_movie = nullptr;
    }
    const QString& bg_path = UISettings::values.home_background_path;
    if (!bg_path.isEmpty() && QFile::exists(bg_path)) {
        auto* movie = new QMovie(bg_path, QByteArray(), this);
        if (movie->isValid() && (movie->frameCount() > 1 || movie->frameCount() == 0)) {
            background_movie = movie;
            connect(background_movie, &QMovie::frameChanged, this,
                    &HomeScreenWidget::OnBackgroundMovieFrame);
            connect(background_movie, &QMovie::finished, background_movie, &QMovie::start);
            background_movie->start();
            background_pixmap = background_movie->currentPixmap();
        } else {
            delete movie;
            const QPixmap source(bg_path);
            background_pixmap = source;
        }
    } else {
        background_pixmap = QPixmap();
    }
    RebuildScaledBackground();

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
    QFont clock_font = font;
    clock_font.setPointSizeF(font.pointSizeF() * 1.2);
    clock_label->setFont(clock_font);
    QFont battery_font = font;
    battery_font.setPointSizeF(font.pointSizeF() * 0.95);
    battery_percent_label->setFont(battery_font);
    tile_list->setFont(font);

    ApplyPillBarPlacement(); // dock side is an appearance setting too
    UpdateLegendBar();
    UpdateProfileButtonIcon();
    update();
}

void HomeScreenWidget::UpdateProfileButtonIcon() {
    if (!profile_button) {
        return;
    }
    constexpr int CanvasSize = 36;
    const qreal dpr = devicePixelRatioF();

    const QString& custom_path = UISettings::values.profile_avatar_path;
    if (!custom_path.isEmpty() && QFile::exists(custom_path)) {
        QImage src_img(custom_path);
        if (!src_img.isNull()) {
            if (src_img.format() != QImage::Format_ARGB32_Premultiplied) {
                src_img = src_img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            }
            const int min_dim = qMin(src_img.width(), src_img.height());
            const int sx = (src_img.width() - min_dim) / 2;
            const int sy = (src_img.height() - min_dim) / 2;
            QImage square = src_img.copy(sx, sy, min_dim, min_dim);

            QImage scaled = square.scaled(qRound(CanvasSize * dpr), qRound(CanvasSize * dpr),
                                          Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            QPixmap canvas(qRound(CanvasSize * dpr), qRound(CanvasSize * dpr));
            canvas.setDevicePixelRatio(dpr);
            canvas.fill(Qt::transparent);

            QPainter painter(&canvas);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            QPainterPath circle_path;
            circle_path.addEllipse(QRectF(0, 0, CanvasSize, CanvasSize));
            painter.setClipPath(circle_path);
            QPixmap scaled_pix = QPixmap::fromImage(scaled);
            scaled_pix.setDevicePixelRatio(dpr);
            painter.drawPixmap(0, 0, scaled_pix);

            profile_button->setIcon(QIcon(canvas));
            profile_button->setIconSize(QSize(CanvasSize, CanvasSize));
            return;
        }
    }

    QImage source_image(HomeIconPath(QStringLiteral("profile.png")));
    QPixmap canvas(qRound(CanvasSize * dpr), qRound(CanvasSize * dpr));
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(Qt::transparent);
    if (!source_image.isNull()) {
        if (source_image.format() != QImage::Format_ARGB32) {
            source_image = source_image.convertToFormat(QImage::Format_ARGB32);
        }
        constexpr int ContentSize = 26;
        const QRect bounds = OpaqueBounds(source_image);
        if (bounds.isValid()) {
            source_image = source_image.copy(bounds);
        }
        QImage scaled = source_image.scaled(qRound(ContentSize * dpr), qRound(ContentSize * dpr),
                                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap scaled_logical = QPixmap::fromImage(scaled);
        scaled_logical.setDevicePixelRatio(dpr);
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawPixmap(QPointF((CanvasSize - scaled_logical.width() / dpr) / 2.0,
                                   (CanvasSize - scaled_logical.height() / dpr) / 2.0),
                           scaled_logical);
    }
    profile_button->setIcon(QIcon(canvas));
    profile_button->setIconSize(QSize(CanvasSize, CanvasSize));
}

void HomeScreenWidget::OnBackgroundMovieFrame() {
    if (background_movie) {
        background_pixmap = background_movie->currentPixmap();
        update();
    }
}

void HomeScreenWidget::paintEvent(QPaintEvent* event) {
    {
        // Solid theme base fill: white for the custom White theme, dark for everything else.
        QPainter base_painter(this);
        // Taste rule 2: same base hue the settings panel and palette are derived from.
        base_painter.fillRect(rect(),
                              IsWhiteTheme() ? QColor(0xff, 0xff, 0xff) : QColor(0x16, 0x18, 0x1d));
    }
    const bool is_animated = background_movie && !background_pixmap.isNull();
    if (is_animated || !background_scaled.isNull()) {
        QPainter painter(this);
        if (is_animated) {
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            const QSize img_size = background_pixmap.size();
            const QSize scaled = img_size.scaled(size(), Qt::KeepAspectRatioByExpanding);
            const int x = (width() - scaled.width()) / 2;
            const int y = (height() - scaled.height()) / 2;
            painter.drawPixmap(QRect(x, y, scaled.width(), scaled.height()), background_pixmap);
        } else {
            const int x = qRound((width() - background_scaled.width() / devicePixelRatioF()) / 2.0);
            const int y =
                qRound((height() - background_scaled.height() / devicePixelRatioF()) / 2.0);
            painter.drawPixmap(x, y, background_scaled);
        }

        // Contrast scrims. Region sampling alone can't rescue a strip that is genuinely
        // half-bright and half-dark, so the two bands that carry text get a soft gradient in
        // the opposite luminance. It stays subtle over friendly wallpapers and does the heavy
        // lifting over hostile ones, which is what keeps text legible on *any* upload.
        const QColor scrim = home_text_color == QColor(Qt::white) ? Qt::black : Qt::white;
        const int band = qMax(64, height() / 6);

        QLinearGradient top(0, 0, 0, band);
        QColor top_strong = scrim;
        top_strong.setAlphaF(0.45);
        QColor clear = scrim;
        clear.setAlphaF(0.0);
        top.setColorAt(0.0, top_strong);
        top.setColorAt(1.0, clear);
        painter.fillRect(QRect(0, 0, width(), band), top);

        QLinearGradient bottom(0, height() - band, 0, height());
        bottom.setColorAt(0.0, clear);
        bottom.setColorAt(1.0, top_strong);
        painter.fillRect(QRect(0, height() - band, width(), band), bottom);
    }
    QWidget::paintEvent(event);
}

// Scales the untouched wallpaper source straight to the current widget size, once, and
// re-derives the per-region foreground colours from that scaled copy.
void HomeScreenWidget::RebuildScaledBackground() {
    if (background_movie && background_movie->isValid()) {
        background_pixmap = background_movie->currentPixmap();
        if (!background_pixmap.isNull()) {
            const qreal dpr = devicePixelRatioF();
            const QSize target = size() * dpr;
            background_scaled = background_pixmap.scaled(target, Qt::KeepAspectRatioByExpanding,
                                                         Qt::SmoothTransformation);
            background_scaled.setDevicePixelRatio(dpr);
            UpdateTextColorForBackground();
        }
        return;
    }

    if (background_pixmap.isNull() || width() <= 0 || height() <= 0) {
        background_scaled = QPixmap();
        return;
    }
    const qreal dpr = devicePixelRatioF();
    const QSize target = size() * dpr;
    // How far past its native size the source has to stretch to fill the window.
    const qreal upscale = qMax(qreal(target.width()) / qMax(1, background_pixmap.width()),
                               qreal(target.height()) / qMax(1, background_pixmap.height()));
    QPixmap scaled =
        background_pixmap.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    if (upscale > 1.6) {
        // Blur in proportion to how badly the image is being stretched, capped so a very small
        // source doesn't turn into a featureless smear.
        const int radius = qBound(1, qRound((upscale - 1.0) * 1.6), 8);
        scaled = QPixmap::fromImage(BoxBlur(scaled.toImage(), radius));
    }
    background_scaled = scaled;
    background_scaled.setDevicePixelRatio(dpr);
    UpdateTextColorForBackground();
}

void HomeScreenWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    RebuildScaledBackground();
    ApplyPillBarPlacement(); // hand-positioned, so it has to follow the window
    UpdateLegendBar();
    UpdateTileListWidth();
}

void HomeScreenWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (background_movie && background_movie->state() == QMovie::Paused) {
        background_movie->setPaused(false);
    }
    for (auto* movie : cover_movies) {
        if (movie && movie->state() == QMovie::Paused) {
            movie->setPaused(false);
        }
    }
}

void HomeScreenWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (background_movie && background_movie->state() == QMovie::Running) {
        background_movie->setPaused(true);
    }
    for (auto* movie : cover_movies) {
        if (movie && movie->state() == QMovie::Running) {
            movie->setPaused(true);
        }
    }
}

bool HomeScreenWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == tile_list->viewport()) {
        if (event->type() == QEvent::Leave) {
            tile_hover_delegate->SetHoveredIndex(QModelIndex());
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                shelf_dragging = false;
                drag_start_pos = me->pos();
                QScrollBar* bar = tile_list->horizontalScrollBar();
                drag_start_scroll = bar ? bar->value() : 0;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                const int dx = me->pos().x() - drag_start_pos.x();
                if (!shelf_dragging && std::abs(dx) > QApplication::startDragDistance()) {
                    shelf_dragging = true;
                    if (scroll_animation) {
                        scroll_animation->stop();
                    }
                }
                if (shelf_dragging) {
                    QScrollBar* bar = tile_list->horizontalScrollBar();
                    if (bar) {
                        bar->setValue(drag_start_scroll - dx);
                    }
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (shelf_dragging) {
                shelf_dragging = false;
                return true;
            }
        } else if (event->type() == QEvent::Wheel) {
            auto* wheel = static_cast<QWheelEvent*>(event);
            QScrollBar* bar = tile_list->horizontalScrollBar();
            if (!bar || bar->maximum() == 0) {
                return QWidget::eventFilter(watched, event);
            }
            int delta = wheel->pixelDelta().x();
            if (delta == 0) {
                delta = wheel->pixelDelta().y();
            }
            if (delta == 0) {
                const QPoint angle = wheel->angleDelta();
                const int ticks = (angle.x() != 0 ? angle.x() : angle.y());
                delta = ticks * tile_icon_size / 120; // one notch ≈ one tile
            }
            if (delta == 0) {
                return QWidget::eventFilter(watched, event);
            }

            if (!scroll_animation) {
                scroll_animation = new QPropertyAnimation(bar, "value", this);
                scroll_animation->setEasingCurve(QEasingCurve::OutCubic);
                scroll_animation->setDuration(260);
            }
            const bool running = scroll_animation->state() == QAbstractAnimation::Running;
            const int base = running ? scroll_animation->endValue().toInt() : bar->value();
            const int target = qBound(bar->minimum(), base - delta, bar->maximum());
            if (target != bar->value() || running) {
                scroll_animation->stop();
                scroll_animation->setStartValue(bar->value());
                scroll_animation->setEndValue(target);
                scroll_animation->start();
            }
            return true; // swallow it so Qt's own stepped scrolling doesn't fight the animation
        }
    }
    return QWidget::eventFilter(watched, event);
}

void HomeScreenWidget::UpdateClock() {
    clock_label->setText(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm")));
    UpdateBattery();
}

void HomeScreenWidget::UpdateBattery() {
    if (!battery_label) {
        return;
    }
    const BatteryState state = ReadBatteryState();
    battery_label->setPixmap(RenderBatteryIcon(state, home_text_color, devicePixelRatioF()));
    // Taste rule 7: the status cluster stays quiet, so the percentage rides beside the pill at
    // a smaller size rather than competing with the clock.
    battery_percent_label->setText(QStringLiteral("%1%").arg(state.percent));
    battery_label->setToolTip(state.charging ? tr("Battery %1% (charging)").arg(state.percent)
                                             : tr("Battery %1%").arg(state.percent));
}

void HomeScreenWidget::UpdateTileListWidth() {
    const int count = tile_list->count();
    const int cell_width = TileCellWidth(tile_icon_size);
    const int natural_width = count * cell_width;
    tile_list->SetCellWidth(cell_width);
    tile_list->SetNaturalWidth(natural_width);
    tile_list->setMaximumWidth(natural_width);
    tile_list->setMinimumWidth(qMin(natural_width, cell_width));
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
    if (tile_hover_delegate) {
        tile_hover_delegate->SetHoveredIndex(QModelIndex());
    }
    for (auto* movie : cover_movies) {
        if (movie) {
            movie->stop();
            delete movie;
        }
    }
    cover_movies.clear();
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
            const QString& cover_path = cover_it.value();
            auto* movie = new QMovie(cover_path, QByteArray(), this);
            if (movie->isValid() && (movie->frameCount() > 1 || movie->frameCount() == 0)) {
                connect(movie, &QMovie::frameChanged, this, [this](int) {
                    if (tile_list) {
                        tile_list->viewport()->update();
                    }
                });
                connect(movie, &QMovie::finished, movie, &QMovie::start);
                movie->start();
                cover_movies.insert(full_path, movie);
                icon = movie->currentPixmap();
            } else {
                delete movie;
                icon = QPixmap(cover_path);
            }
        }
        if (icon.isNull()) {
            icon = item->data(Qt::DecorationRole).value<QPixmap>();
        }
        if (icon.isNull()) {
            icon = QPixmap(tile_icon_size, tile_icon_size);
            icon.fill(Qt::darkGray);
        }

        // Near-square corners (radius 3) matching the reference tile shelf, not the
        // heavily rounded 16px used elsewhere in this UI.
        auto* tile = new QListWidgetItem(
            QIcon(RoundedPixmap(icon, tile_icon_size, 3, devicePixelRatioF())), QString());
        tile->setData(FullPathRole, full_path);
        tile->setData(TileHoverDelegate::TitleRole, title);
        tile_list->addItem(tile);
        ++games_added;
    }

    // Trailing "+" tile to add a new game directory.
    const qreal dpr = devicePixelRatioF();
    QPixmap plus_pixmap(qRound(tile_icon_size * dpr), qRound(tile_icon_size * dpr));
    plus_pixmap.setDevicePixelRatio(dpr);
    plus_pixmap.fill(Qt::transparent);
    {
        QPainter painter(&plus_pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        // Follow background detection: black "+" and square on a light wallpaper, white on dark.
        QPen pen(home_text_color);
        pen.setWidth(3);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawRoundedRect(2, 2, tile_icon_size - 4, tile_icon_size - 4, 3, 3);
        QFont plus_font = font();
        plus_font.setPointSize(36);
        painter.setFont(plus_font);
        painter.drawText(QRect(0, 0, tile_icon_size, tile_icon_size), Qt::AlignCenter,
                         QStringLiteral("+"));
    }
    auto* add_tile = new QListWidgetItem(QIcon(plus_pixmap), QString());
    add_tile->setData(AddDirectoryRole, true);
    add_tile->setData(TileHoverDelegate::TitleRole, tr("Add Game Directory"));
    tile_list->addItem(add_tile);

    UpdateTileListWidth();

    empty_state_container->setVisible(games_added == 0);
    populated_container->setVisible(games_added > 0);
}

void HomeScreenWidget::OnChooseWallpaper() {
    const QString image_path =
        QFileDialog::getOpenFileName(this, tr("Select Background Image"), QString(),
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
            // Copy into our own user data folder rather than keeping a reference to wherever
            // the user picked it from, so the cover survives the source file moving/deleting
            // and travels with the app's user data.
            const QString covers_dir = UserDataDir() + QStringLiteral("covers/");
            QDir().mkpath(covers_dir);
            const QString ext = QFileInfo(image_path).suffix();
            const QString stored_path = covers_dir + QString::number(qHash(full_path), 16) +
                                        (ext.isEmpty() ? QString() : QStringLiteral(".") + ext);
            QFile::remove(stored_path);
            QFile::copy(image_path, stored_path);
            custom_covers.insert(full_path, stored_path);
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
    // Both itemClicked and itemActivated route here so a single click launches while keyboard
    // (and controller) activation keeps working — but a double-click fires both, so ignore any
    // follow-up within the platform's double-click window.
    if (tile_activation_locked) {
        return;
    }
    tile_activation_locked = true;
    QTimer::singleShot(QApplication::doubleClickInterval(), this,
                       [this] { tile_activation_locked = false; });

    if (item->data(AddDirectoryRole).toBool()) {
        emit AddDirectory();
        return;
    }
    const QString full_path = item->data(FullPathRole).toString();
    if (!full_path.isEmpty()) {
        emit GameChosen(full_path);
    }
}
