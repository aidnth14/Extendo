// Copyright 2026 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <functional>
#include <utility>
#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QVariant>
#include <QVariantAnimation>
#include "citra_qt/console_modal.h"

#include <algorithm>
#include <vector>
#include <QApplication>
#include <QButtonGroup>
#include <QDate>
#include <QEvent>
#include <QEventLoop>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStyleHints>
#include <QVBoxLayout>
#include "citra_qt/uisounds.h"
#include "citra_qt/uisettings.h"

namespace ConsoleUI {

static bool IsCurrentThemeLight() {
    const QString& theme = UISettings::values.theme;
    if (theme == QStringLiteral("white")) {
        return true;
    }
    if (theme.isEmpty() || theme == QStringLiteral("default")) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (const auto* hints = QGuiApplication::styleHints()) {
            return hints->colorScheme() != Qt::ColorScheme::Dark;
        }
#endif
        return QGuiApplication::palette().color(QPalette::Window).lightness() >= 128;
    }
    return false;
}

struct ThemePalette {
    bool is_light;
    QColor backdrop;
    QString card_bg;
    QString card_border;
    QString title_color;
    QString subtitle_color;
    QString divider_color;
    QString scroll_bar_handle;

    QString item_bg;
    QString item_bg_hover;
    QString item_text;

    QString item_sel_bg;
    QString item_sel_text;
    QString item_sel_check;

    QString edit_bg;
    QString edit_border;
    QString edit_text;
    QString edit_counter;

    QString stepper_bg;
    QString stepper_text;

    QString back_btn_bg;
    QString back_btn_hover;
    QString back_btn_text;

    QString primary_btn_bg;
    QString primary_btn_hover;
    QString primary_btn_text;

    QString secondary_btn_bg;
    QString secondary_btn_hover;
    QString secondary_btn_text;
};

static ThemePalette GetThemePalette() {
    bool light = IsCurrentThemeLight();
    ThemePalette p;
    p.is_light = light;
    p.backdrop = QColor(0, 0, 0, light ? 140 : 160);

    if (light) {
        p.card_bg = QStringLiteral("#FFFFFF");
        p.card_border = QStringLiteral("1px solid rgba(0, 0, 0, 0.08)");
        p.title_color = QStringLiteral("#16181D");
        p.subtitle_color = QStringLiteral("rgba(22, 24, 29, 0.68)");
        p.divider_color = QStringLiteral("rgba(0, 0, 0, 0.08)");
        p.scroll_bar_handle = QStringLiteral("rgba(0, 0, 0, 0.20)");

        p.item_bg = QStringLiteral("rgba(0, 0, 0, 0.05)");
        p.item_bg_hover = QStringLiteral("rgba(0, 0, 0, 0.09)");
        p.item_text = QStringLiteral("#16181D");

        // Luminance Inversion: solid dark tile with pure white text
        p.item_sel_bg = QStringLiteral("#16181D");
        p.item_sel_text = QStringLiteral("#FFFFFF");
        p.item_sel_check = QStringLiteral("#FFFFFF");

        p.edit_bg = QStringLiteral("rgba(0, 0, 0, 0.05)");
        p.edit_border = QStringLiteral("2px solid rgba(0, 0, 0, 0.12)");
        p.edit_text = QStringLiteral("#16181D");
        p.edit_counter = QStringLiteral("rgba(22, 24, 29, 0.50)");

        p.stepper_bg = QStringLiteral("rgba(0, 0, 0, 0.05)");
        p.stepper_text = QStringLiteral("#16181D");

        p.back_btn_bg = QStringLiteral("rgba(0, 0, 0, 0.06)");
        p.back_btn_hover = QStringLiteral("rgba(0, 0, 0, 0.12)");
        p.back_btn_text = QStringLiteral("#16181D");

        p.primary_btn_bg = QStringLiteral("#3C8CE0");
        p.primary_btn_hover = QStringLiteral("#529FF0");
        p.primary_btn_text = QStringLiteral("#FFFFFF");

        p.secondary_btn_bg = QStringLiteral("rgba(224, 60, 60, 0.09)");
        p.secondary_btn_hover = QStringLiteral("rgba(224, 60, 60, 0.16)");
        p.secondary_btn_text = QStringLiteral("#D32F2F");
    } else {
        p.card_bg = QStringLiteral("#20242D");
        p.card_border = QStringLiteral("1px solid rgba(255, 255, 255, 0.12)");
        p.title_color = QStringLiteral("#FFFFFF");
        p.subtitle_color = QStringLiteral("rgba(255, 255, 255, 0.65)");
        p.divider_color = QStringLiteral("rgba(255, 255, 255, 0.10)");
        p.scroll_bar_handle = QStringLiteral("rgba(255, 255, 255, 0.25)");

        p.item_bg = QStringLiteral("rgba(255, 255, 255, 0.08)");
        p.item_bg_hover = QStringLiteral("rgba(255, 255, 255, 0.16)");
        p.item_text = QStringLiteral("#E6E9F0");

        // Luminance Inversion: solid pure white with dark bold text
        p.item_sel_bg = QStringLiteral("#FFFFFF");
        p.item_sel_text = QStringLiteral("#14171F");
        p.item_sel_check = QStringLiteral("#14171F");

        p.edit_bg = QStringLiteral("rgba(255, 255, 255, 0.09)");
        p.edit_border = QStringLiteral("2px solid rgba(255, 255, 255, 0.18)");
        p.edit_text = QStringLiteral("#FFFFFF");
        p.edit_counter = QStringLiteral("rgba(255, 255, 255, 0.50)");

        p.stepper_bg = QStringLiteral("rgba(255, 255, 255, 0.08)");
        p.stepper_text = QStringLiteral("#FFFFFF");

        p.back_btn_bg = QStringLiteral("rgba(255, 255, 255, 0.08)");
        p.back_btn_hover = QStringLiteral("rgba(255, 255, 255, 0.16)");
        p.back_btn_text = QStringLiteral("#FFFFFF");

        p.primary_btn_bg = QStringLiteral("#3C8CE0");
        p.primary_btn_hover = QStringLiteral("#529FF0");
        p.primary_btn_text = QStringLiteral("#FFFFFF");

        p.secondary_btn_bg = QStringLiteral("rgba(224, 60, 60, 0.16)");
        p.secondary_btn_hover = QStringLiteral("rgba(224, 60, 60, 0.26)");
        p.secondary_btn_text = QStringLiteral("#FF6B6B");
    }
    return p;
}

