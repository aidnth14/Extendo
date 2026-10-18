// Copyright 2026 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include "citra_qt/mii_maker/mii_render.h"

#include <algorithm>
#include <cmath>
#include <QBrush>
#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QRadialGradient>
#include <QRectF>

namespace MiiMaker {

// Sampled from imported garment textures
// (tex_bottom_ae_*/tex_hand_ae_*/tex_feet_ae_*), not a designed ramp.
const std::vector<ColorEntry> SHIRT_COLORS = {
    {"Wine", "#580E1F"},
    {"Umber", "#875E2C"},
    {"Plum", "#392031"},
    {"Indigo", "#332B4C"},
    {"Rust", "#7D332B"},
    {"Bronze", "#683605"},
    {"Espresso", "#3D1C0B"},
    {"Navy", "#272F71"},
    {"Mauve", "#503750"},
    {"Clay", "#724941"},
    {"Cocoa", "#371606"},
    {"Amber", "#C48248"},
};

// Sampled from the imported skin texture atlas.
const std::vector<ColorEntry> SKIN_COLORS = {
    {"Peach", "#FCA180"},
    {"Coral", "#FF9E7D"},
    {"Blush", "#FFA483"},
    {"Ivory", "#FFE0B7"},
    {"Sand", "#FFBF9E"},
    {"Honey", "#FDDCB3"},
};

const std::vector<ColorEntry> HAIR_COLORS = {
    {"Black", "#222222"},
    {"Dark Brown", "#3D2416"},
    {"Brown", "#583622"},
    {"Light Brown", "#7E4D30"},
    {"Platinum", "#C8BDB0"},
    {"Chestnut", "#844732"},
    {"Blonde", "#C6A355"},
    {"Grey", "#8E8E93"},
};

const std::vector<ColorEntry> EYE_COLORS = {
    {"Black", "#1A1A1A"},
    {"Brown", "#53331E"},
    {"Amber", "#8E3D22"},
    {"Blue", "#2B6396"},
    {"Green", "#3B7A44"},
    {"Hazel", "#616E61"},
};

const std::vector<ColorEntry> GLASSES_COLORS = {
    {"Black", "#2B2B2B"},
    {"Brown", "#583622"},
    {"Red", "#C62828"},
    {"Blue", "#1565C0"},
    {"Yellow", "#EAA816"},
    {"Green", "#2E7D32"},
};

QPixmap RenderMii(const Mii::MiiData& mii, int width, int height,
                  const QString& expression, int rotation_y) {
    QPixmap pix(width, height);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const double cx = width / 2.0;
    const double cy = 130.0;

    // Head rotation / tilt simulation
    const double angle_rad = (std::clamp(rotation_y, -50, 50) / 180.0) * 0.40;
    const double dx = std::sin(angle_rad) * 16.0;

    const int shirt_idx = std::clamp(static_cast<int>(mii.mii_details.favorite_color.Value()), 0, 11);
    const int skin_idx = std::clamp(static_cast<int>(mii.face_style.skin_color.Value()), 0, 5);
    const int hair_color_idx = std::clamp(static_cast<int>(mii.hair_details.color.Value()), 0, 7);
    const int brow_color_idx = std::clamp(static_cast<int>(mii.eyebrow_details.color.Value()), 0, 7);
    const int eye_color_idx = std::clamp(static_cast<int>(mii.eye_details.color.Value()), 0, 5);
    const int glasses_color_idx = std::clamp(static_cast<int>(mii.glasses_details.color.Value()), 0, 5);

    const QColor base_shirt(SHIRT_COLORS[shirt_idx].hex);
    const QColor base_skin(SKIN_COLORS[skin_idx].hex);
    const QColor base_hair(HAIR_COLORS[hair_color_idx].hex);
    const QColor brow_color(HAIR_COLORS[brow_color_idx].hex);
    const QColor base_eye(EYE_COLORS[eye_color_idx].hex);
    const QColor glasses_color(GLASSES_COLORS[glasses_color_idx].hex);

    const QColor dark_skin = base_skin.darker(115);
    const QColor shadow_skin = base_skin.darker(130);

    const double hx = cx + dx * 0.5;
    const double flip_mul = (mii.hair_details.flip.Value() == 1) ? -1.0 : 1.0;
    const int hs = mii.hair_style % 12;

    // 1. Torso / Shirt (Classic avatar shirt)
    QLinearGradient torso_grad(cx - 80, cy + 90, cx + 80, cy + 155);
    torso_grad.setColorAt(0.0, base_shirt.lighter(112));
    torso_grad.setColorAt(1.0, base_shirt.darker(120));
    painter.setPen(Qt::NoPen);
    painter.setBrush(torso_grad);

    QPainterPath torso_path;
    torso_path.moveTo(cx - 36, cy + 92);
    torso_path.quadTo(cx - 90, cy + 104, cx - 110, height);
    torso_path.lineTo(cx + 110, height);
    torso_path.quadTo(cx + 90, cy + 104, cx + 36, cy + 92);
    torso_path.closeSubpath();
    painter.drawPath(torso_path);

    // Torso collar line
    painter.setPen(QPen(base_shirt.darker(145), 2.5));
    painter.drawLine(QPointF(cx - 32, cy + 92), QPointF(cx, cy + 98));
    painter.drawLine(QPointF(cx, cy + 98), QPointF(cx + 32, cy + 92));

    // 2. Neck
    QLinearGradient neck_grad(cx - 18, cy + 60, cx + 18, cy + 95);
    neck_grad.setColorAt(0.0, shadow_skin);
    neck_grad.setColorAt(0.6, base_skin);
    neck_grad.setColorAt(1.0, dark_skin);
    painter.setPen(Qt::NoPen);
    painter.setBrush(neck_grad);
    painter.drawRoundedRect(QRectF(cx - 17, cy + 62, 34, 34), 8, 8);

    // 3. Back Hair (For styles that extend below/behind head)
    if (hs == 5 || hs == 6 || hs == 7 || hs == 8) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(base_hair.darker(115));
        QPainterPath back_hair;
        if (hs == 7) { // Long straight hair
            back_hair.addRoundedRect(QRectF(hx - 68, cy - 10, 136, 115), 30, 30);
        } else if (hs == 6) { // Ponytail
            back_hair.addEllipse(QPointF(hx, cy + 78), 24, 34);
        } else if (hs == 5) { // Bob cut
            back_hair.addRoundedRect(QRectF(hx - 70, cy - 10, 140, 88), 26, 26);
        } else { // Wavy hair
            back_hair.addRoundedRect(QRectF(hx - 72, cy - 8, 144, 96), 32, 32);
        }
        painter.drawPath(back_hair);
    }

