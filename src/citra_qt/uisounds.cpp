// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <QAbstractButton>
#include <QByteArray>
#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QHash>
#include <QSoundEffect>
#include <QUrl>
#include <QVariant>
#include "citra_qt/uisounds.h"
#include "citra_qt/uisettings.h"
#include "common/logging/log.h"

namespace UiSounds {
namespace {

/// Loose .wav files beside the other swappable UI assets.
QString SoundPath(const char* name) {
    const QString app_dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MAC
    const QString dir = app_dir + QStringLiteral("/../Resources/ui_sounds/");
#else
    const QString dir = app_dir + QStringLiteral("/ui_sounds/");
#endif
    return dir + QLatin1String(name) + QStringLiteral(".wav");
}

qreal CurrentVolume() {
    // Stored 0-100 for a sane config file; QSoundEffect wants 0.0-1.0.
    return qBound(0, static_cast<int>(UISettings::values.ui_sounds_volume.GetValue()), 100) /
           100.0;
}

QHash<QString, QSoundEffect*>& Cache() {
    static QHash<QString, QSoundEffect*> cache;
    return cache;
}

/// Returns a ready effect for `name`, decoding it on first use. Null if the file is absent.
QSoundEffect* Acquire(const char* name) {
    const QString key = QLatin1String(name);
    auto& cache = Cache();
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    const QString path = SoundPath(name);
    if (!QFileInfo::exists(path)) {
        // Cache the miss so a missing file isn't stat-ed on every hover.
        cache.insert(key, nullptr);
        LOG_WARNING(Frontend, "UI sound {} not found at {}", name, path.toStdString());
        return nullptr;
    }

    // Parented to the application so the effects outlive individual widgets and are torn
    // down cleanly at exit.
    auto* effect = new QSoundEffect(QCoreApplication::instance());
    effect->setSource(QUrl::fromLocalFile(path));
    effect->setVolume(CurrentVolume());
    cache.insert(key, effect);
    return effect;
}

/// Application-wide hover/press feedback for buttons. See InstallGlobalFilter().
class GlobalButtonSounds : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        const QEvent::Type type = event->type();
        if (type != QEvent::Enter && type != QEvent::MouseButtonPress) {
            return QObject::eventFilter(watched, event);
        }

        auto* button = qobject_cast<QAbstractButton*>(watched);
        if (!button || !button->isEnabled() || button->property(SilentProperty).toBool()) {
            return QObject::eventFilter(watched, event);
        }

        if (type == QEvent::Enter) {
            Play(Name::Hover);
        } else {
            // A button may name its own press effect; everything else gets the generic click.
            const QByteArray override_name = button->property(PressSoundProperty).toByteArray();
            Play(override_name.isEmpty() ? Name::SelectTab : override_name.constData());
        }

        // Never consume the event -- the button still has to actually activate.
        return QObject::eventFilter(watched, event);
    }
};

} // Anonymous namespace

void InstallGlobalFilter() {
    if (auto* app = QCoreApplication::instance()) {
        app->installEventFilter(new GlobalButtonSounds(app));
    }
}

void Play(const char* name) {
    if (!UISettings::values.ui_sounds_enabled.GetValue()) {
        return;
    }
    QSoundEffect* effect = Acquire(name);
    if (!effect) {
        return;
    }
    // Restart from the beginning rather than overlapping copies of the same click, which is
    // what makes rapid cursor movement across tiles sound like a rattle.
    effect->stop();
    effect->play();
}

void ApplySettings() {
    const qreal volume = CurrentVolume();
    for (QSoundEffect* effect : Cache()) {
        if (effect) {
            effect->setVolume(volume);
        }
    }
}

void Preload() {
    if (!UISettings::values.ui_sounds_enabled.GetValue()) {
        return;
    }
    for (const char* name : {Name::Hover, Name::Select, Name::Back, Name::SelectPart,
                             Name::PickUp, Name::PutDown}) {
        Acquire(name);
    }
}

} // namespace UiSounds