static QIcon MakeGlyphIcon(const QString& glyph, const QColor& bg_color, int size = 24) {
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg_color);
    p.drawEllipse(0, 0, size, size);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(size <= 24 ? 11 : 13);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, glyph);
    return QIcon(pix);
}

static QString EscapeButtonLabel(const QString& text) {
    QString out;
    out.reserve(text.size() + 4);
    for (int i = 0; i < text.size(); ++i) {
        if (text[i] == QLatin1Char('&')) {
            out.append(QLatin1String("&&"));
            if (i + 1 < text.size() && text[i + 1] == QLatin1Char('&')) {
                ++i;
            }
        } else {
            out.append(text[i]);
        }
    }
    return out;
}

class BaseConsoleModal : public QWidget {
public:
    /// Panels come in fast and leave faster; a slow close makes the whole UI feel sluggish
    /// even when the open felt fine.
    static constexpr int PopInMs = 150;
    static constexpr int PopOutMs = 110;

    explicit BaseConsoleModal(QWidget* parent, bool allow_outside_click = true)
        : QWidget(ResolveParent(parent)), dismiss_on_backdrop_click(allow_outside_click) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        if (parentWidget()) {
            setAttribute(Qt::WA_StyledBackground, false);
            setGeometry(parentWidget()->rect());
            parentWidget()->installEventFilter(this);
        } else {
            setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            resize(800, 600);
        }
        setFocusPolicy(Qt::StrongFocus);
    }

    ~BaseConsoleModal() override {
        if (parentWidget()) {
            parentWidget()->removeEventFilter(this);
        }
        if (m_loop && m_loop->isRunning()) {
            m_loop->quit();
        }
    }

    static QWidget* ResolveParent(QWidget* parent) {
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
        for (auto* top : QApplication::topLevelWidgets()) {
            if (top->isVisible() && !top->inherits("QMenu")) {
                if (auto* mw = qobject_cast<QMainWindow*>(top)) {
                    if (mw->centralWidget()) {
                        return mw->centralWidget();
                    }
                }
                return top;
            }
        }
        return nullptr;
    }

    int Exec() {
        std::vector<QWidget*> hidden_overlays;
        if (parentWidget()) {
            for (auto* child : parentWidget()->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
                if (child != this && child->isVisible() &&
                    (child->inherits("QToolButton") || child->inherits("OverlayHomeButton") ||
                     child->inherits("OverlayStatusWidget"))) {
                    child->hide();
                    hidden_overlays.push_back(child);
                }
            }
        }

        show();
        raise();
        setFocus();
        AnimateIn();

        QEventLoop loop;
        m_loop = &loop;
        loop.exec();
        m_loop = nullptr;

        for (auto* w : hidden_overlays) {
            if (w) {
                w->show();
                w->raise();
            }
        }

        return m_result;
    }

    void Accept(int result = 0) {
        m_result = result;
        AnimateOutThenClose();
    }

    void Reject() {
        m_result = -1;
        AnimateOutThenClose();
    }