    // 4. Ears
    QRadialGradient ear_grad_l(hx - 62, cy + 16, 16);
    ear_grad_l.setColorAt(0.0, base_skin.lighter(106));
    ear_grad_l.setColorAt(1.0, dark_skin);
    painter.setBrush(ear_grad_l);
    painter.setPen(QPen(shadow_skin, 1.2));
    painter.drawEllipse(QRectF(hx - 72, cy + 4, 20, 26));

    QRadialGradient ear_grad_r(hx + 62, cy + 16, 16);
    ear_grad_r.setColorAt(0.0, base_skin);
    ear_grad_r.setColorAt(1.0, dark_skin);
    painter.setBrush(ear_grad_r);
    painter.drawEllipse(QRectF(hx + 52, cy + 4, 20, 26));

    // Inner ear creases
    painter.setPen(QPen(shadow_skin, 1.8));
    painter.drawArc(QRectF(hx - 68, cy + 11, 11, 13), 60 * 16, 180 * 16);
    painter.drawArc(QRectF(hx + 57, cy + 11, 11, 13), -60 * 16, -180 * 16);

    // 5. Head / Face (Classic 3D shaded head)
    QRadialGradient head_grad(hx - 15, cy - 10, 85);
    head_grad.setColorAt(0.0, base_skin.lighter(106));
    head_grad.setColorAt(0.7, base_skin);
    head_grad.setColorAt(1.0, dark_skin);

    painter.setPen(QPen(shadow_skin, 1.4));
    painter.setBrush(head_grad);

