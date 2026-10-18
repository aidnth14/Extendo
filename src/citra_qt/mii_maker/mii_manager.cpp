// Copyright 2026 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include "citra_qt/mii_maker/mii_manager.h"
#include "citra_qt/mii_maker/mii_render.h"

#include <random>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include "common/file_util.h"
#include "common/string_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/frontend/applets/mii_selector.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/service/ptm/ptm.h"

namespace MiiMaker {

QString MiiManager::GetMiiName(const Mii::MiiData& mii) {
    return QString::fromStdString(Common::UTF16BufferToUTF8(mii.mii_name)).trimmed();
}

void MiiManager::SetMiiName(Mii::MiiData& mii, const QString& name) {
    std::u16string u16 = name.trimmed().left(10).toStdU16String();
    mii.mii_name.fill(0);
    for (std::size_t i = 0; i < u16.size() && i < mii.mii_name.size(); ++i) {
        mii.mii_name[i] = u16[i];
    }
}

Mii::MiiData MiiManager::CreateDefaultMii(const QString& name) {
    auto res = HLE::Applets::MiiSelector::GetStandardMiiResult();
    Mii::MiiData mii = res.selected_mii_data;
    SetMiiName(mii, name.isEmpty() ? QStringLiteral("Hero") : name);
    return mii;
}

Mii::MiiData MiiManager::CreateRandomMii(const QString& name) {
    Mii::MiiData mii = CreateDefaultMii(name);

    static std::mt19937 rng(static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch()));
    auto rand_range = [](int min_v, int max_v) -> int {
        std::uniform_int_distribution<int> dist(min_v, max_v);
        return dist(rng);
    };

    mii.mii_id = static_cast<u32>(rand_range(0x10000000, 0x7FFFFFFF));
    mii.mii_details.gender.Assign(rand_range(0, 1));
    mii.mii_details.favorite_color.Assign(rand_range(0, 11));
    mii.face_style.skin_color.Assign(rand_range(0, 5));
    mii.hair_style = static_cast<u8>(rand_range(0, 7));
    mii.hair_details.color.Assign(rand_range(0, 7));
    mii.hair_details.flip.Assign(rand_range(0, 1));

    mii.eye_details.type.Assign(rand_range(0, 5));
    mii.eye_details.color.Assign(rand_range(0, 5));
    mii.eye_details.scale.Assign(rand_range(2, 6));
    mii.eye_details.rotate.Assign(rand_range(2, 4));
    mii.eye_details.x.Assign(rand_range(1, 4));
    mii.eye_details.y.Assign(rand_range(10, 14));

    mii.eyebrow_details.style.Assign(rand_range(0, 8));
    mii.eyebrow_details.color.Assign(static_cast<u32>(mii.hair_details.color.Value()));
    mii.eyebrow_details.scale.Assign(rand_range(2, 5));
    mii.eyebrow_details.rotate.Assign(rand_range(2, 5));
    mii.eyebrow_details.x.Assign(rand_range(1, 4));
    mii.eyebrow_details.y.Assign(rand_range(8, 12));

    mii.nose_details.type.Assign(rand_range(0, 4));
    mii.nose_details.y.Assign(rand_range(6, 10));

    mii.mouth_details.type.Assign(rand_range(0, 5));
    mii.mouth_details.scale.Assign(rand_range(2, 5));
    mii.mustache_details.mouth_y.Assign(rand_range(11, 15));

    // Low chance for facial accessories
    mii.glasses_details.type.Assign(rand_range(0, 10) == 0 ? rand_range(1, 2) : 0);
    mii.mole_details.type.Assign(rand_range(0, 8) == 0 ? 1 : 0);

    return mii;
}

std::vector<Mii::MiiData> MiiManager::LoadMiisFromNAND() {
    std::vector<Mii::MiiData> miis = Frontend::LoadMiis();
    if (miis.empty()) {
        miis.push_back(CreateDefaultMii(QStringLiteral("Citra")));
    }
    return miis;
}

bool MiiManager::SaveMiiToNAND(const Mii::MiiData& mii, int slot_index) {
    std::string nand_directory{FileUtil::GetUserPath(FileUtil::UserPath::NANDDir)};
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(nand_directory,
                                                                FileSys::ExtSaveDataType::Shared);

    auto archive_result = extdata_archive_factory.Open(Service::PTM::ptm_shared_extdata_id, 0);
    if (!archive_result.Succeeded()) {
        return false;
    }

    auto archive = std::move(archive_result).Unwrap();
    FileSys::Path file_path = "/CFL_DB.dat";
    FileSys::Mode mode{};
    mode.read_flag.Assign(1);
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    auto file_result = archive->OpenFile(file_path, mode);
    if (!file_result.Succeeded()) {
        return false;
    }

    auto file = std::move(file_result).Unwrap();

    // Check header
    std::array<u8, 8> header{};
    file->Read(0, 8, header.data());
    if (header[0] != 'C' || header[1] != 'F' || header[2] != 'O' || header[3] != 'G') {
        const u8 default_header[8] = {'C', 'F', 'O', 'G', 0x00, 0x01, 0x00, 0x00};
        file->Write(0, 8, true, false, default_header);
    }

    int target_slot = slot_index;
    if (target_slot < 0 || target_slot >= 100) {
        // Find existing Mii with same ID or first free slot
        int free_slot = -1;
        u32 cur_offset = 0x8;
        for (int i = 0; i < 100; ++i) {
            Mii::MiiData existing{};
            file->Read(cur_offset, sizeof(existing), reinterpret_cast<u8*>(&existing));
            if (existing.mii_id == mii.mii_id && mii.mii_id != 0) {
                target_slot = i;
                break;
            }
            if (existing.mii_id == 0 && free_slot < 0) {
                free_slot = i;
            }
            cur_offset += sizeof(existing);
        }
        if (target_slot < 0) {
            target_slot = (free_slot >= 0) ? free_slot : 0;
        }
    }

    u32 write_offset = 0x8 + target_slot * static_cast<u32>(sizeof(Mii::MiiData));
    file->Write(write_offset, sizeof(Mii::MiiData), true, false,
                reinterpret_cast<const u8*>(&mii));
    return true;
}

bool MiiManager::ExportMiiAvatar(const Mii::MiiData& mii, const QString& destination_path) {
    QPixmap pix = RenderMii(mii, 288, 280, QStringLiteral("normal"), 0);
    QFileInfo fi(destination_path);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return pix.save(destination_path, "PNG");
}

} // namespace MiiMaker