protected:
    /// Fades the card in and the backdrop up. The card lives inside a QVBoxLayout, so its
    /// position and size are owned by the layout; opacity is animated instead of geometry
    /// because a pos()/resize() tween would be undone on the next layout pass.
    void AnimateIn() {
        UiSounds::Play(UiSounds::Name::Notice);

        backdrop_progress = 0.0;
        StartBackdropAnimation(0.0, 1.0, PopInMs, QEasingCurve::OutCubic, {});

        if (!card_widget) {
            return;
        }
        auto* fade = new QGraphicsOpacityEffect(card_widget);
        fade->setOpacity(0.0);
        card_widget->setGraphicsEffect(fade);
        auto* card_anim = new QPropertyAnimation(fade, "opacity", this);
        card_anim->setDuration(PopInMs);
        card_anim->setStartValue(0.0);
        card_anim->setEndValue(1.0);
        card_anim->setEasingCurve(QEasingCurve::OutCubic);
        // Drop the effect once it is fully opaque: leaving a graphics effect installed forces
        // the card through an offscreen render target for the rest of its life.
        connect(card_anim, &QPropertyAnimation::finished, this, [this] {
            if (card_widget) {
                card_widget->setGraphicsEffect(nullptr);
            }
        });
        card_anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    /// Fades back out before releasing the event loop, so the panel visibly closes instead of
    /// vanishing between frames.
    void AnimateOutThenClose() {
        if (closing) {
            return;
        }
        closing = true;
        UiSounds::Play(UiSounds::Name::Back);

        if (card_widget) {
            auto* fade = new QGraphicsOpacityEffect(card_widget);
            card_widget->setGraphicsEffect(fade);
            auto* card_anim = new QPropertyAnimation(fade, "opacity", this);
            card_anim->setDuration(PopOutMs);
            card_anim->setStartValue(1.0);
            card_anim->setEndValue(0.0);
            card_anim->setEasingCurve(QEasingCurve::InCubic);
            card_anim->start(QAbstractAnimation::DeleteWhenStopped);
        }

        StartBackdropAnimation(backdrop_progress, 0.0, PopOutMs, QEasingCurve::InCubic, [this] {
            hide();
            if (m_loop && m_loop->isRunning()) {
                m_loop->quit();
            }
        });
    }

    /// Drives `backdrop_progress` without needing a Q_OBJECT/Q_PROPERTY on this class, which
    /// a .cpp-local type cannot have without also including its moc output.
    void StartBackdropAnimation(qreal from, qreal to, int duration, QEasingCurve::Type easing,
                                std::function<void()> on_finished) {
        auto* anim = new QVariantAnimation(this);
        anim->setDuration(duration);
        anim->setStartValue(from);
        anim->setEndValue(to);
        anim->setEasingCurve(easing);
        connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            backdrop_progress = value.toReal();
            update();
        });
        if (on_finished) {
            connect(anim, &QVariantAnimation::finished, this, std::move(on_finished));
        }
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            setGeometry(parentWidget()->rect());
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter p(this);
        QColor backdrop = backdrop_color;
        backdrop.setAlphaF(backdrop.alphaF() * qBound(0.0, backdrop_progress, 1.0));
        p.fillRect(rect(), backdrop);
    }


    void mousePressEvent(QMouseEvent* event) override {
        if (card_widget && !card_widget->geometry().contains(event->pos())) {
            if (dismiss_on_backdrop_click) {
                Reject();
                return;
            }
        }
        QWidget::mousePressEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            Reject();
            return;
        }
        QWidget::keyPressEvent(event);
    }

public:
    QWidget* card_widget = nullptr;
    QColor backdrop_color{0, 0, 0, 150};
    bool dismiss_on_backdrop_click = true;

private:
    int m_result = -1;
    QEventLoop* m_loop = nullptr;
    qreal backdrop_progress = 1.0;
    /// Guards against a second dismissal arriving during the close animation (an Escape press
    /// landing while the fade is already running would otherwise quit the loop twice).
    bool closing = false;
};