    QPainterPath face_path;
    const int ft = mii.face_style.type.Value() % 8;
    if (ft == 0) { // Classic round
        face_path.addRoundedRect(QRectF(hx - 58, cy - 64, 116, 136), 56, 64);
    } else if (ft == 1) { // Pointed chin
        face_path.moveTo(hx - 58, cy - 20);
        face_path.quadTo(hx - 58, cy - 64, hx, cy - 64);
        face_path.quadTo(hx + 58, cy - 64, hx + 58, cy - 20);
        face_path.quadTo(hx + 56, cy + 40, hx, cy + 74);
        face_path.quadTo(hx - 56, cy + 40, hx - 58, cy - 20);
        face_path.closeSubpath();
    } else if (ft == 2) { // Square / flat jaw
        face_path.moveTo(hx - 58, cy - 20);
        face_path.quadTo(hx - 58, cy - 64, hx, cy - 64);
        face_path.quadTo(hx + 58, cy - 64, hx + 58, cy - 20);
        face_path.quadTo(hx + 58, cy + 50, hx + 42, cy + 68);
        face_path.lineTo(hx - 42, cy + 68);
        face_path.quadTo(hx - 58, cy + 50, hx - 58, cy - 20);
        face_path.closeSubpath();
    } else if (ft == 3) { // Chiseled wide
        face_path.addRoundedRect(QRectF(hx - 62, cy - 64, 124, 138), 50, 58);
    } else if (ft == 4) { // Elongated oval
        face_path.addRoundedRect(QRectF(hx - 54, cy - 68, 108, 144), 52, 68);
    } else if (ft == 5) { // Chubby cheeks
        face_path.moveTo(hx - 56, cy - 20);
        face_path.quadTo(hx - 56, cy - 64, hx, cy - 64);
        face_path.quadTo(hx + 56, cy - 64, hx + 56, cy - 20);
        face_path.quadTo(hx + 64, cy + 30, hx + 38, cy + 70);
        face_path.quadTo(hx, cy + 74, hx - 38, cy + 70);
        face_path.quadTo(hx - 64, cy + 30, hx - 56, cy - 20);
        face_path.closeSubpath();
    } else if (ft == 6) { // Heart
        face_path.moveTo(hx - 60, cy - 20);
        face_path.quadTo(hx - 60, cy - 64, hx, cy - 64);
        face_path.quadTo(hx + 60, cy - 64, hx + 60, cy - 20);
        face_path.quadTo(hx + 52, cy + 35, hx, cy + 72);
        face_path.quadTo(hx - 52, cy + 35, hx - 60, cy - 20);
        face_path.closeSubpath();
    } else { // Diamond
        face_path.moveTo(hx - 62, cy - 10);
        face_path.quadTo(hx - 54, cy - 64, hx, cy - 64);
        face_path.quadTo(hx + 54, cy - 64, hx + 62, cy - 10);
        face_path.quadTo(hx + 56, cy + 42, hx, cy + 72);
        face_path.quadTo(hx - 56, cy + 42, hx - 62, cy - 10);
        face_path.closeSubpath();
    }

    painter.drawPath(face_path);

    // 6. Facial Features / Wrinkles / Blush
    const int makeup = mii.face_details.makeup.Value();
    const int wrinkles = mii.face_details.wrinkles.Value();

