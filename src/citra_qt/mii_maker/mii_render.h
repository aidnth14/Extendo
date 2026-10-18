// Copyright 2026 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>
#include <vector>
#include "core/hle/mii.h"

namespace MiiMaker {

struct ColorEntry {
    const char* name;
    const char* hex;
};

extern const std::vector<ColorEntry> SHIRT_COLORS;
extern const std::vector<ColorEntry> SKIN_COLORS;
extern const std::vector<ColorEntry> HAIR_COLORS;
extern const std::vector<ColorEntry> EYE_COLORS;
extern const std::vector<ColorEntry> GLASSES_COLORS;

/**
 * Procedural cel-shaded chibi avatar renderer.
 * 100% offline, 0ms local vector rendering via QPainter.
 *
 * @param mii The 3DS MiiData bitfield structure.
 * @param width Output pixmap width.
 * @param height Output pixmap height.
 * @param expression Expression string: "normal", "smile", "wink_left", "surprise", "anger", "smug".
 * @param rotation_y Head rotation Y in degrees (-60 to +60).
 * @return High-resolution anti-aliased cel-shaded character pixmap.
 */
QPixmap RenderMii(const Mii::MiiData& mii, int width = 288, int height = 280,
                  const QString& expression = QStringLiteral("normal"), int rotation_y = 0);

} // namespace MiiMaker