int ShowChoiceModal(QWidget* parent, const QString& title, const QString& subtitle,
                    const QStringList& items, int current_index) {
    const auto pal = GetThemePalette();
    auto* modal = new BaseConsoleModal(parent, true);
    modal->backdrop_color = pal.backdrop;

    auto* root_layout = new QVBoxLayout(modal);
    root_layout->setAlignment(Qt::AlignCenter);
    root_layout->setContentsMargins(20, 20, 20, 20);

    auto* card = new QWidget(modal);
    card->setObjectName(QStringLiteral("modalCard"));
    card->setFixedWidth(460);
    card->setMaximumHeight(580);
    card->setStyleSheet(QStringLiteral(
        "QWidget#modalCard {"
        "  background-color: %1;"
        "  border: %2;"
        "  border-radius: 0px;"
        "}").arg(pal.card_bg, pal.card_border));
    modal->card_widget = card;

    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(28, 24, 28, 20);
    card_layout->setSpacing(14);

    // Header Title & Subtitle
    auto* header_layout = new QVBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(6);

    auto* title_lbl = new QLabel(title, card);
    title_lbl->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 700; color: %1; background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
        .arg(pal.title_color));
    header_layout->addWidget(title_lbl);

    if (!subtitle.isEmpty()) {
        auto* sub_lbl = new QLabel(subtitle, card);
        sub_lbl->setWordWrap(true);
        sub_lbl->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: 400; color: %1;"
            " background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
            .arg(pal.subtitle_color));
        header_layout->addWidget(sub_lbl);
    }
    card_layout->addLayout(header_layout);

    // Horizontal divider
    auto* divider = new QFrame(card);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral(
        "background-color: %1; height: 1px; min-height: 1px; max-height: 1px; border: none;")
        .arg(pal.divider_color));
    card_layout->addWidget(divider);

    // Items List in a clean scroll area
    auto* scroll = new QScrollArea(card);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }")
        .arg(pal.scroll_bar_handle));

    auto* list_widget = new QWidget(scroll);
    list_widget->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto* list_layout = new QVBoxLayout(list_widget);
    list_layout->setContentsMargins(2, 4, 6, 4);
    list_layout->setSpacing(8);

    int selected_idx = (current_index >= 0 && current_index < items.size()) ? current_index : 0;
    std::vector<QPushButton*> buttons;
    std::vector<QLabel*> checkmarks;

    auto update_styles = [&](int sel) {
        selected_idx = sel;
        for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
            bool is_sel = (i == sel);
            checkmarks[i]->setVisible(is_sel);
            if (is_sel) {
                // Selection: Luminance Inversion
                buttons[i]->setStyleSheet(QStringLiteral(
                    "QPushButton {"
                    "  background-color: %1;"
                    "  color: %2;"
                    "  font-size: 15px;"
                    "  font-weight: 700;"
                    "  border-radius: 12px;"
                    "  padding: 0 16px;"
                    "  text-align: left;"
                    "  border: none;"
                    "}").arg(pal.item_sel_bg, pal.item_sel_text));
            } else {
                buttons[i]->setStyleSheet(QStringLiteral(
                    "QPushButton {"
                    "  background-color: %1;"
                    "  color: %2;"
                    "  font-size: 15px;"
                    "  font-weight: 500;"
                    "  border-radius: 12px;"
                    "  padding: 0 16px;"
                    "  text-align: left;"
                    "  border: none;"
                    "}"
                    "QPushButton:hover {"
                    "  background-color: %3;"
                    "}").arg(pal.item_bg, pal.item_text, pal.item_bg_hover));
            }
        }
    };

    for (int i = 0; i < items.size(); ++i) {
        auto* btn = new QPushButton(list_widget);
        btn->setFixedHeight(46);
        btn->setCursor(Qt::PointingHandCursor);

        auto* row_ly = new QHBoxLayout(btn);
        row_ly->setContentsMargins(14, 0, 14, 0);

        auto* lbl = new QLabel(items[i], btn);
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        lbl->setStyleSheet(QStringLiteral("background: transparent; border: none; font-size: 15px;"));
        row_ly->addWidget(lbl);
        row_ly->addStretch();

        auto* chk = new QLabel(QString::fromUtf8("✓"), btn);
        chk->setAttribute(Qt::WA_TransparentForMouseEvents);
        chk->setStyleSheet(QStringLiteral(
            "background: transparent; border: none; font-size: 16px; font-weight: 700; color: %1;")
            .arg(pal.item_sel_check));
        chk->setVisible(i == selected_idx);
        row_ly->addWidget(chk);

        buttons.push_back(btn);
        checkmarks.push_back(chk);
        list_layout->addWidget(btn);

        QObject::connect(btn, &QPushButton::clicked, [update_styles, i] {
            update_styles(i);
        });
    }

    update_styles(selected_idx);
    scroll->setWidget(list_widget);
    card_layout->addWidget(scroll, 1);

    // Footer with Docked [A] OK / [B] Back Legend
    auto* footer_layout = new QHBoxLayout();
    footer_layout->setContentsMargins(0, 10, 0, 0);
    footer_layout->setSpacing(14);
    footer_layout->addStretch();

    auto* btn_back = new QPushButton(card);
    btn_back->setText(EscapeButtonLabel(QObject::tr("Back")));
    btn_back->setIcon(MakeGlyphIcon(QStringLiteral("B"), QColor(0xE0, 0x3C, 0x3C)));
    btn_back->setIconSize(QSize(22, 22));
    btn_back->setCursor(Qt::PointingHandCursor);
    btn_back->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 8px 18px;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: %3; }")
        .arg(pal.back_btn_bg, pal.back_btn_text, pal.back_btn_hover));
    QObject::connect(btn_back, &QPushButton::clicked, modal, &BaseConsoleModal::Reject);
    footer_layout->addWidget(btn_back);

    auto* btn_ok = new QPushButton(card);
    btn_ok->setText(EscapeButtonLabel(QObject::tr("OK")));
    btn_ok->setIcon(MakeGlyphIcon(QStringLiteral("A"), QColor(0x3C, 0x8C, 0xE0)));
    btn_ok->setIconSize(QSize(22, 22));
    btn_ok->setCursor(Qt::PointingHandCursor);
    btn_ok->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 8px 22px;"
        "  font-size: 14px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover { background: %3; }")
        .arg(pal.primary_btn_bg, pal.primary_btn_text, pal.primary_btn_hover));
    QObject::connect(btn_ok, &QPushButton::clicked, [modal, &selected_idx] {
        modal->Accept(selected_idx);
    });
    footer_layout->addWidget(btn_ok);

    card_layout->addLayout(footer_layout);
    root_layout->addWidget(card, 0, Qt::AlignCenter);

    int ret = modal->Exec();
    modal->deleteLater();
    return ret;
}