    if (makeup == 1) { // Soft cheek blush
        QColor blush(230, 90, 80, 55);
        painter.setBrush(blush);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(hx - 48, cy + 18, 22, 14));
        painter.drawEllipse(QRectF(hx + 26, cy + 18, 22, 14));
    } else if (makeup == 2) { // Freckles
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadow_skin.darker(110));
        for (int s : {-1, 1}) {
            painter.drawEllipse(QRectF(hx + s * 30, cy + 16, 2.5, 2.5));
            painter.drawEllipse(QRectF(hx + s * 36, cy + 19, 2.2, 2.2));
            painter.drawEllipse(QRectF(hx + s * 26, cy + 22, 2.0, 2.0));
        }
    }

    if (wrinkles == 1) { // Smile lines
        painter.setPen(QPen(shadow_skin.darker(115), 1.2, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(QRectF(hx - 28, cy + 25, 12, 20), 40 * 16, 100 * 16);
        painter.drawArc(QRectF(hx + 16, cy + 25, 12, 20), -40 * 16, -100 * 16);
    } else if (wrinkles == 2) { // Forehead lines
        painter.setPen(QPen(shadow_skin.darker(115), 1.2, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(hx - 24, cy - 40), QPointF(hx + 24, cy - 40));
        painter.drawLine(QPointF(hx - 18, cy - 46), QPointF(hx + 18, cy - 46));
    }

    // 7. Eyes (Iconic avatar Eyes)
    const double eye_spacing = 16.0 + (static_cast<int>(mii.eye_details.x.Value()) - 2) * 1.8;
    const double eye_height = cy - 2.0 + (static_cast<int>(mii.eye_details.y.Value()) - 12) * 1.8;
    const double eye_sz = 1.0 + (static_cast<int>(mii.eye_details.scale.Value()) - 3) * 0.12;
    const int et = mii.eye_details.type.Value() % 6;
    const double user_rot = static_cast<int>(mii.eye_details.rotate.Value()) - 3;

    for (int side : {-1, 1}) {
        const double ex = hx + side * eye_spacing;
        const double ey = eye_height;
        const double rot = user_rot * 5.0 * side;

        painter.save();
        painter.translate(ex, ey);
        painter.rotate(rot);
        painter.scale(eye_sz, eye_sz);

        if (et == 0) { // Classic avatar Eye (Oval + black pupil + specular shine dot)
            painter.setPen(QPen(shadow_skin.darker(140), 1.4));
            painter.setBrush(Qt::white);
            painter.drawEllipse(QRectF(-12, -15, 24, 30));

            painter.setPen(Qt::NoPen);
            painter.setBrush(base_eye);
            painter.drawEllipse(QRectF(-8, -11, 16, 22));
            painter.setBrush(Qt::black);
            painter.drawEllipse(QRectF(-6, -9, 12, 17));

            // Specular highlight dot (Top left)
            painter.setBrush(Qt::white);
            painter.drawEllipse(QRectF(-6, -9, 6, 6));

        } else if (et == 1) { // Minimalist dot/pill Mii eye
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::black);
            painter.drawRoundedRect(QRectF(-7, -13, 14, 26), 7, 7);
            painter.setBrush(Qt::white);
            painter.drawEllipse(QRectF(-4, -10, 4, 5));

        } else if (et == 2) { // Happy curved arc eyes (^_^)
            painter.setPen(QPen(Qt::black, 3.8, Qt::SolidLine, Qt::RoundCap));
            painter.setBrush(Qt::NoBrush);
            QPainterPath arc_path;
            arc_path.moveTo(-11, 2);
            arc_path.quadTo(0, -14, 11, 2);
            painter.drawPath(arc_path);

        } else if (et == 3) { // Almond eye
            painter.setPen(QPen(shadow_skin.darker(140), 1.5));
            painter.setBrush(Qt::white);
            QPainterPath almond;
            almond.moveTo(-13, 0);
            almond.quadTo(0, -14, 13, 0);
            almond.quadTo(0, 14, -13, 0);
            painter.drawPath(almond);
            painter.setPen(Qt::NoPen);
            painter.setBrush(base_eye);
            painter.drawEllipse(QRectF(-7, -8, 14, 16));
            painter.setBrush(Qt::black);
            painter.drawEllipse(QRectF(-4, -5, 8, 10));
            painter.setBrush(Qt::white);
            painter.drawEllipse(QRectF(-4, -6, 4, 4));

        } else if (et == 4) { // Round curious open eye
            painter.setPen(QPen(shadow_skin.darker(140), 1.4));
            painter.setBrush(Qt::white);
            painter.drawEllipse(QRectF(-12, -12, 24, 24));
            painter.setPen(Qt::NoPen);
            painter.setBrush(base_eye);
            painter.drawEllipse(QRectF(-7, -7, 14, 14));
            painter.setBrush(Qt::black);
            painter.drawEllipse(QRectF(-4, -4, 8, 8));
            painter.setBrush(Qt::white);
            painter.drawEllipse(QRectF(-4, -5, 4, 4));

        } else { // Confident / smirk eye
            painter.setPen(QPen(shadow_skin.darker(140), 1.4));
            painter.setBrush(Qt::white);
            QPainterPath smirk;
            smirk.moveTo(-12, 3);
            smirk.quadTo(0, -9, 12, 1);
            smirk.quadTo(0, 11, -12, 3);
            painter.drawPath(smirk);
            painter.setPen(Qt::NoPen);
            painter.setBrush(base_eye);
            painter.drawEllipse(QRectF(-7, -6, 14, 14));
            painter.setBrush(Qt::black);
            painter.drawEllipse(QRectF(-4, -4, 8, 8));
            painter.setBrush(Qt::white);
            painter.drawEllipse(QRectF(-4, -5, 4, 4));
        }

        painter.restore();
    }

    // 8. Eyebrows
    const double brow_spacing = 18.0 + (static_cast<int>(mii.eyebrow_details.x.Value()) - 2) * 1.8;
    const double brow_height = cy - 22.0 + (static_cast<int>(mii.eyebrow_details.y.Value()) - 10) * 1.8;
    const double brow_sz = 1.0 + (static_cast<int>(mii.eyebrow_details.scale.Value()) - 3) * 0.12;
    const int bt = mii.eyebrow_details.style.Value() % 6;
    const double brow_rot = static_cast<int>(mii.eyebrow_details.rotate.Value()) - 3;

    for (int side : {-1, 1}) {
        const double bx = hx + side * brow_spacing;
        const double by = brow_height;
        const double brot = brow_rot * 6.0 * side;

        painter.save();
        painter.translate(bx, by);
        painter.rotate(brot);
        painter.scale(brow_sz, brow_sz);

        painter.setPen(Qt::NoPen);
        painter.setBrush(brow_color);

        if (bt == 0) { // Classic thick rounded bar
            painter.drawRoundedRect(QRectF(-14, -4, 28, 8), 4, 4);
        } else if (bt == 1) { // Arched gentle curve
            QPainterPath b_path;
            b_path.moveTo(-14, 3);
            b_path.quadTo(0, -7, 14, 0);
            b_path.quadTo(0, -2, -14, 3);
            painter.drawPath(b_path);
        } else if (bt == 2) { // Sharp angled
            QPainterPath b_path;
            b_path.moveTo(-15, 4 * side);
            b_path.lineTo(15, -4 * side);
            b_path.lineTo(15, -9 * side);
            b_path.lineTo(-15, 0);
            b_path.closeSubpath();
            painter.drawPath(b_path);
        } else if (bt == 3) { // Bushy flat
            painter.drawRoundedRect(QRectF(-15, -5, 30, 10), 3, 3);
        } else if (bt == 4) { // Thin arched line
            painter.setPen(QPen(brow_color, 3.0, Qt::SolidLine, Qt::RoundCap));
            painter.setBrush(Qt::NoBrush);
            painter.drawArc(QRectF(-13, -7, 26, 14), 30 * 16, 120 * 16);
        } else { // Dots
            painter.drawEllipse(QRectF(-6, -6, 12, 12));
        }

        painter.restore();
    }

    // 9. Nose (Classic avatar Nose)
    const double ny = cy + 18.0 + (static_cast<int>(mii.nose_details.y.Value()) - 8) * 1.8;
    const double ns = 1.0 + (static_cast<int>(mii.nose_details.scale.Value()) - 2) * 0.15;
    const int nt = mii.nose_details.type.Value() % 6;

    painter.save();
    painter.translate(hx, ny);
    painter.scale(ns, ns);

    if (nt == 0) { // Classic triangle / wedge nose with drop shadow
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 30));
        painter.drawEllipse(QRectF(-7, 2, 14, 6));

        painter.setPen(QPen(shadow_skin.darker(120), 1.2));
        painter.setBrush(base_skin.darker(110));
        QPainterPath nose_tri;
        nose_tri.moveTo(0, -9);
        nose_tri.lineTo(7, 4);
        nose_tri.lineTo(-7, 4);
        nose_tri.closeSubpath();
        painter.drawPath(nose_tri);

    } else if (nt == 1) { // Button / round nose
        painter.setPen(QPen(shadow_skin.darker(125), 1.5));
        painter.setBrush(base_skin.darker(108));
        painter.drawEllipse(QRectF(-6, -6, 12, 12));

    } else if (nt == 2) { // Pointy nose
        painter.setPen(QPen(shadow_skin.darker(125), 1.4));
        painter.setBrush(base_skin.darker(110));
        QPainterPath tri;
        tri.moveTo(-4, -8);
        tri.lineTo(7, 2);
        tri.lineTo(-4, 4);
        tri.closeSubpath();
        painter.drawPath(tri);

    } else if (nt == 3) { // Rounded wide nose
        painter.setPen(QPen(shadow_skin.darker(125), 1.5));
        painter.setBrush(base_skin.darker(108));
        painter.drawRoundedRect(QRectF(-9, -4, 18, 9), 4, 4);

    } else if (nt == 4) { // L-shaped nose line
        painter.setPen(QPen(shadow_skin.darker(135), 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(0, -8), QPointF(0, 4));
        painter.drawLine(QPointF(0, 4), QPointF(5, 4));

    } else { // Nostril dots
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadow_skin.darker(140));
        painter.drawEllipse(QRectF(-5, 0, 3, 3));
        painter.drawEllipse(QRectF(2, 0, 3, 3));
    }

    painter.restore();

    // 10. Mustache & Beard
    const int mustache = mii.mustache_details.mustache_type.Value();
    const int beard = mii.beard_details.type.Value();

    if (beard > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(base_hair.darker(110));
        if (beard == 1) { // Chin Goatee
            painter.drawRoundedRect(QRectF(hx - 12, cy + 54, 24, 14), 6, 6);
        } else if (beard == 2) { // Full chin strap
            QPainterPath beard_path;
            beard_path.moveTo(hx - 50, cy + 30);
            beard_path.quadTo(hx - 40, cy + 68, hx, cy + 72);
            beard_path.quadTo(hx + 40, cy + 68, hx + 50, cy + 30);
            beard_path.quadTo(hx + 38, cy + 56, hx, cy + 60);
            beard_path.quadTo(hx - 38, cy + 56, hx - 50, cy + 30);
            painter.drawPath(beard_path);
        }
    }

    if (mustache > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(base_hair.darker(110));
        QPainterPath mustache_path;
        const double my = cy + 28.0;
        if (mustache == 1) { // Classic brush
            mustache_path.addRoundedRect(QRectF(hx - 20, my, 40, 11), 3, 3);
        } else if (mustache == 2) { // Handlebar
            mustache_path.moveTo(hx - 24, my + 10);
            mustache_path.quadTo(hx - 10, my - 2, hx, my + 3);
            mustache_path.quadTo(hx + 10, my - 2, hx + 24, my + 10);
            mustache_path.quadTo(hx, my + 8, hx - 24, my + 10);
        } else { // Small pencil
            mustache_path.addRoundedRect(QRectF(hx - 14, my + 2, 28, 5), 2, 2);
        }
        painter.drawPath(mustache_path);
    }

    // 11. Mouth (Classic avatar Mouth)
    const double m_pos_y = cy + 38.0 + (static_cast<int>(mii.mustache_details.mouth_y.Value()) - 13) * 1.8;
    const double m_sz = 1.0 + (static_cast<int>(mii.mouth_details.scale.Value()) - 3) * 0.15;
    const int mt = mii.mouth_details.type.Value() % 6;

    painter.save();
    painter.translate(hx, m_pos_y);
    painter.scale(m_sz, m_sz);

    if (mt == 0) { // Classic simple smile line
        painter.setPen(QPen(shadow_skin.darker(160), 2.8, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        QPainterPath m_path;
        m_path.moveTo(-16, -3);
        m_path.quadTo(0, 9, 16, -3);
        painter.drawPath(m_path);

    } else if (mt == 1) { // Open happy mouth with white teeth & pink tongue
        painter.setPen(QPen(shadow_skin.darker(170), 1.8));
        painter.setBrush(QColor("#6A1818"));
        QPainterPath open_m;
        open_m.moveTo(-16, -2);
        open_m.quadTo(0, -6, 16, -2);
        open_m.quadTo(0, 19, -16, -2);
        painter.drawPath(open_m);

        painter.setClipPath(open_m);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(QRectF(-14, -6, 28, 7), 2, 2);

        painter.setBrush(QColor("#E26B6B"));
        painter.drawEllipse(QRectF(-10, 5, 20, 14));

    } else if (mt == 2) { // Neutral line
        painter.setPen(QPen(shadow_skin.darker(160), 2.6, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(-13, 0), QPointF(13, 0));

    } else if (mt == 3) { // Grin
        painter.setPen(QPen(shadow_skin.darker(160), 2.8, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(-15, 2), QPointF(15, -2));

    } else if (mt == 4) { // Cute cat mouth (:3)
        painter.setPen(QPen(shadow_skin.darker(160), 2.4, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        QPainterPath cat_m;
        cat_m.moveTo(-12, -2);
        cat_m.quadTo(-6, 5, 0, 0);
        cat_m.quadTo(6, 5, 12, -2);
        painter.drawPath(cat_m);

    } else { // Wide smile with teeth
        painter.setPen(QPen(shadow_skin.darker(170), 1.8));
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(QRectF(-15, -4, 30, 9), 4, 4);
        painter.setPen(QPen(shadow_skin.darker(160), 1.2));
        painter.drawLine(QPointF(0, -4), QPointF(0, 5));
    }

    painter.restore();

    // 12. Mole (Beauty mark)
    if (mii.mole_details.type.Value() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(40, 25, 20));
        painter.drawEllipse(QRectF(hx + 28, cy + 32, 4, 4));
    }

    // 13. Glasses
    const int glasses = mii.glasses_details.type.Value();
    if (glasses > 0) {
        const int gt = glasses % 4;
        painter.setPen(QPen(glasses_color, 3.2));
        painter.setBrush(Qt::transparent);
        const double gy = cy - 2.0;
        if (gt == 1) { // Rectangular
            painter.drawRoundedRect(QRectF(hx - 44, gy - 12, 34, 24), 4, 4);
            painter.drawRoundedRect(QRectF(hx + 10, gy - 12, 34, 24), 4, 4);
        } else if (gt == 2) { // Round
            painter.drawEllipse(QRectF(hx - 43, gy - 14, 30, 28));
            painter.drawEllipse(QRectF(hx + 13, gy - 14, 30, 28));
        } else { // Sunglasses
            painter.setBrush(QColor(30, 30, 30, 215));
            painter.drawRoundedRect(QRectF(hx - 44, gy - 13, 34, 26), 6, 6);
            painter.drawRoundedRect(QRectF(hx + 10, gy - 13, 34, 26), 6, 6);
        }
        painter.drawLine(QPointF(hx - 10, gy), QPointF(hx + 10, gy));
    }

    // 14. Front Hair (Classic avatar Hairstyles)
    painter.setPen(Qt::NoPen);
    QLinearGradient hair_grad(hx - 60, cy - 70, hx + 50, cy + 30);
    hair_grad.setColorAt(0.0, base_hair.lighter(118));
    hair_grad.setColorAt(0.5, base_hair);
    hair_grad.setColorAt(1.0, base_hair.darker(125));
    painter.setBrush(hair_grad);

    QPainterPath hair_path;
    bool has_hair = true;

    if (hs == 0) { // Classic Side-Part / Casual Mii hair
        hair_path.moveTo(hx - 60 * flip_mul, cy + 10);
        hair_path.quadTo(hx - 64 * flip_mul, cy - 68, hx, cy - 70);
        hair_path.quadTo(hx + 64 * flip_mul, cy - 68, hx + 60 * flip_mul, cy + 10);
        hair_path.quadTo(hx + 56 * flip_mul, cy - 25, hx + 20 * flip_mul, cy - 36);
        hair_path.quadTo(hx - 10 * flip_mul, cy - 42, hx - 40 * flip_mul, cy - 30);
        hair_path.quadTo(hx - 56 * flip_mul, cy - 20, hx - 60 * flip_mul, cy + 10);
        hair_path.closeSubpath();

    } else if (hs == 1) { // Spiky tufts
        hair_path.moveTo(hx - 62, cy + 8);
        hair_path.lineTo(hx - 68, cy - 30);
        hair_path.lineTo(hx - 50, cy - 50);
        hair_path.lineTo(hx - 36, cy - 74);
        hair_path.lineTo(hx - 12, cy - 58);
        hair_path.lineTo(hx + 4, cy - 78);
        hair_path.lineTo(hx + 22, cy - 60);
        hair_path.lineTo(hx + 45, cy - 72);
        hair_path.lineTo(hx + 62, cy - 36);
        hair_path.lineTo(hx + 60, cy + 8);
        hair_path.lineTo(hx + 46, cy - 18);
        hair_path.lineTo(hx + 25, cy - 32);
        hair_path.lineTo(hx + 6, cy - 20);
        hair_path.lineTo(hx - 12, cy - 34);
        hair_path.lineTo(hx - 38, cy - 22);
        hair_path.closeSubpath();

    } else if (hs == 2) { // Neat Side-swept Bangs
        hair_path.moveTo(hx - 62, cy + 8);
        hair_path.quadTo(hx - 64, cy - 70, hx, cy - 72);
        hair_path.quadTo(hx + 64, cy - 70, hx + 62, cy + 8);
        hair_path.quadTo(hx + 54, cy - 15, hx + 32, cy - 22);
        hair_path.quadTo(hx, cy - 26, hx - 35, cy - 42);
        hair_path.closeSubpath();

    } else if (hs == 3) { // Buzzcut / Short crop
        hair_path.moveTo(hx - 60, cy - 5);
        hair_path.quadTo(hx - 62, cy - 66, hx, cy - 68);
        hair_path.quadTo(hx + 62, cy - 66, hx + 60, cy - 5);
        hair_path.quadTo(hx + 50, cy - 36, hx, cy - 44);
        hair_path.quadTo(hx - 50, cy - 36, hx - 60, cy - 5);
        hair_path.closeSubpath();

    } else if (hs == 4) { // Bald / Shaved
        has_hair = false;

    } else if (hs == 5) { // Bob cut
        hair_path.moveTo(hx - 64, cy + 25);
        hair_path.quadTo(hx - 66, cy - 70, hx, cy - 72);
        hair_path.quadTo(hx + 66, cy - 70, hx + 64, cy + 25);
        hair_path.quadTo(hx + 56, cy + 10, hx + 45, cy - 28);
        hair_path.quadTo(hx, cy - 36, hx - 45, cy - 28);
        hair_path.quadTo(hx - 56, cy + 10, hx - 64, cy + 25);
        hair_path.closeSubpath();

    } else if (hs == 6) { // Ponytail front fringe
        hair_path.moveTo(hx - 60, cy + 5);
        hair_path.quadTo(hx - 64, cy - 68, hx, cy - 70);
        hair_path.quadTo(hx + 64, cy - 68, hx + 60, cy + 5);
        hair_path.quadTo(hx + 46, cy - 30, hx, cy - 34);
        hair_path.quadTo(hx - 46, cy - 30, hx - 60, cy + 5);
        hair_path.closeSubpath();

    } else if (hs == 7) { // Long straight hair front
        hair_path.moveTo(hx - 64, cy + 45);
        hair_path.quadTo(hx - 66, cy - 72, hx, cy - 74);
        hair_path.quadTo(hx + 66, cy - 72, hx + 64, cy + 45);
        hair_path.quadTo(hx + 56, cy - 10, hx + 46, cy - 32);
        hair_path.quadTo(hx, cy - 38, hx - 46, cy - 32);
        hair_path.quadTo(hx - 56, cy - 10, hx - 64, cy + 45);
        hair_path.closeSubpath();

    } else if (hs == 8) { // Curly / Afro puff
        hair_path.addEllipse(QPointF(hx, cy - 40), 72, 60);

    } else if (hs == 9) { // Executive side-part
        hair_path.moveTo(hx - 62, cy + 8);
        hair_path.quadTo(hx - 64, cy - 68, hx, cy - 70);
        hair_path.quadTo(hx + 64, cy - 68, hx + 62, cy + 8);
        hair_path.quadTo(hx + 40, cy - 30, hx + 10, cy - 36);
        hair_path.quadTo(hx - 30, cy - 38, hx - 62, cy + 8);
        hair_path.closeSubpath();

    } else { // Parted bangs with fringe
        hair_path.moveTo(hx - 60, cy + 5);
        hair_path.quadTo(hx - 62, cy - 68, hx, cy - 70);
        hair_path.quadTo(hx + 62, cy - 68, hx + 60, cy + 5);
        hair_path.quadTo(hx + 40, cy - 25, hx + 8, cy - 32);
        hair_path.lineTo(hx, cy - 42);
        hair_path.lineTo(hx - 8, cy - 32);
        hair_path.quadTo(hx - 40, cy - 25, hx - 60, cy + 5);
        hair_path.closeSubpath();
    }

    if (has_hair) {
        painter.setPen(QPen(base_hair.darker(130), 1.2));
        painter.drawPath(hair_path);

        // 3D subtle specular hair sheen arc
        painter.setPen(QPen(base_hair.lighter(135), 3.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(QRectF(hx - 45, cy - 64, 90, 40), 55 * 16, 70 * 16);
    }

    return pix;
}

} // namespace MiiMaker
