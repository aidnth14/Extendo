// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <QHash>
#include <QString>

class QSoundEffect;

/// Short interface sound effects (hover, select, back, drag...) for the console-style UI.
///
/// Effects are loose .wav files in the app's ui_sounds folder rather than a compiled-in Qt
/// resource, matching how home screen icons are shipped, so the sound set can be swapped
/// without a rebuild.
///
/// QSoundEffect is used rather than QMediaPlayer because it keeps decoded PCM in memory and
/// starts with far less latency, which is what makes a click feel attached to the press.
namespace UiSounds {

/// Canonical effect names. These match the .wav file stems, minus the extension.
namespace Name {
inline constexpr char Hover[] = "hover";
inline constexpr char Select[] = "select";
inline constexpr char SelectPart[] = "select_part";
inline constexpr char SelectTab[] = "select_tab";
inline constexpr char SelectMisc[] = "select_misc";
inline constexpr char Back[] = "back";
inline constexpr char NoClick[] = "no_click";
inline constexpr char Notice[] = "notice";
inline constexpr char PickUp[] = "position_up";
inline constexpr char PutDown[] = "position_down";
inline constexpr char MoveApart[] = "move_apart";
inline constexpr char Sparkle[] = "sparkle";
inline constexpr char Finish[] = "finish";
} // namespace Name

/// Widget property naming a non-default press effect, e.g.
/// `button->setProperty(PressSoundProperty, UiSounds::Name::Finish)`. Lets a single button opt
/// out of the generic click without needing its own connect.
inline constexpr char PressSoundProperty[] = "uiSoundPress";

/// Widget property set to true to silence a control entirely.
inline constexpr char SilentProperty[] = "uiSoundSilent";

/// Plays an effect by name. Cheap to call: the effect is decoded once and cached. Does nothing
/// when UI sounds are disabled or the named file is missing, so callers never need to check.
void Play(const char* name);

/// Installs an application-wide filter giving every QAbstractButton hover and press feedback,
/// including buttons inside dialogs generated from .ui files.
///
/// One filter on the application is used rather than per-button wiring because this UI has
/// hundreds of buttons across generated dialogs: hooking them individually guarantees some get
/// missed, and that any button added later is silent.
void InstallGlobalFilter();

/// Re-reads the enable flag and volume from UISettings and applies them to loaded effects.
void ApplySettings();

/// Decodes the handful of effects used on nearly every interaction. QSoundEffect loads its
/// source asynchronously, so without this the very first hover or click is silent.
void Preload();

} // namespace UiSounds