std::optional<QString> ShowTextInputModal(QWidget* parent, const QString& title,
                                          const QString& subtitle, const QString& initial_text,
                                          int max_length) {
    const auto pal = GetThemePalette();
    auto* modal = new BaseConsoleModal(parent, true);
    modal->backdrop_color = pal.backdrop;

    auto* root_layout = new QVBoxLayout(modal);
    root_layout->setAlignment(Qt::AlignCenter);
    root_layout->setContentsMargins(20, 20, 20, 20);

    auto* card = new QWidget(modal);
    card->setObjectName(QStringLiteral("modalCard"));
    card->setFixedWidth(460);
    card->setStyleSheet(QStringLiteral(
        "QWidget#modalCard {"
        "  background-color: %1;"
        "  border: %2;"
        "  border-radius: 0px;"
        "}").arg(pal.card_bg, pal.card_border));
    modal->card_widget = card;

    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(28, 24, 28, 20);
    card_layout->setSpacing(14);

    auto* header_layout = new QVBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(6);

    auto* title_lbl = new QLabel(title, card);
    title_lbl->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 700; color: %1; background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
        .arg(pal.title_color));
    header_layout->addWidget(title_lbl);

    if (!subtitle.isEmpty()) {
        auto* sub_lbl = new QLabel(subtitle, card);
        sub_lbl->setWordWrap(true);
        sub_lbl->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: 400; color: %1;"
            " background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
            .arg(pal.subtitle_color));
        header_layout->addWidget(sub_lbl);
    }
    card_layout->addLayout(header_layout);

    // Input box
    auto* line_edit = new QLineEdit(initial_text, card);
    line_edit->setMaxLength(max_length);
    line_edit->setFixedHeight(50);
    line_edit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background-color: %1;"
        "  border: %2;"
        "  border-radius: 12px;"
        "  color: %3;"
        "  font-size: 17px;"
        "  font-weight: 600;"
        "  padding: 0 16px;"
        "}"
        "QLineEdit:focus {"
        "  border: 2px solid #3C8CE0;"
        "}").arg(pal.edit_bg, pal.edit_border, pal.edit_text));
    card_layout->addWidget(line_edit);

    // Character counter
    auto* counter_lbl = new QLabel(
        QStringLiteral("%1 / %2").arg(initial_text.length()).arg(max_length), card);
    counter_lbl->setAlignment(Qt::AlignRight);
    counter_lbl->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: %1; background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
        .arg(pal.edit_counter));
    card_layout->addWidget(counter_lbl);

    QObject::connect(line_edit, &QLineEdit::textChanged, [counter_lbl, max_length](const QString& text) {
        counter_lbl->setText(QStringLiteral("%1 / %2").arg(text.length()).arg(max_length));
    });

    QObject::connect(line_edit, &QLineEdit::returnPressed, [modal] {
        modal->Accept(0);
    });

    // Footer with Docked [A] OK / [B] Back Legend
    auto* footer_layout = new QHBoxLayout();
    footer_layout->setContentsMargins(0, 8, 0, 0);
    footer_layout->setSpacing(14);
    footer_layout->addStretch();

    auto* btn_back = new QPushButton(card);
    btn_back->setText(EscapeButtonLabel(QObject::tr("Back")));
    btn_back->setIcon(MakeGlyphIcon(QStringLiteral("B"), QColor(0xE0, 0x3C, 0x3C)));
    btn_back->setIconSize(QSize(22, 22));
    btn_back->setCursor(Qt::PointingHandCursor);
    btn_back->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 8px 18px;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: %3; }")
        .arg(pal.back_btn_bg, pal.back_btn_text, pal.back_btn_hover));
    QObject::connect(btn_back, &QPushButton::clicked, modal, &BaseConsoleModal::Reject);
    footer_layout->addWidget(btn_back);

    auto* btn_ok = new QPushButton(card);
    btn_ok->setText(EscapeButtonLabel(QObject::tr("OK")));
    btn_ok->setIcon(MakeGlyphIcon(QStringLiteral("A"), QColor(0x3C, 0x8C, 0xE0)));
    btn_ok->setIconSize(QSize(22, 22));
    btn_ok->setCursor(Qt::PointingHandCursor);
    btn_ok->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 8px 22px;"
        "  font-size: 14px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover { background: %3; }")
        .arg(pal.primary_btn_bg, pal.primary_btn_text, pal.primary_btn_hover));
    QObject::connect(btn_ok, &QPushButton::clicked, [modal] {
        modal->Accept(0);
    });
    footer_layout->addWidget(btn_ok);

    card_layout->addLayout(footer_layout);
    root_layout->addWidget(card, 0, Qt::AlignCenter);

    line_edit->selectAll();
    line_edit->setFocus();

    int ret = modal->Exec();
    QString result_text = line_edit->text();
    modal->deleteLater();

    if (ret == 0) {
        return result_text;
    }
    return std::nullopt;
}

