// Copyright 2026 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <QString>
#include <vector>
#include "core/hle/mii.h"

namespace MiiMaker {

class MiiManager {
public:
    static Mii::MiiData CreateDefaultMii(const QString& name = QStringLiteral("Hero"));
    static Mii::MiiData CreateRandomMii(const QString& name = QStringLiteral("Hero"));

    static QString GetMiiName(const Mii::MiiData& mii);
    static void SetMiiName(Mii::MiiData& mii, const QString& name);

    static std::vector<Mii::MiiData> LoadMiisFromNAND();
    static bool SaveMiiToNAND(const Mii::MiiData& mii, int slot_index = -1);

    static bool ExportMiiAvatar(const Mii::MiiData& mii, const QString& destination_path);
};

} // namespace MiiMaker