std::optional<std::pair<int, int>> ShowDateModal(QWidget* parent, const QString& title,
                                                 const QString& subtitle, int initial_month,
                                                 int initial_day) {
    const auto pal = GetThemePalette();
    auto* modal = new BaseConsoleModal(parent, true);
    modal->backdrop_color = pal.backdrop;

    auto* root_layout = new QVBoxLayout(modal);
    root_layout->setAlignment(Qt::AlignCenter);
    root_layout->setContentsMargins(20, 20, 20, 20);

    auto* card = new QWidget(modal);
    card->setObjectName(QStringLiteral("modalCard"));
    card->setFixedWidth(460);
    card->setStyleSheet(QStringLiteral(
        "QWidget#modalCard {"
        "  background-color: %1;"
        "  border: %2;"
        "  border-radius: 0px;"
        "}").arg(pal.card_bg, pal.card_border));
    modal->card_widget = card;

    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(28, 24, 28, 20);
    card_layout->setSpacing(14);

    auto* header_layout = new QVBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(6);

    auto* title_lbl = new QLabel(title, card);
    title_lbl->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 700; color: %1; background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
        .arg(pal.title_color));
    header_layout->addWidget(title_lbl);

    if (!subtitle.isEmpty()) {
        auto* sub_lbl = new QLabel(subtitle, card);
        sub_lbl->setWordWrap(true);
        sub_lbl->setStyleSheet(QStringLiteral(
            "font-size: 13px; font-weight: 400; color: %1;"
            " background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
            .arg(pal.subtitle_color));
        header_layout->addWidget(sub_lbl);
    }
    card_layout->addLayout(header_layout);

    const QStringList month_names = {
        QObject::tr("January"),   QObject::tr("February"), QObject::tr("March"),
        QObject::tr("April"),     QObject::tr("May"),      QObject::tr("June"),
        QObject::tr("July"),      QObject::tr("August"),   QObject::tr("September"),
        QObject::tr("October"),   QObject::tr("November"), QObject::tr("December"),
    };

    int cur_month = std::clamp(initial_month, 1, 12);
    int cur_day = std::clamp(initial_day, 1, 31);

    auto get_max_days = [](int m) -> int {
        if (m == 2) return 29;
        if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
        return 31;
    };

    // Steppers container
    auto* steppers_widget = new QWidget(card);
    steppers_widget->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto* steppers_layout = new QHBoxLayout(steppers_widget);
    steppers_layout->setContentsMargins(0, 10, 0, 10);
    steppers_layout->setSpacing(20);

    // Month Stepper Box
    auto* month_box = new QWidget(steppers_widget);
    month_box->setStyleSheet(QStringLiteral(
        "background-color: %1; border-radius: 14px;").arg(pal.stepper_bg));
    auto* m_box_layout = new QVBoxLayout(month_box);
    m_box_layout->setContentsMargins(10, 12, 10, 12);
    m_box_layout->setSpacing(6);

    auto* m_up = new QPushButton(QString::fromUtf8("▲"), month_box);
    m_up->setCursor(Qt::PointingHandCursor);
    m_up->setFixedHeight(28);
    m_up->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1; font-size: 14px; border: none; }"
        "QPushButton:hover { color: #3C8CE0; }").arg(pal.stepper_text));

    auto* m_val = new QLabel(month_names.at(cur_month - 1), month_box);
    m_val->setAlignment(Qt::AlignCenter);
    m_val->setStyleSheet(QStringLiteral(
        "font-size: 17px; font-weight: 700; color: %1; background: transparent; border: none;")
        .arg(pal.stepper_text));

    auto* m_down = new QPushButton(QString::fromUtf8("▼"), month_box);
    m_down->setCursor(Qt::PointingHandCursor);
    m_down->setFixedHeight(28);
    m_down->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1; font-size: 14px; border: none; }"
        "QPushButton:hover { color: #3C8CE0; }").arg(pal.stepper_text));

    m_box_layout->addWidget(m_up);
    m_box_layout->addWidget(m_val);
    m_box_layout->addWidget(m_down);
    steppers_layout->addWidget(month_box, 3);

    // Day Stepper Box
    auto* day_box = new QWidget(steppers_widget);
    day_box->setStyleSheet(QStringLiteral(
        "background-color: %1; border-radius: 14px;").arg(pal.stepper_bg));
    auto* d_box_layout = new QVBoxLayout(day_box);
    d_box_layout->setContentsMargins(10, 12, 10, 12);
    d_box_layout->setSpacing(6);

    auto* d_up = new QPushButton(QString::fromUtf8("▲"), day_box);
    d_up->setCursor(Qt::PointingHandCursor);
    d_up->setFixedHeight(28);
    d_up->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1; font-size: 14px; border: none; }"
        "QPushButton:hover { color: #3C8CE0; }").arg(pal.stepper_text));

    auto* d_val = new QLabel(QString::number(cur_day), day_box);
    d_val->setAlignment(Qt::AlignCenter);
    d_val->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 700; color: %1; background: transparent; border: none;")
        .arg(pal.stepper_text));

    auto* d_down = new QPushButton(QString::fromUtf8("▼"), day_box);
    d_down->setCursor(Qt::PointingHandCursor);
    d_down->setFixedHeight(28);
    d_down->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1; font-size: 14px; border: none; }"
        "QPushButton:hover { color: #3C8CE0; }").arg(pal.stepper_text));

    d_box_layout->addWidget(d_up);
    d_box_layout->addWidget(d_val);
    d_box_layout->addWidget(d_down);
    steppers_layout->addWidget(day_box, 2);

    card_layout->addWidget(steppers_widget);

    auto update_date_display = [&] {
        int max_d = get_max_days(cur_month);
        if (cur_day > max_d) cur_day = max_d;
        m_val->setText(month_names.at(cur_month - 1));
        d_val->setText(QString::number(cur_day));
    };

    QObject::connect(m_up, &QPushButton::clicked, [&] {
        cur_month = (cur_month == 12) ? 1 : (cur_month + 1);
        update_date_display();
    });
    QObject::connect(m_down, &QPushButton::clicked, [&] {
        cur_month = (cur_month == 1) ? 12 : (cur_month - 1);
        update_date_display();
    });
    QObject::connect(d_up, &QPushButton::clicked, [&] {
        int max_d = get_max_days(cur_month);
        cur_day = (cur_day >= max_d) ? 1 : (cur_day + 1);
        update_date_display();
    });
    QObject::connect(d_down, &QPushButton::clicked, [&] {
        int max_d = get_max_days(cur_month);
        cur_day = (cur_day <= 1) ? max_d : (cur_day - 1);
        update_date_display();
    });

    // Footer with Docked [A] OK / [B] Back Legend
    auto* footer_layout = new QHBoxLayout();
    footer_layout->setContentsMargins(0, 10, 0, 0);
    footer_layout->setSpacing(14);
    footer_layout->addStretch();

    auto* btn_back = new QPushButton(card);
    btn_back->setText(EscapeButtonLabel(QObject::tr("Back")));
    btn_back->setIcon(MakeGlyphIcon(QStringLiteral("B"), QColor(0xE0, 0x3C, 0x3C)));
    btn_back->setIconSize(QSize(22, 22));
    btn_back->setCursor(Qt::PointingHandCursor);
    btn_back->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 8px 18px;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: %3; }")
        .arg(pal.back_btn_bg, pal.back_btn_text, pal.back_btn_hover));
    QObject::connect(btn_back, &QPushButton::clicked, modal, &BaseConsoleModal::Reject);
    footer_layout->addWidget(btn_back);

    auto* btn_ok = new QPushButton(card);
    btn_ok->setText(EscapeButtonLabel(QObject::tr("OK")));
    btn_ok->setIcon(MakeGlyphIcon(QStringLiteral("A"), QColor(0x3C, 0x8C, 0xE0)));
    btn_ok->setIconSize(QSize(22, 22));
    btn_ok->setCursor(Qt::PointingHandCursor);
    btn_ok->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 8px 22px;"
        "  font-size: 14px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover { background: %3; }")
        .arg(pal.primary_btn_bg, pal.primary_btn_text, pal.primary_btn_hover));
    QObject::connect(btn_ok, &QPushButton::clicked, [modal] {
        modal->Accept(0);
    });
    footer_layout->addWidget(btn_ok);

    card_layout->addLayout(footer_layout);
    root_layout->addWidget(card, 0, Qt::AlignCenter);

    int ret = modal->Exec();
    modal->deleteLater();

    if (ret == 0) {
        return std::make_pair(cur_month, cur_day);
    }
    return std::nullopt;
}

ActionChoice ShowConfirmModal(QWidget* parent, const QString& title, const QString& message,
                             const QString& primary_text, const QString& secondary_text,
                             const QString& cancel_text) {
    const auto pal = GetThemePalette();
    auto* modal = new BaseConsoleModal(parent, false);
    modal->backdrop_color = pal.backdrop;

    auto* root_layout = new QVBoxLayout(modal);
    root_layout->setAlignment(Qt::AlignCenter);
    root_layout->setContentsMargins(20, 20, 20, 20);

    auto* card = new QWidget(modal);
    card->setObjectName(QStringLiteral("modalCard"));
    card->setFixedWidth(460);
    card->setStyleSheet(QStringLiteral(
        "QWidget#modalCard {"
        "  background-color: %1;"
        "  border: %2;"
        "  border-radius: 0px;"
        "}").arg(pal.card_bg, pal.card_border));
    modal->card_widget = card;

    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(28, 24, 28, 22);
    card_layout->setSpacing(14);

    auto* header_layout = new QVBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(8);

    // Title (only shown if provided)
    if (!title.isEmpty()) {
        auto* title_lbl = new QLabel(title, card);
        title_lbl->setStyleSheet(QStringLiteral(
            "font-size: 20px; font-weight: 700; color: %1; background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
            .arg(pal.title_color));
        header_layout->addWidget(title_lbl);
    }

    // Message
    if (!message.isEmpty()) {
        auto* msg_lbl = new QLabel(message, card);
        msg_lbl->setWordWrap(true);
        msg_lbl->setStyleSheet(QStringLiteral(
            "font-size: 14px; font-weight: 400; color: %1; background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
            .arg(pal.subtitle_color));
        header_layout->addWidget(msg_lbl);
    }
    card_layout->addLayout(header_layout);

    // Divider
    auto* divider = new QFrame(card);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral(
        "background-color: %1; height: 1px; min-height: 1px; max-height: 1px; border: none;")
        .arg(pal.divider_color));
    card_layout->addWidget(divider);

    if (secondary_text.isEmpty()) {
        // Two buttons side-by-side in docked footer
        auto* footer = new QHBoxLayout();
        footer->setContentsMargins(0, 8, 0, 0);
        footer->setSpacing(12);
        footer->addStretch();

        QString c_text = cancel_text.isEmpty() ? QObject::tr("Cancel") : cancel_text;
        auto* btn_cancel = new QPushButton(card);
        btn_cancel->setText(EscapeButtonLabel(c_text));
        btn_cancel->setIcon(MakeGlyphIcon(QStringLiteral("B"), QColor(0xE0, 0x3C, 0x3C)));
        btn_cancel->setIconSize(QSize(22, 22));
        btn_cancel->setCursor(Qt::PointingHandCursor);
        btn_cancel->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  color: %2;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 8px 18px;"
            "  font-size: 14px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:hover { background: %3; }")
            .arg(pal.back_btn_bg, pal.back_btn_text, pal.back_btn_hover));
        QObject::connect(btn_cancel, &QPushButton::clicked, modal, &BaseConsoleModal::Reject);
        footer->addWidget(btn_cancel);

        auto* btn_primary = new QPushButton(card);
        btn_primary->setText(EscapeButtonLabel(primary_text));
        btn_primary->setIcon(MakeGlyphIcon(QStringLiteral("A"), QColor(0x3C, 0x8C, 0xE0)));
        btn_primary->setIconSize(QSize(22, 22));
        btn_primary->setCursor(Qt::PointingHandCursor);
        btn_primary->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  color: %2;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 8px 22px;"
            "  font-size: 14px;"
            "  font-weight: 700;"
            "}"
            "QPushButton:hover { background: %3; }")
            .arg(pal.primary_btn_bg, pal.primary_btn_text, pal.primary_btn_hover));
        QObject::connect(btn_primary, &QPushButton::clicked, [modal] {
            modal->Accept(0);
        });
        footer->addWidget(btn_primary);

        card_layout->addLayout(footer);
        btn_primary->setFocus();
    } else {
        // Vertical stacked console action buttons (Primary, Secondary, Cancel)
        auto* actions_layout = new QVBoxLayout();
        actions_layout->setContentsMargins(0, 6, 0, 0);
        actions_layout->setSpacing(10);

        auto* btn_primary = new QPushButton(card);
        btn_primary->setFixedHeight(46);
        btn_primary->setCursor(Qt::PointingHandCursor);
        btn_primary->setText(EscapeButtonLabel(primary_text));
        btn_primary->setIcon(MakeGlyphIcon(QStringLiteral("A"), QColor(0x3C, 0x8C, 0xE0)));
        btn_primary->setIconSize(QSize(22, 22));
        btn_primary->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  color: %2;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 0 18px;"
            "  font-size: 15px;"
            "  font-weight: 700;"
            "}"
            "QPushButton:hover { background: %3; }")
            .arg(pal.primary_btn_bg, pal.primary_btn_text, pal.primary_btn_hover));
        QObject::connect(btn_primary, &QPushButton::clicked, [modal] {
            modal->Accept(0);
        });
        actions_layout->addWidget(btn_primary);

        auto* btn_secondary = new QPushButton(card);
        btn_secondary->setFixedHeight(46);
        btn_secondary->setCursor(Qt::PointingHandCursor);
        btn_secondary->setText(EscapeButtonLabel(secondary_text));
        btn_secondary->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  color: %2;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 0 18px;"
            "  font-size: 15px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:hover { background: %3; }")
            .arg(pal.secondary_btn_bg, pal.secondary_btn_text, pal.secondary_btn_hover));
        QObject::connect(btn_secondary, &QPushButton::clicked, [modal] {
            modal->Accept(1);
        });
        actions_layout->addWidget(btn_secondary);

        QString c_text = cancel_text.isEmpty() ? QObject::tr("Cancel") : cancel_text;
        auto* btn_cancel = new QPushButton(card);
        btn_cancel->setFixedHeight(42);
        btn_cancel->setCursor(Qt::PointingHandCursor);
        btn_cancel->setText(EscapeButtonLabel(c_text));
        btn_cancel->setIcon(MakeGlyphIcon(QStringLiteral("B"), QColor(0xE0, 0x3C, 0x3C)));
        btn_cancel->setIconSize(QSize(20, 20));
        btn_cancel->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  color: %2;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 0 18px;"
            "  font-size: 14px;"
            "  font-weight: 600;"
            "}"
            "QPushButton:hover { background: %3; }")
            .arg(pal.back_btn_bg, pal.back_btn_text, pal.back_btn_hover));
        QObject::connect(btn_cancel, &QPushButton::clicked, modal, &BaseConsoleModal::Reject);
        actions_layout->addWidget(btn_cancel);

        card_layout->addLayout(actions_layout);
        btn_primary->setFocus();
    }

    root_layout->addWidget(card, 0, Qt::AlignCenter);

    int ret = modal->Exec();
    modal->deleteLater();

    if (ret == 0) {
        return ActionChoice::Primary;
    } else if (ret == 1) {
        return ActionChoice::Secondary;
    }
    return ActionChoice::Cancel;
}

void ShowAlertModal(QWidget* parent, const QString& title, const QString& message,
                    AlertType type) {
    const auto pal = GetThemePalette();
    auto* modal = new BaseConsoleModal(parent, true);
    modal->backdrop_color = pal.backdrop;

    auto* root_layout = new QVBoxLayout(modal);
    root_layout->setAlignment(Qt::AlignCenter);
    root_layout->setContentsMargins(20, 20, 20, 20);

    auto* card = new QWidget(modal);
    card->setObjectName(QStringLiteral("modalCard"));
    card->setFixedWidth(440);
    card->setStyleSheet(QStringLiteral(
        "QWidget#modalCard {"
        "  background-color: %1;"
        "  border: %2;"
        "  border-radius: 0px;"
        "}").arg(pal.card_bg, pal.card_border));
    modal->card_widget = card;

    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(28, 24, 28, 22);
    card_layout->setSpacing(14);

    auto* header_layout = new QVBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(8);

    // Title with severity indicator if needed
    auto* title_lbl = new QLabel(title, card);
    QString title_color = pal.title_color;
    if (type == AlertType::Error) {
        title_color = QStringLiteral("#E03C3C");
    } else if (type == AlertType::Warning) {
        title_color = QStringLiteral("#E08C3C");
    }
    title_lbl->setStyleSheet(QStringLiteral(
        "font-size: 20px; font-weight: 700; color: %1; background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
        .arg(title_color));
    header_layout->addWidget(title_lbl);

    if (!message.isEmpty()) {
        auto* msg_lbl = new QLabel(message, card);
        msg_lbl->setWordWrap(true);
        msg_lbl->setStyleSheet(QStringLiteral(
            "font-size: 14px; font-weight: 400; color: %1; background: transparent; border: none; padding: 2px 0 0 0; margin: 0;")
            .arg(pal.subtitle_color));
        header_layout->addWidget(msg_lbl);
    }
    card_layout->addLayout(header_layout);

    auto* divider = new QFrame(card);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral(
        "background-color: %1; height: 1px; min-height: 1px; max-height: 1px; border: none;")
        .arg(pal.divider_color));
    card_layout->addWidget(divider);

    auto* footer = new QHBoxLayout();
    footer->setContentsMargins(0, 8, 0, 0);
    footer->addStretch();

    auto* btn_ok = new QPushButton(card);
    btn_ok->setText(EscapeButtonLabel(QObject::tr("OK")));
    btn_ok->setIcon(MakeGlyphIcon(QStringLiteral("A"), QColor(0x3C, 0x8C, 0xE0)));
    btn_ok->setIconSize(QSize(22, 22));
    btn_ok->setCursor(Qt::PointingHandCursor);
    btn_ok->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 8px 24px;"
        "  font-size: 14px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover { background: %3; }")
        .arg(pal.primary_btn_bg, pal.primary_btn_text, pal.primary_btn_hover));
    QObject::connect(btn_ok, &QPushButton::clicked, [modal] {
        modal->Accept(0);
    });
    footer->addWidget(btn_ok);

    card_layout->addLayout(footer);
    root_layout->addWidget(card, 0, Qt::AlignCenter);
    btn_ok->setFocus();

    modal->Exec();
    modal->deleteLater();
}

} // namespace ConsoleUI
