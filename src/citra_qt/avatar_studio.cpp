#include "citra_qt/avatar_studio.h"

#include <algorithm>
#include <random>
#include <vector>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>
#include "citra_qt/qr/mii_qr.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "citra_qt/console_modal.h"
#include "citra_qt/uisettings.h"
#include "citra_qt/uisounds.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/cfg/cfg.h"

namespace AvatarStudio {

namespace {

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

#pragma pack(push, 1)
struct CharInfoStudio {
    uint8_t beard_color;
    uint8_t beard_type;
    uint8_t build;
    uint8_t eye_aspect;
    uint8_t eye_color;
    uint8_t eye_rotate;
    uint8_t eye_scale;
    uint8_t eye_type;
    uint8_t eye_x;
    uint8_t eye_y;
    uint8_t eyebrow_aspect;
    uint8_t eyebrow_color;
    uint8_t eyebrow_rotate;
    uint8_t eyebrow_scale;
    uint8_t eyebrow_type;
    uint8_t eyebrow_x;
    uint8_t eyebrow_y;
    uint8_t faceline_color;
    uint8_t faceline_make;
    uint8_t faceline_type;
    uint8_t faceline_wrinkle;
    uint8_t favorite_color;
    uint8_t gender;
    uint8_t glass_color;
    uint8_t glass_scale;
    uint8_t glass_type;
    uint8_t glass_y;
    uint8_t hair_color;
    uint8_t hair_flip;
    uint8_t hair_type;
    uint8_t height;
    uint8_t mole_scale;
    uint8_t mole_type;
    uint8_t mole_x;
    uint8_t mole_y;
    uint8_t mouth_aspect;
    uint8_t mouth_color;
    uint8_t mouth_scale;
    uint8_t mouth_type;
    uint8_t mouth_y;
    uint8_t mustache_scale;
    uint8_t mustache_type;
    uint8_t mustache_y;
    uint8_t nose_scale;
    uint8_t nose_type;
    uint8_t nose_y;
};

struct RenderRequestPacket {
    uint8_t  data[96];
    uint16_t dataLength;
    uint8_t  modelFlag;
    uint8_t  responseFormat;
    uint16_t resolution;
    int16_t  texResolution;
    uint8_t  viewType;
    int8_t   resourceType;
    uint8_t  shaderType;
    uint8_t  expression;
    uint32_t expressionFlag[3];
    int16_t  cameraRotate[3];
    int16_t  modelRotate[3];
    uint8_t  backgroundColor[4];
    uint8_t  aaMethod;
    uint8_t  drawStageMode;
    bool     verifyCharInfo;
    bool     verifyCRC16;
    bool     lightEnable;
    int8_t   clothesColor;
    int8_t   pantsColor;
    int8_t   bodyType;
    int8_t   headwearIndex;
    int8_t   headwearColor;
    uint8_t  instanceCount;
    uint8_t  instanceRotationMode;
    int16_t  lightDirection[3];
    uint8_t  splitMode;
};
#pragma pack(pop)

class Mii3DClient {
public:
    static Mii3DClient& Instance() {
        static Mii3DClient s_instance;
        return s_instance;
    }

    ~Mii3DClient() {
        Shutdown();
    }

    void Shutdown() {
        if (m_process) {
            m_process->kill();
            m_process->waitForFinished(500);
            delete m_process;
            m_process = nullptr;
            m_port = 0;
        }
    }

    bool EnsureEngineRunning() {
        if (m_process && m_process->state() == QProcess::Running && m_port > 0) {
            QTcpSocket check;
            check.connectToHost(QHostAddress::LocalHost, m_port);
            if (check.waitForConnected(50)) {
                check.disconnectFromHost();
                return true;
            }
        }

        Shutdown();

        QString bin_path = FindEngineBinary();
        if (bin_path.isEmpty()) {
            return false;
        }

        QString res_path = FindResourceDirectory();

        QTcpServer server;
        if (!server.listen(QHostAddress::LocalHost, 0)) {
            return false;
        }
        m_port = server.serverPort();
        server.close();

        m_process = new QProcess();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (!res_path.isEmpty()) {
            env.insert(QStringLiteral("RIO_CONTENT_PATH"), res_path + QStringLiteral("/fs/content"));
            env.insert(QStringLiteral("MII_RESOURCE_PATH"), res_path + QStringLiteral("/FFLResHigh.dat"));
            m_process->setWorkingDirectory(res_path);
        }
        m_process->setProcessEnvironment(env);

        QStringList args;
        args << QStringLiteral("--port") << QString::number(m_port);
        if (!res_path.isEmpty()) {
            args << QStringLiteral("--resource-high") << (res_path + QStringLiteral("/FFLResHigh.dat"));
            args << QStringLiteral("--resource-path") << res_path;
        }

        m_process->start(bin_path, args);
        if (!m_process->waitForStarted(3000)) {
            delete m_process;
            m_process = nullptr;
            m_port = 0;
            return false;
        }

        // Wait up to 3 seconds for TCP listener to be ready
        for (int i = 0; i < 60; ++i) {
            if (m_process->state() != QProcess::Running) {
                break;
            }
            QTcpSocket probe;
            probe.connectToHost(QHostAddress::LocalHost, m_port);
            if (probe.waitForConnected(50)) {
                probe.disconnectFromHost();
                return true;
            }
            QThread::msleep(40);
        }

        return false;
    }

    QImage Render(const AvatarData& data, int resolution = 512, int yaw = 0, int pitch = 0, int view_type = 0) {
        if (!EnsureEngineRunning()) {
            return QImage();
        }

        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, m_port);
        if (!socket.waitForConnected(1000)) {
            return QImage();
        }

        CharInfoStudio studio{};
        PopulateCharInfoStudio(data, studio);

        RenderRequestPacket req{};
        std::memset(&req, 0, sizeof(req));
        std::memcpy(req.data, &studio, sizeof(studio));
        req.dataLength = sizeof(studio);
        req.modelFlag = 1;
        req.responseFormat = 2; // RESPONSE_FORMAT_TGA_BGRA_FLIP_Y
        req.resolution = static_cast<uint16_t>(resolution);
        req.texResolution = 512;
        req.viewType = (view_type == 1) ? 2 /* VIEW_TYPE_ALL_BODY */ : 0 /* VIEW_TYPE_FACE */;
        req.resourceType = -1;
        req.shaderType = static_cast<uint8_t>(std::clamp(data.shader_style, 0, 2));
        req.expression = static_cast<uint8_t>(std::clamp(data.expression, 0, 6));
        req.cameraRotate[0] = 0;
        req.cameraRotate[1] = 0;
        req.cameraRotate[2] = 0;
        req.modelRotate[0] = 0; // Turntable yaw-only to prevent tilt
        req.modelRotate[1] = static_cast<int16_t>(yaw);
        req.modelRotate[2] = 0;
        req.backgroundColor[0] = 255;
        req.backgroundColor[1] = 255;
        req.backgroundColor[2] = 255;
        req.backgroundColor[3] = 0; // Transparent background
        req.lightEnable = true;
        req.clothesColor = static_cast<int8_t>(std::clamp(data.favorite_color, 0, 11));
        req.pantsColor = 0;
        req.bodyType = -1;
        req.headwearIndex = (data.hat_type > 0 && data.hat_type <= 9) ? static_cast<int8_t>(data.hat_type) : -1;
        req.headwearColor = static_cast<int8_t>(std::clamp(data.favorite_color, 0, 11));

        socket.write(reinterpret_cast<const char*>(&req), sizeof(req));
        if (!socket.waitForBytesWritten(1000)) {
            return QImage();
        }

        QByteArray resp;
        const int expected_payload = resolution * resolution * 4;
        const int expected_total = 18 + expected_payload;
        while (resp.size() < expected_total && socket.waitForReadyRead(1000)) {
            resp.append(socket.readAll());
        }
        while (socket.bytesAvailable() > 0) {
            resp.append(socket.readAll());
        }

        if (resp.size() < expected_total) {
            return QImage();
        }

        const uchar* pixels = reinterpret_cast<const uchar*>(resp.constData() + 18);
        QImage img(pixels, resolution, resolution, resolution * 4, QImage::Format_ARGB32);
        return img.mirrored(false, true);
    }

private:
    Mii3DClient() = default;

    static QString FindEngineBinary() {
        const QString app_dir = QCoreApplication::applicationDirPath();
        QString candidate;
#ifdef Q_OS_MACOS
        // Ships as a nested LSUIElement .app (see src/citra_meta/CMakeLists.txt) so macOS never
        // gives it a Dock icon, instead of a flat binary inheriting the main app's Info.plist.
        candidate = app_dir + QStringLiteral("/../Frameworks/mii_render_engine.app/Contents/MacOS/mii_render_engine");
        if (QFile::exists(candidate)) return QFileInfo(candidate).canonicalFilePath();
#endif
        candidate = app_dir + QStringLiteral("/mii_render_engine");
        if (QFile::exists(candidate)) return candidate;

        candidate = app_dir + QStringLiteral("/../../bin/Release/mii_render_engine");
        if (QFile::exists(candidate)) return QFileInfo(candidate).canonicalFilePath();

        candidate = QDir::current().filePath(QStringLiteral("build/arm64/bin/Release/mii_render_engine"));
        if (QFile::exists(candidate)) return QFileInfo(candidate).canonicalFilePath();

        if (QFile::exists(QStringLiteral("/tmp/mii-renderer-backend/ffl_testing_2"))) {
            return QStringLiteral("/tmp/mii-renderer-backend/ffl_testing_2");
        }
        return QString();
    }

    static QString FindResourceDirectory() {
        const QString app_dir = QCoreApplication::applicationDirPath();
        QString candidate = app_dir + QStringLiteral("/../Resources/mii_resources");
        if (QDir(candidate).exists()) return QFileInfo(candidate).canonicalFilePath();

        candidate = QDir::current().filePath(QStringLiteral("dist/mii_resources"));
        if (QDir(candidate).exists()) return QFileInfo(candidate).canonicalFilePath();

        candidate = app_dir + QStringLiteral("/../../dist/mii_resources");
        if (QDir(candidate).exists()) return QFileInfo(candidate).canonicalFilePath();

        if (QDir(QStringLiteral("/tmp/mii-renderer-backend")).exists()) {
            return QStringLiteral("/tmp/mii-renderer-backend");
        }
        return QString();
    }

    static void PopulateCharInfoStudio(const AvatarData& data, CharInfoStudio& studio) {
        std::memset(&studio, 0, sizeof(studio));
        studio.beard_color = static_cast<uint8_t>(std::clamp(data.facial_hair_color, 0, 99));
        studio.beard_type = std::clamp(data.beard_type, 0, 5);
        studio.build = std::clamp(data.build, 0, 128);
        studio.eye_aspect = 3;
        studio.eye_color = std::clamp(data.eyes_color, 0, 5);
        studio.eye_rotate = std::clamp(data.eyes_rotation + 4, 0, 7);
        studio.eye_scale = std::clamp(data.eyes_scale + 4, 0, 7);
        studio.eye_type = std::clamp(data.eyes_type, 0, 59);
        studio.eye_x = std::clamp(data.eyes_x + 2, 0, 15);
        studio.eye_y = std::clamp(data.eyes_y + 12, 0, 18);

        studio.eyebrow_aspect = 3;
        studio.eyebrow_color = static_cast<uint8_t>(std::clamp(data.eyebrows_color, 0, 99));
        studio.eyebrow_rotate = std::clamp(data.eyebrows_rotation + 6, 0, 11);
        studio.eyebrow_scale = std::clamp(data.eyebrows_scale + 4, 0, 8);
        studio.eyebrow_type = std::clamp(data.eyebrows_type, 0, 23);
        studio.eyebrow_x = std::clamp(data.eyebrows_x + 2, 0, 15);
        studio.eyebrow_y = std::clamp(data.eyebrows_y + 10, 0, 18);

        studio.faceline_color = std::clamp(data.skin_color, 0, 9);
        studio.faceline_make = std::clamp(data.makeup_type, 0, 11);
        studio.faceline_type = std::clamp(data.face_type, 0, 11);
        studio.faceline_wrinkle = std::clamp(data.wrinkles_type, 0, 11);

        studio.favorite_color = std::clamp(data.favorite_color, 0, 11);
        studio.gender = std::clamp(data.gender, 0, 1);

        studio.glass_color = std::clamp(data.glasses_color, 0, 5);
        studio.glass_scale = std::clamp(data.glasses_scale + 4, 0, 7);
        studio.glass_type = std::clamp(data.glasses_type, 0, 19);
        studio.glass_y = std::clamp(data.glasses_y + 10, 0, 20);

        studio.hair_color = static_cast<uint8_t>(std::clamp(data.hair_color, 0, 99));
        studio.hair_flip = data.hair_flip ? 1 : 0;
        studio.hair_type = std::clamp(data.hair_type, 0, 131);

        studio.height = std::clamp(data.height, 0, 128);

        studio.mole_scale = std::clamp(data.mole_scale + 4, 0, 8);
        studio.mole_type = data.mole_enable ? 1 : 0;
        studio.mole_x = std::clamp(data.mole_x + 2, 0, 16);
        studio.mole_y = std::clamp(data.mole_y + 20, 0, 30);

        studio.mouth_aspect = 3;
        studio.mouth_color = std::clamp(data.mouth_color, 0, 4);
        studio.mouth_scale = std::clamp(data.mouth_scale + 4, 0, 8);
        studio.mouth_type = std::clamp(data.mouth_type, 0, 35);
        studio.mouth_y = std::clamp(data.mouth_y + 13, 0, 18);

        studio.mustache_scale = 4;
        studio.mustache_type = std::clamp(data.mustache_type, 0, 5);
        studio.mustache_y = 10;

        studio.nose_scale = std::clamp(data.nose_scale + 4, 0, 8);
        studio.nose_type = std::clamp(data.nose_type, 0, 17);
        studio.nose_y = std::clamp(data.nose_y + 9, 0, 18);
    }

    QProcess* m_process = nullptr;
    int m_port = 0;
};

class InteractiveAvatarPreview : public QLabel {
public:
    explicit InteractiveAvatarPreview(QWidget* parent = nullptr) : QLabel(parent) {
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
    }

    std::function<void(int, int)> on_drag_rotate;
    std::function<void()> on_double_click_reset;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragging = true;
            m_last_pos = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
        QLabel::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_dragging && on_drag_rotate) {
            QPoint delta = event->pos() - m_last_pos;
            m_last_pos = event->pos();
            on_drag_rotate(delta.x(), delta.y());
        }
        QLabel::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragging = false;
            setCursor(Qt::OpenHandCursor);
        }
        QLabel::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (on_double_click_reset) {
            on_double_click_reset();
        }
        QLabel::mouseDoubleClickEvent(event);
    }

private:
    bool m_dragging = false;
    QPoint m_last_pos;
};

static QPixmap MakeGlyphPixmap(const QString& glyph, const QColor& bg_color, const QColor& text_color = Qt::white, int size = 20, qreal dpr = 2.0) {
    const int phys_size = qRound(size * dpr);
    QPixmap pix(phys_size, phys_size);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg_color);
    p.drawEllipse(0, 0, size, size);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(size <= 20 ? 11 : 13);
    p.setFont(f);
    p.setPen(text_color);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, glyph);
    return pix;
}

static QIcon MakeGlyphIcon(const QString& glyph, const QColor& bg_color, const QColor& text_color = Qt::white, int size = 20) {
    return QIcon(MakeGlyphPixmap(glyph, bg_color, text_color, size, 2.0));
}

} // namespace

QString UserDataDir() {
    const QString config_dir =
        QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir));
    QDir dir(config_dir);
    dir.cdUp();
    const QString user_dir = dir.absoluteFilePath(QStringLiteral("user"));
    QDir().mkpath(user_dir);
    return user_dir + QStringLiteral("/");
}

// =============================================================================
// Color Palettes
// =============================================================================

const std::vector<QColor>& GetFavoriteColors() {
    static const std::vector<QColor> colors = {
        QColor(0xd2, 0x1e, 0x14), // 0: Red
        QColor(0xff, 0x6e, 0x19), // 1: Orange
        QColor(0xff, 0xd8, 0x20), // 2: Yellow
        QColor(0x78, 0xd2, 0x20), // 3: Lime
        QColor(0x00, 0x78, 0x30), // 4: Green
        QColor(0x0a, 0x48, 0xbc), // 5: Blue
        QColor(0x3c, 0xaa, 0xde), // 6: Light Blue
        QColor(0xf5, 0x5a, 0x7d), // 7: Pink
        QColor(0x73, 0x28, 0xad), // 8: Purple
        QColor(0x48, 0x38, 0x18), // 9: Brown
        QColor(0xe0, 0xe0, 0xe0), // 10: White
        QColor(0x22, 0x22, 0x22), // 11: Black
    };
    return colors;
}

const std::vector<QColor>& GetSkinColors() {
    static const std::vector<QColor> colors = {
        QColor(0xFF, 0xEB, 0xDC), // 0: Porcelain
        QColor(0xFF, 0xD5, 0xB3), // 1: Fair Peach
        QColor(0xF9, 0xC6, 0x9D), // 2: Warm Ivory
        QColor(0xFE, 0xB6, 0x6B), // 3: Golden Peach
        QColor(0xEA, 0xAB, 0x7E), // 4: Honey Beige
        QColor(0xDE, 0x79, 0x42), // 5: Warm Tan
        QColor(0xCD, 0x75, 0x48), // 6: Amber Olive
        QColor(0xAD, 0x51, 0x29), // 7: Rich Bronze
        QColor(0x7C, 0x3A, 0x1A), // 8: Deep Chestnut
        QColor(0x52, 0x24, 0x14), // 9: Dark Espresso
    };
    return colors;
}

const std::vector<QColor>& GetHairColors() {
    static const std::vector<QColor> colors = {
        // Standard Nintendo 3DS colors (0-7)
        QColor(0x1A, 0x1A, 0x1A), // 0: Black
        QColor(0x40, 0x20, 0x10), // 1: Dark Brown
        QColor(0x5C, 0x18, 0x0A), // 2: Reddish Brown
        QColor(0x7C, 0x3A, 0x14), // 3: Auburn
        QColor(0x78, 0x78, 0x80), // 4: Grey
        QColor(0x4E, 0x3E, 0x11), // 5: Olive Brown
        QColor(0x87, 0x59, 0x17), // 6: Chestnut
        QColor(0xD0, 0xA0, 0x49), // 7: Blonde
        // Extended & Vibrant colors (8-25)
        QColor(0xFA, 0xEB, 0xB4), // 8: Platinum Blonde
        QColor(0xE0, 0xE4, 0xEA), // 9: Silver White
        QColor(0xC0, 0x48, 0x14), // 10: Copper Ginger
        QColor(0x9E, 0x1A, 0x1A), // 11: Crimson Red
        QColor(0xD4, 0x28, 0x28), // 12: Bright Red
        QColor(0xF0, 0x6C, 0x24), // 13: Neon Orange
        QColor(0xEE, 0xB6, 0x18), // 14: Golden Yellow
        QColor(0x7C, 0xBA, 0x34), // 15: Lime Green
        QColor(0x1E, 0x78, 0x3E), // 16: Emerald Green
        QColor(0x28, 0xAA, 0x90), // 17: Teal Mint
        QColor(0x38, 0x9C, 0xEA), // 18: Sky Blue
        QColor(0x1A, 0x4A, 0xB8), // 19: Royal Blue
        QColor(0x18, 0x24, 0x64), // 20: Navy
        QColor(0xA0, 0x78, 0xD4), // 21: Lavender
        QColor(0x6A, 0x22, 0x9E), // 22: Purple
        QColor(0xB8, 0x1C, 0x78), // 23: Magenta
        QColor(0xE8, 0x44, 0x8A), // 24: Hot Pink
        QColor(0xF6, 0x98, 0xB8), // 25: Pastel Pink
    };
    return colors;
}

const std::vector<QColor>& GetEyeColors() {
    static const std::vector<QColor> colors = {
        QColor(0x1A, 0x1A, 0x1A), // 0: Black
        QColor(0x71, 0x73, 0x72), // 1: Grey
        QColor(0x66, 0x3C, 0x2C), // 2: Brown
        QColor(0x68, 0x65, 0x37), // 3: Hazel
        QColor(0x4B, 0x58, 0xA8), // 4: Blue
        QColor(0x38, 0x70, 0x59), // 5: Green
    };
    return colors;
}

const std::vector<QColor>& GetMouthColors() {
    static const std::vector<QColor> colors = {
        QColor(0xD0, 0x44, 0x01), // 0: Coral
        QColor(0xF3, 0x01, 0x00), // 1: Red
        QColor(0xFD, 0x39, 0x3A), // 2: Pink
        QColor(0xF5, 0x88, 0x62), // 3: Peach
        QColor(0x3A, 0x22, 0x18), // 4: Dark
    };
    return colors;
}

const std::vector<QColor>& GetGlassesColors() {
    static const std::vector<QColor> colors = {
        QColor(0x18, 0x18, 0x18), // 0: Black
        QColor(0x5D, 0x39, 0x1A), // 1: Brown
        QColor(0xA0, 0x16, 0x12), // 2: Red
        QColor(0x2E, 0x39, 0x69), // 3: Blue
        QColor(0xA4, 0x60, 0x1E), // 4: Yellow/Amber
        QColor(0x76, 0x6F, 0x67), // 5: Silver/Grey
    };
    return colors;
}

// =============================================================================
// AvatarData JSON serialization
// =============================================================================

AvatarData AvatarData::Default() {
    AvatarData d;
    d.gender = 0;
    d.nickname = QStringLiteral("Player");
    d.favorite_color = 5; // Blue
    d.face_type = 0;
    d.skin_color = 0;
    d.makeup_type = 0;
    d.wrinkles_type = 0;
    d.hair_type = 1;
    d.hair_color = 0;
    d.hair_flip = false;
    d.eyebrows_type = 0;
    d.eyebrows_color = 0;
    d.eyes_type = 0;
    d.eyes_color = 0;
    d.nose_type = 0;
    d.mouth_type = 0;
    d.mouth_color = 0;
    d.mustache_type = 0;
    d.beard_type = 0;
    d.facial_hair_color = 0;
    d.glasses_type = 0;
    d.glasses_color = 0;
    d.mole_enable = false;
    d.hat_type = 0;
    d.hat_color = 5;
    return d;
}

AvatarData AvatarData::Random() {
    static std::mt19937 rng(std::random_device{}());
    AvatarData d;
    std::uniform_int_distribution<int> dist_gender(0, 1);
    std::uniform_int_distribution<int> dist_color(0, 11);
    std::uniform_int_distribution<int> dist_face(0, 11);
    std::uniform_int_distribution<int> dist_skin(0, 5);
    std::uniform_int_distribution<int> dist_hair(0, 131);
    std::uniform_int_distribution<int> dist_hair_col(0, 7);
    std::uniform_int_distribution<int> dist_eyebrows(0, 23);
    std::uniform_int_distribution<int> dist_eyes(0, 59);
    std::uniform_int_distribution<int> dist_eye_col(0, 5);
    std::uniform_int_distribution<int> dist_nose(0, 17);
    std::uniform_int_distribution<int> dist_mouth(0, 35);
    std::uniform_int_distribution<int> dist_mouth_col(0, 4);
    std::uniform_int_distribution<int> dist_mustache(0, 5);
    std::uniform_int_distribution<int> dist_beard(0, 5);
    std::uniform_int_distribution<int> dist_glasses(0, 19);

    d.gender = dist_gender(rng);
    d.nickname = QStringLiteral("Player");
    d.favorite_color = dist_color(rng);
    d.face_type = dist_face(rng);
    d.skin_color = dist_skin(rng);
    d.hair_type = dist_hair(rng);
    d.hair_color = dist_hair_col(rng);
    d.hair_flip = dist_gender(rng) == 1;
    d.eyebrows_type = dist_eyebrows(rng);
    d.eyebrows_color = d.hair_color;
    d.eyes_type = dist_eyes(rng);
    d.eyes_color = dist_eye_col(rng);
    d.nose_type = dist_nose(rng);
    d.mouth_type = dist_mouth(rng);
    d.mouth_color = dist_mouth_col(rng);

    // Occasional facial hair or glasses
    d.mustache_type = (dist_gender(rng) == 0 && dist_mustache(rng) > 3) ? dist_mustache(rng) : 0;
    d.beard_type = (dist_gender(rng) == 0 && dist_beard(rng) > 3) ? dist_beard(rng) : 0;
    d.facial_hair_color = d.hair_color;
    d.glasses_type = (dist_color(rng) > 8) ? dist_glasses(rng) : 0;
    d.glasses_color = dist_skin(rng);
    d.mole_enable = false;
    d.hat_type = 0;
    return d;
}

QJsonObject AvatarData::ToJson() const {
    QJsonObject j;
    j[QStringLiteral("gender")] = gender;
    j[QStringLiteral("nickname")] = nickname;
    j[QStringLiteral("favorite_color")] = favorite_color;
    j[QStringLiteral("face_type")] = face_type;
    j[QStringLiteral("skin_color")] = skin_color;
    j[QStringLiteral("makeup_type")] = makeup_type;
    j[QStringLiteral("wrinkles_type")] = wrinkles_type;
    j[QStringLiteral("hair_type")] = hair_type;
    j[QStringLiteral("hair_color")] = hair_color;
    j[QStringLiteral("hair_flip")] = hair_flip;
    j[QStringLiteral("eyebrows_type")] = eyebrows_type;
    j[QStringLiteral("eyebrows_color")] = eyebrows_color;
    j[QStringLiteral("eyebrows_y")] = eyebrows_y;
    j[QStringLiteral("eyebrows_x")] = eyebrows_x;
    j[QStringLiteral("eyebrows_scale")] = eyebrows_scale;
    j[QStringLiteral("eyebrows_rotation")] = eyebrows_rotation;
    j[QStringLiteral("eyes_type")] = eyes_type;
    j[QStringLiteral("eyes_color")] = eyes_color;
    j[QStringLiteral("eyes_y")] = eyes_y;
    j[QStringLiteral("eyes_x")] = eyes_x;
    j[QStringLiteral("eyes_scale")] = eyes_scale;
    j[QStringLiteral("eyes_rotation")] = eyes_rotation;
    j[QStringLiteral("nose_type")] = nose_type;
    j[QStringLiteral("nose_y")] = nose_y;
    j[QStringLiteral("nose_scale")] = nose_scale;
    j[QStringLiteral("mouth_type")] = mouth_type;
    j[QStringLiteral("mouth_color")] = mouth_color;
    j[QStringLiteral("mouth_y")] = mouth_y;
    j[QStringLiteral("mouth_scale")] = mouth_scale;
    j[QStringLiteral("mustache_type")] = mustache_type;
    j[QStringLiteral("beard_type")] = beard_type;
    j[QStringLiteral("facial_hair_color")] = facial_hair_color;
    j[QStringLiteral("glasses_type")] = glasses_type;
    j[QStringLiteral("glasses_color")] = glasses_color;
    j[QStringLiteral("glasses_y")] = glasses_y;
    j[QStringLiteral("glasses_scale")] = glasses_scale;
    j[QStringLiteral("mole_enable")] = mole_enable;
    j[QStringLiteral("hat_type")] = hat_type;
    j[QStringLiteral("hat_color")] = hat_color;
    j[QStringLiteral("height")] = height;
    j[QStringLiteral("build")] = build;
    j[QStringLiteral("expression")] = expression;
    j[QStringLiteral("view_mode")] = view_mode;
    j[QStringLiteral("shader_style")] = shader_style;
    j[QStringLiteral("camera_yaw")] = camera_yaw;
    j[QStringLiteral("camera_pitch")] = camera_pitch;
    return j;
}

AvatarData AvatarData::FromJson(const QJsonObject& j) {
    AvatarData d = Default();
    if (j.contains(QStringLiteral("gender"))) d.gender = j[QStringLiteral("gender")].toInt();
    if (j.contains(QStringLiteral("nickname"))) d.nickname = j[QStringLiteral("nickname")].toString();
    if (j.contains(QStringLiteral("favorite_color"))) d.favorite_color = j[QStringLiteral("favorite_color")].toInt();
    if (j.contains(QStringLiteral("face_type"))) d.face_type = j[QStringLiteral("face_type")].toInt();
    if (j.contains(QStringLiteral("skin_color"))) d.skin_color = j[QStringLiteral("skin_color")].toInt();
    if (j.contains(QStringLiteral("makeup_type"))) d.makeup_type = j[QStringLiteral("makeup_type")].toInt();
    if (j.contains(QStringLiteral("wrinkles_type"))) d.wrinkles_type = j[QStringLiteral("wrinkles_type")].toInt();
    if (j.contains(QStringLiteral("hair_type"))) d.hair_type = j[QStringLiteral("hair_type")].toInt();
    if (j.contains(QStringLiteral("hair_color"))) d.hair_color = j[QStringLiteral("hair_color")].toInt();
    if (j.contains(QStringLiteral("hair_flip"))) d.hair_flip = j[QStringLiteral("hair_flip")].toBool();
    if (j.contains(QStringLiteral("eyebrows_type"))) d.eyebrows_type = j[QStringLiteral("eyebrows_type")].toInt();
    if (j.contains(QStringLiteral("eyebrows_color"))) d.eyebrows_color = j[QStringLiteral("eyebrows_color")].toInt();
    if (j.contains(QStringLiteral("eyebrows_y"))) d.eyebrows_y = j[QStringLiteral("eyebrows_y")].toInt();
    if (j.contains(QStringLiteral("eyebrows_x"))) d.eyebrows_x = j[QStringLiteral("eyebrows_x")].toInt();
    if (j.contains(QStringLiteral("eyebrows_scale"))) d.eyebrows_scale = j[QStringLiteral("eyebrows_scale")].toInt();
    if (j.contains(QStringLiteral("eyebrows_rotation"))) d.eyebrows_rotation = j[QStringLiteral("eyebrows_rotation")].toInt();
    if (j.contains(QStringLiteral("eyes_type"))) d.eyes_type = j[QStringLiteral("eyes_type")].toInt();
    if (j.contains(QStringLiteral("eyes_color"))) d.eyes_color = j[QStringLiteral("eyes_color")].toInt();
    if (j.contains(QStringLiteral("eyes_y"))) d.eyes_y = j[QStringLiteral("eyes_y")].toInt();
    if (j.contains(QStringLiteral("eyes_x"))) d.eyes_x = j[QStringLiteral("eyes_x")].toInt();
    if (j.contains(QStringLiteral("eyes_scale"))) d.eyes_scale = j[QStringLiteral("eyes_scale")].toInt();
    if (j.contains(QStringLiteral("eyes_rotation"))) d.eyes_rotation = j[QStringLiteral("eyes_rotation")].toInt();
    if (j.contains(QStringLiteral("nose_type"))) d.nose_type = j[QStringLiteral("nose_type")].toInt();
    if (j.contains(QStringLiteral("nose_y"))) d.nose_y = j[QStringLiteral("nose_y")].toInt();
    if (j.contains(QStringLiteral("nose_scale"))) d.nose_scale = j[QStringLiteral("nose_scale")].toInt();
    if (j.contains(QStringLiteral("mouth_type"))) d.mouth_type = j[QStringLiteral("mouth_type")].toInt();
    if (j.contains(QStringLiteral("mouth_color"))) d.mouth_color = j[QStringLiteral("mouth_color")].toInt();
    if (j.contains(QStringLiteral("mouth_y"))) d.mouth_y = j[QStringLiteral("mouth_y")].toInt();
    if (j.contains(QStringLiteral("mouth_scale"))) d.mouth_scale = j[QStringLiteral("mouth_scale")].toInt();
    if (j.contains(QStringLiteral("mustache_type"))) d.mustache_type = j[QStringLiteral("mustache_type")].toInt();
    if (j.contains(QStringLiteral("beard_type"))) d.beard_type = j[QStringLiteral("beard_type")].toInt();
    if (j.contains(QStringLiteral("facial_hair_color"))) d.facial_hair_color = j[QStringLiteral("facial_hair_color")].toInt();
    if (j.contains(QStringLiteral("glasses_type"))) d.glasses_type = j[QStringLiteral("glasses_type")].toInt();
    if (j.contains(QStringLiteral("glasses_color"))) d.glasses_color = j[QStringLiteral("glasses_color")].toInt();
    if (j.contains(QStringLiteral("glasses_y"))) d.glasses_y = j[QStringLiteral("glasses_y")].toInt();
    if (j.contains(QStringLiteral("glasses_scale"))) d.glasses_scale = j[QStringLiteral("glasses_scale")].toInt();
    if (j.contains(QStringLiteral("mole_enable"))) d.mole_enable = j[QStringLiteral("mole_enable")].toBool();
    if (j.contains(QStringLiteral("hat_type"))) d.hat_type = j[QStringLiteral("hat_type")].toInt();
    if (j.contains(QStringLiteral("hat_color"))) d.hat_color = j[QStringLiteral("hat_color")].toInt();
    if (j.contains(QStringLiteral("height"))) d.height = j[QStringLiteral("height")].toInt();
    if (j.contains(QStringLiteral("build"))) d.build = j[QStringLiteral("build")].toInt();
    if (j.contains(QStringLiteral("expression"))) d.expression = j[QStringLiteral("expression")].toInt();
    if (j.contains(QStringLiteral("view_mode"))) d.view_mode = j[QStringLiteral("view_mode")].toInt();
    if (j.contains(QStringLiteral("shader_style"))) d.shader_style = j[QStringLiteral("shader_style")].toInt();
    if (j.contains(QStringLiteral("camera_yaw"))) d.camera_yaw = j[QStringLiteral("camera_yaw")].toInt();
    if (j.contains(QStringLiteral("camera_pitch"))) d.camera_pitch = j[QStringLiteral("camera_pitch")].toInt();
    return d;
}

// =============================================================================
// File Asset Path Resolution
// =============================================================================

QString GetSvgAssetPath(const QString& filename) {
    const QString app_dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MAC
    const QString bundle_path = app_dir + QStringLiteral("/../Resources/avatar_studio/svg/") + filename;
    if (QFile::exists(bundle_path)) {
        return bundle_path;
    }
#else
    const QString install_path = app_dir + QStringLiteral("/avatar_studio/svg/") + filename;
    if (QFile::exists(install_path)) {
        return install_path;
    }
#endif
    // Source tree fallback during development
    const QString dev_path = QDir::current().filePath(QStringLiteral("dist/avatar_studio/svg/") + filename);
    if (QFile::exists(dev_path)) {
        return dev_path;
    }
    return QString();
}

static QByteArray LoadSvgContent(const QString& filename) {
    const QString path = GetSvgAssetPath(filename);
    if (path.isEmpty()) {
        return QByteArray();
    }
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        return file.readAll();
    }
    return QByteArray();
}

// =============================================================================
// Real-time Composite 2D Renderer
// =============================================================================

static QPixmap RenderAvatar2D(const AvatarData& data, int size, bool draw_background) {
    const qreal dpr = 2.0; // Render supersampled for high-DPI crispness
    QPixmap pixmap(qRound(size * dpr), qRound(size * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const auto& fav_colors = GetFavoriteColors();
    const auto& skin_colors = GetSkinColors();
    const auto& hair_colors = GetHairColors();
    const auto& eye_colors = GetEyeColors();
    const auto& mouth_colors = GetMouthColors();
    const auto& glasses_colors = GetGlassesColors();

    const QColor fav_color = fav_colors[qBound(0, data.favorite_color, (int)fav_colors.size() - 1)];
    const QColor skin_color = skin_colors[qBound(0, data.skin_color, (int)skin_colors.size() - 1)];
    const QColor hair_color = hair_colors[qBound(0, data.hair_color, (int)hair_colors.size() - 1)];
    const QColor eye_color = eye_colors[qBound(0, data.eyes_color, (int)eye_colors.size() - 1)];
    const QColor mouth_color = mouth_colors[qBound(0, data.mouth_color, (int)mouth_colors.size() - 1)];
    const QColor beard_color = hair_colors[qBound(0, data.facial_hair_color, (int)hair_colors.size() - 1)];
    const QColor glasses_color = glasses_colors[qBound(0, data.glasses_color, (int)glasses_colors.size() - 1)];

    const qreal logical_size = size;

    // Optional background circle
    if (draw_background) {
        QPainterPath bg_circle;
        bg_circle.addEllipse(QRectF(8, 8, logical_size - 16, logical_size - 16));
        QRadialGradient grad(logical_size / 2.0, logical_size / 2.0 - 20, logical_size / 2.0);
        grad.setColorAt(0.0, QColor(0xF4, 0xF6, 0xFA));
        grad.setColorAt(1.0, QColor(0xE0, 0xE4, 0xEE));
        painter.fillPath(bg_circle, grad);
        painter.setPen(QPen(QColor(0, 0, 0, 24), 2));
        painter.drawPath(bg_circle);
    }

    // 1. Torso & Shirt
    {
        painter.save();
        QPainterPath torso_path;
        torso_path.moveTo(logical_size * 0.22, logical_size);
        torso_path.cubicTo(logical_size * 0.24, logical_size * 0.82,
                           logical_size * 0.32, logical_size * 0.74,
                           logical_size * 0.44, logical_size * 0.73);
        torso_path.lineTo(logical_size * 0.56, logical_size * 0.73);
        torso_path.cubicTo(logical_size * 0.68, logical_size * 0.74,
                           logical_size * 0.76, logical_size * 0.82,
                           logical_size * 0.78, logical_size);
        torso_path.closeSubpath();

        painter.setPen(Qt::NoPen);
        painter.setBrush(fav_color);
        painter.drawPath(torso_path);

        // Neck
        QPainterPath neck_path;
        neck_path.moveTo(logical_size * 0.44, logical_size * 0.73);
        neck_path.lineTo(logical_size * 0.44, logical_size * 0.65);
        neck_path.lineTo(logical_size * 0.56, logical_size * 0.65);
        neck_path.lineTo(logical_size * 0.56, logical_size * 0.73);
        painter.setBrush(skin_color);
        painter.drawPath(neck_path);

        // Collar
        QPainterPath collar_path;
        collar_path.moveTo(logical_size * 0.43, logical_size * 0.73);
        collar_path.lineTo(logical_size * 0.50, logical_size * 0.80);
        collar_path.lineTo(logical_size * 0.57, logical_size * 0.73);
        collar_path.cubicTo(logical_size * 0.53, logical_size * 0.75,
                            logical_size * 0.47, logical_size * 0.75,
                            logical_size * 0.43, logical_size * 0.73);
        painter.setPen(QPen(fav_color.darker(125), 3));
        painter.setBrush(fav_color);
        painter.drawPath(collar_path);
        painter.restore();
    }

    // SVG Coordinate Space: 52 x 52. Head center is at (26, 26).
    const qreal head_scale = (logical_size * 0.65) / 52.0;
    const qreal head_cx = logical_size * 0.50;
    const qreal head_cy = logical_size * 0.46;
    const QRectF base_head_rect(head_cx - 26.0 * head_scale, head_cy - 26.0 * head_scale,
                                52.0 * head_scale, 52.0 * head_scale);

    // 2. Head / Face Shape (face-XX.svg)
    {
        const int face_idx = qBound(0, data.face_type, 11) + 1;
        const QString face_name = QString::asprintf("face-%02d.svg", face_idx);
        QByteArray content = LoadSvgContent(face_name);
        if (!content.isEmpty()) {
            // Remove dummy preview features inside Vector_2
            content.replace("fill=\"#8D8D8D\"", "fill=\"none\"");
            // Replace face white fill with skin tone
            content.replace("fill=\"white\"", QStringLiteral("fill=\"%1\"").arg(skin_color.name()).toUtf8());
            content.replace("stroke=\"#6F6F6F\"", QStringLiteral("stroke=\"%1\"").arg(skin_color.darker(135).name()).toUtf8());
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                rend.render(&painter, base_head_rect);
            }
        }
    }

    // 3. Makeup (if selected)
    if (data.makeup_type > 0) {
        const QString mk_name = QString::asprintf("makeup-%02d.svg", data.makeup_type + 1);
        QByteArray content = LoadSvgContent(mk_name);
        if (!content.isEmpty()) {
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                rend.render(&painter, base_head_rect);
            }
        }
    }

    // 4. Wrinkles (if selected)
    if (data.wrinkles_type > 0) {
        const QString wr_name = QString::asprintf("wrinkles-%02d.svg", data.wrinkles_type + 1);
        QByteArray content = LoadSvgContent(wr_name);
        if (!content.isEmpty()) {
            content.replace("#996D54", skin_color.darker(140).name().toUtf8());
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                rend.render(&painter, base_head_rect);
            }
        }
    }

    // 5. Beard / Goatee (if selected)
    if (data.beard_type > 0) {
        const QString goatee_name = QString::asprintf("goatee-%d.svg", data.beard_type + 1);
        QByteArray content = LoadSvgContent(goatee_name);
        if (!content.isEmpty()) {
            // Strip white dummy head outline
            content.replace("fill=\"white\"", "fill=\"none\"");
            content.replace("stroke=\"#BFBFBF\"", "stroke=\"none\"");
            content.replace("#1E1E1E", beard_color.name().toUtf8());
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                rend.render(&painter, base_head_rect);
            }
        }
    }

    // 6. Nose
    {
        const int nose_idx = qBound(0, data.nose_type, 17) + 1;
        const QString nose_name = QString::asprintf("nose-%02d.svg", nose_idx);
        QByteArray content = LoadSvgContent(nose_name);
        if (!content.isEmpty()) {
            content.replace("#FF5F5F", skin_color.darker(145).name().toUtf8());
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                painter.save();
                const qreal n_scale = 1.0 + (data.nose_scale * 0.08);
                const qreal n_y = 3.8 + (data.nose_y * 0.45);
                painter.translate(head_cx, head_cy + n_y * head_scale);
                painter.scale(0.24 * n_scale * head_scale, 0.24 * n_scale * head_scale);
                painter.translate(-26.0, -26.0);
                rend.render(&painter, QRectF(0, 0, 52, 52));
                painter.restore();
            }
        }
    }

    // 7. Mouth
    {
        const int mouth_idx = qBound(0, data.mouth_type, 35) + 1;
        const QString mouth_name = QString::asprintf("mouth-%02d.svg", mouth_idx);
        QByteArray content = LoadSvgContent(mouth_name);
        if (!content.isEmpty()) {
            content.replace("#712A04", mouth_color.darker(120).name().toUtf8());
            content.replace("#BE4E26", mouth_color.name().toUtf8());
            content.replace("#0055FF", "#FFFFFF"); // tooth
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                painter.save();
                const qreal m_scale = 1.0 + (data.mouth_scale * 0.08);
                const qreal m_y = 8.5 + (data.mouth_y * 0.45);
                painter.translate(head_cx, head_cy + m_y * head_scale);
                painter.scale(0.28 * m_scale * head_scale, 0.28 * m_scale * head_scale);
                painter.translate(-26.0, -26.5);
                rend.render(&painter, QRectF(0, 0, 52, 52));
                painter.restore();
            }
        }
    }

    // 8. Mustache (if selected)
    if (data.mustache_type > 0) {
        const QString mus_name = QString::asprintf("mustache-%d.svg", data.mustache_type + 1);
        QByteArray content = LoadSvgContent(mus_name);
        if (!content.isEmpty()) {
            content.replace("fill=\"white\"", "fill=\"none\"");
            content.replace("stroke=\"#BFBFBF\"", "stroke=\"none\"");
            content.replace("#1E1E1E", beard_color.name().toUtf8());
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                rend.render(&painter, base_head_rect);
            }
        }
    }

    // 9. Eyes (rendered left and mirrored right with authentic Mii proportions)
    {
        const int eye_idx = qBound(0, data.eyes_type, 59) + 1;
        const QString eye_name = QString::asprintf("eyes-%02d.svg", eye_idx);
        QByteArray content = LoadSvgContent(eye_name);
        if (!content.isEmpty()) {
            content.replace("#6C7070", eye_color.name().toUtf8());
            content.replace("#0010BF", "#1A1A1A");
            content.replace("stroke=\"#0010BF\"", "stroke=\"#1A1A1A\"");
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                const qreal eye_base_scale = 0.20;
                const qreal eye_scale_mult = 1.0 + (data.eyes_scale * 0.08);
                const qreal eye_spacing = 6.2 + (data.eyes_x * 0.35);
                const qreal eye_y_offset = -0.6 + (data.eyes_y * 0.45);
                const qreal eye_rot = data.eyes_rotation * 3.0;

                // Left Eye (Screen Left - outer temple corner on left, mirrored)
                painter.save();
                painter.translate(head_cx - eye_spacing * head_scale, head_cy + eye_y_offset * head_scale);
                painter.rotate(-eye_rot);
                painter.scale(-eye_base_scale * eye_scale_mult * head_scale, eye_base_scale * eye_scale_mult * head_scale);
                painter.translate(-21.5, -26.5);
                rend.render(&painter, QRectF(0, 0, 52, 52));
                painter.restore();

                // Right Eye (Screen Right - outer temple corner on right, unmirrored)
                painter.save();
                painter.translate(head_cx + eye_spacing * head_scale, head_cy + eye_y_offset * head_scale);
                painter.rotate(eye_rot);
                painter.scale(eye_base_scale * eye_scale_mult * head_scale, eye_base_scale * eye_scale_mult * head_scale);
                painter.translate(-21.5, -26.5);
                rend.render(&painter, QRectF(0, 0, 52, 52));
                painter.restore();
            }
        }
    }

    // 10. Eyebrows (rendered left and mirrored right)
    {
        const int eb_idx = qBound(0, data.eyebrows_type, 23) + 1;
        const QString eb_name = QString::asprintf("eyebrows-%02d.svg", eb_idx);
        QByteArray content = LoadSvgContent(eb_name);
        if (!content.isEmpty()) {
            const QColor eb_color = hair_colors[qBound(0, data.eyebrows_color, (int)hair_colors.size() - 1)];
            content.replace("#E30000", eb_color.name().toUtf8());
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                const qreal eb_base_scale = 0.18;
                const qreal eb_scale_mult = 1.0 + (data.eyebrows_scale * 0.08);
                const qreal eb_spacing = 6.2 + (data.eyebrows_x * 0.35);
                const qreal eb_y_offset = -4.5 + (data.eyebrows_y * 0.45);
                const qreal eb_rot = data.eyebrows_rotation * 4.0;

                // Left Eyebrow (Screen Left - mirrored)
                painter.save();
                painter.translate(head_cx - eb_spacing * head_scale, head_cy + eb_y_offset * head_scale);
                painter.rotate(-eb_rot);
                painter.scale(-eb_base_scale * eb_scale_mult * head_scale, eb_base_scale * eb_scale_mult * head_scale);
                painter.translate(-26.0, -25.0);
                rend.render(&painter, QRectF(0, 0, 52, 52));
                painter.restore();

                // Right Eyebrow (Screen Right - unmirrored)
                painter.save();
                painter.translate(head_cx + eb_spacing * head_scale, head_cy + eb_y_offset * head_scale);
                painter.rotate(eb_rot);
                painter.scale(eb_base_scale * eb_scale_mult * head_scale, eb_base_scale * eb_scale_mult * head_scale);
                painter.translate(-26.0, -25.0);
                rend.render(&painter, QRectF(0, 0, 52, 52));
                painter.restore();
            }
        }
    }

    // 11. Glasses (if selected)
    if (data.glasses_type > 0) {
        const QString gl_name = QString::asprintf("glasses-%d.svg", data.glasses_type);
        QByteArray content = LoadSvgContent(gl_name);
        if (!content.isEmpty()) {
            content.replace("fill=\"white\"", "fill=\"none\"");
            content.replace("stroke=\"#BFBEBD\"", "stroke=\"none\"");
            content.replace("#7B0000", glasses_color.name().toUtf8());
            content.replace("#F60000", glasses_color.lighter(130).name().toUtf8());
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                painter.save();
                const qreal g_scale = 1.0 + (data.glasses_scale * 0.08);
                const qreal g_y = data.glasses_y * 0.45;
                painter.translate(head_cx, head_cy + g_y * head_scale);
                painter.scale(g_scale, g_scale);
                painter.translate(-head_cx, -head_cy);
                rend.render(&painter, base_head_rect);
                painter.restore();
            }
        }
    }

    // 12. Hair
    {
        const int hair_idx = qBound(0, data.hair_type, 131);
        const QString hair_name = QString::asprintf("hair-%d.svg", hair_idx);
        QByteArray content = LoadSvgContent(hair_name);
        if (!content.isEmpty()) {
            // Remove dummy white head outline
            content.replace("fill=\"white\"", "fill=\"none\"");
            content.replace("stroke=\"#BFBFBF\"", "stroke=\"none\"");
            content.replace("#5F5F5F", hair_color.name().toUtf8());
            content.replace("#BFBFBF", hair_color.lighter(130).name().toUtf8());
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                painter.save();
                if (data.hair_flip) {
                    painter.translate(head_cx, 0);
                    painter.scale(-1.0, 1.0);
                    painter.translate(-head_cx, 0);
                }
                rend.render(&painter, base_head_rect);
                painter.restore();
            }
        }
    }

    // 13. Hat (if selected)
    if (data.hat_type > 0) {
        const QString hat_name = QString::asprintf("hat-%02d.svg", data.hat_type);
        QByteArray content = LoadSvgContent(hat_name);
        if (!content.isEmpty()) {
            content.replace("var(--icon-hat-fill)", "#808080");
            content.replace("var(--icon-hat-stroke)", "#404040");
            content.replace("var(--icon-face-fill)", "transparent");
            content.replace("var(--icon-face-stroke)", "transparent");
            content.replace("fill=\"white\"", "fill=\"transparent\"");
            content.replace("stroke=\"#6F6F6F\"", "stroke=\"transparent\"");
            const QColor hat_col = fav_colors[qBound(0, data.favorite_color, (int)fav_colors.size() - 1)];
            content.replace("#808080", hat_col.name().toUtf8());
            content.replace("#404040", hat_col.darker(140).name().toUtf8());
            QSvgRenderer rend(content);
            if (rend.isValid()) {
                painter.save();
                painter.translate(0, -6.0 * head_scale);
                rend.render(&painter, base_head_rect);
                painter.restore();
            }
        }
    }

    return pixmap;
}

QPixmap RenderAvatar(const AvatarData& data, int size, bool draw_background) {
    // Attempt rendering with local 3D engine
    QImage img3d = Mii3DClient::Instance().Render(data, 512, data.camera_yaw, data.camera_pitch, data.view_mode);
    if (!img3d.isNull()) {
        const qreal dpr = 2.0;
        QPixmap pixmap(qRound(size * dpr), qRound(size * dpr));
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const qreal logical_size = size;
        if (draw_background) {
            QPainterPath bg_circle;
            bg_circle.addEllipse(QRectF(8, 8, logical_size - 16, logical_size - 16));
            const auto& fav_colors = GetFavoriteColors();
            int fav_idx = qBound(0, data.favorite_color, (int)fav_colors.size() - 1);
            QColor fav = fav_colors[fav_idx];

            QLinearGradient grad(0, 0, 0, logical_size);
            grad.setColorAt(0.0, fav.lighter(130));
            grad.setColorAt(1.0, fav);
            painter.fillPath(bg_circle, grad);
            painter.setPen(QPen(QColor(0, 0, 0, 24), 2));
            painter.drawPath(bg_circle);
        }

        QPixmap mii_pix = QPixmap::fromImage(img3d);
        painter.drawPixmap(QRectF(0, 0, logical_size, logical_size), mii_pix, mii_pix.rect());

        painter.end();
        return pixmap;
    }

    // Graceful fallback to 2D SVG composite
    return RenderAvatar2D(data, size, draw_background);
}

// =============================================================================
// Feature Thumbnail Renderer (for selector chips)
// =============================================================================

QPixmap RenderFeatureThumbnail(const QString& category, int index, const AvatarData& context, int size) {
    const qreal dpr = 2.0;
    QPixmap pixmap(qRound(size * dpr), qRound(size * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const auto& hair_colors = GetHairColors();
    const auto& eye_colors = GetEyeColors();
    const auto& mouth_colors = GetMouthColors();
    const auto& glasses_colors = GetGlassesColors();
    const QColor hair_color = hair_colors[qBound(0, context.hair_color, (int)hair_colors.size() - 1)];
    const QColor eye_color = eye_colors[qBound(0, context.eyes_color, (int)eye_colors.size() - 1)];
    const QColor mouth_color = mouth_colors[qBound(0, context.mouth_color, (int)mouth_colors.size() - 1)];
    const QColor glasses_color = glasses_colors[qBound(0, context.glasses_color, (int)glasses_colors.size() - 1)];

    QString filename;
    if (category == QStringLiteral("face")) {
        filename = QString::asprintf("face-%02d.svg", index + 1);
    } else if (category == QStringLiteral("hair")) {
        filename = QString::asprintf("hair-%d.svg", index);
    } else if (category == QStringLiteral("eyes")) {
        filename = QString::asprintf("eyes-%02d.svg", index + 1);
    } else if (category == QStringLiteral("eyebrows")) {
        filename = QString::asprintf("eyebrows-%02d.svg", index + 1);
    } else if (category == QStringLiteral("nose")) {
        filename = QString::asprintf("nose-%02d.svg", index + 1);
    } else if (category == QStringLiteral("mouth")) {
        filename = QString::asprintf("mouth-%02d.svg", index + 1);
    } else if (category == QStringLiteral("mustache")) {
        filename = QString::asprintf("mustache-%d.svg", index + 1);
    } else if (category == QStringLiteral("goatee")) {
        filename = QString::asprintf("goatee-%d.svg", index + 1);
    } else if (category == QStringLiteral("glasses")) {
        filename = QString::asprintf("glasses-%d.svg", index);
    } else if (category == QStringLiteral("hat")) {
        if (index == 0) {
            // "None" option (Clear / no hat)
            painter.setPen(QPen(QColor(120, 120, 120, 130), 2, Qt::DashLine));
            painter.drawEllipse(QRectF(8, 8, size - 16, size - 16));
            painter.setPen(QPen(QColor(220, 50, 50, 180), 2.5));
            painter.drawLine(QPointF(14, 14), QPointF(size - 14, size - 14));
            return pixmap;
        }
        filename = QString::asprintf("hat-%02d.svg", index);
    } else if (category == QStringLiteral("makeup")) {
        filename = QString::asprintf("makeup-%02d.svg", index + 1);
    } else if (category == QStringLiteral("wrinkles")) {
        filename = QString::asprintf("wrinkles-%02d.svg", index + 1);
    }

    if (filename.isEmpty()) {
        return pixmap;
    }

    QByteArray content = LoadSvgContent(filename);
    if (content.isEmpty()) {
        return pixmap;
    }

    // Color substitutions
    if (category == QStringLiteral("hair")) {
        content.replace("#5F5F5F", hair_color.name().toUtf8());
        content.replace("fill=\"white\"", "fill=\"#F0F2F6\"");
        content.replace("stroke=\"#BFBFBF\"", "stroke=\"#D2D6DF\"");
    } else if (category == QStringLiteral("eyes")) {
        content.replace("#6C7070", eye_color.name().toUtf8());
        content.replace("#0010BF", "#1A1A1A");
        content.replace("stroke=\"#0010BF\"", "stroke=\"#1A1A1A\"");
    } else if (category == QStringLiteral("eyebrows")) {
        content.replace("#E30000", hair_color.name().toUtf8());
    } else if (category == QStringLiteral("nose")) {
        content.replace("#FF5F5F", "#885544");
    } else if (category == QStringLiteral("mouth")) {
        content.replace("#712A04", mouth_color.darker(120).name().toUtf8());
        content.replace("#BE4E26", mouth_color.name().toUtf8());
        content.replace("#0055FF", "#FFFFFF");
    } else if (category == QStringLiteral("mustache") || category == QStringLiteral("goatee")) {
        content.replace("#1E1E1E", hair_color.name().toUtf8());
        content.replace("fill=\"white\"", "fill=\"#F0F2F6\"");
        content.replace("stroke=\"#BFBFBF\"", "stroke=\"#D2D6DF\"");
    } else if (category == QStringLiteral("glasses")) {
        content.replace("#7B0000", glasses_color.name().toUtf8());
        content.replace("#F60000", glasses_color.lighter(130).name().toUtf8());
        content.replace("fill=\"white\"", "fill=\"#F0F2F6\"");
        content.replace("stroke=\"#BFBEBD\"", "stroke=\"#D2D6DF\"");
    } else if (category == QStringLiteral("hat")) {
        content.replace("var(--icon-hat-fill)", "#808080");
        content.replace("var(--icon-hat-stroke)", "#404040");
        content.replace("var(--icon-face-fill)", "#F0F2F6");
        content.replace("var(--icon-face-stroke)", "#B4BAC6");
        content.replace("fill=\"white\"", "fill=\"#F0F2F6\"");
        content.replace("stroke=\"#6F6F6F\"", "stroke=\"#B4BAC6\"");
        const auto& fav_colors = GetFavoriteColors();
        const QColor hat_col = fav_colors[qBound(0, context.favorite_color, (int)fav_colors.size() - 1)];
        content.replace("#808080", hat_col.name().toUtf8());
        content.replace("#404040", hat_col.darker(140).name().toUtf8());
    } else if (category == QStringLiteral("face") || category == QStringLiteral("makeup") || category == QStringLiteral("wrinkles")) {
        content.replace("fill=\"white\"", "fill=\"#F0F2F6\"");
        content.replace("stroke=\"#6F6F6F\"", "stroke=\"#B4BAC6\"");
        content.replace("stroke=\"#999999\"", "stroke=\"#B4BAC6\"");
    }

    QSvgRenderer rend(content);
    if (rend.isValid()) {
        const QRectF bounds(4, 4, size - 8, size - 8);
        rend.render(&painter, bounds);
    }

    return pixmap;
}

// =============================================================================
// User Data Persistence
// =============================================================================

bool SaveAvatarToUserDir(const AvatarData& data, QString* out_png_path) {
    const QString user_dir = UserDataDir();
    const QString json_path = user_dir + QStringLiteral("avatar_data.json");
    const QString png_path = user_dir + QStringLiteral("profile_avatar.png");

    // Save JSON
    QFile json_file(json_path);
    if (json_file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(data.ToJson());
        json_file.write(doc.toJson(QJsonDocument::Indented));
        json_file.close();
    }

    // Save PNG
    QPixmap pix = RenderAvatar(data, 512, false);
    const bool ok = pix.save(png_path, "PNG");
    if (ok && out_png_path) {
        *out_png_path = png_path;
    }
    return ok;
}

AvatarData LoadAvatarFromUserDir() {
    const QString json_path = UserDataDir() + QStringLiteral("avatar_data.json");
    QFile json_file(json_path);
    if (json_file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(json_file.readAll());
        if (doc.isObject()) {
            return AvatarData::FromJson(doc.object());
        }
    }
    return AvatarData::Default();
}

// =============================================================================
// AvatarStudioModal implementation
// =============================================================================

class AvatarStudioModal : public QWidget {
public:
    explicit AvatarStudioModal(QWidget* parent, Core::System* system_ptr = nullptr,
                               std::function<void(const AvatarData&, const QPixmap&)> on_apply = nullptr)
        : QWidget(ResolveParent(parent)), system(system_ptr), on_applied_cb(std::move(on_apply)) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);

        if (parentWidget()) {
            setGeometry(parentWidget()->rect());
            parentWidget()->installEventFilter(this);
        } else {
            setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
            resize(920, 640);
        }
        setFocusPolicy(Qt::StrongFocus);
        setAcceptDrops(true);

        current_data = LoadAvatarFromUserDir();
        if (system && current_data.nickname == QStringLiteral("Player")) {
            auto cfg = Service::CFG::GetModule(*system);
            if (cfg) {
                const auto nick = cfg->GetUsername();
                if (!nick.empty()) {
                    current_data.nickname = QString::fromStdU16String(nick);
                }
            }
        }

        SetupUi();
    }

    ~AvatarStudioModal() override {
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
                if (mw->centralWidget()) return mw->centralWidget();
            }
            if (auto* w = parent->window()) {
                if (auto* mw = qobject_cast<QMainWindow*>(w)) {
                    if (mw->centralWidget()) return mw->centralWidget();
                }
                return w;
            }
            return parent;
        }
        if (auto* w = QApplication::activeWindow()) {
            if (auto* mw = qobject_cast<QMainWindow*>(w)) {
                if (mw->centralWidget()) return mw->centralWidget();
            }
            return w;
        }
        return nullptr;
    }

    void Exec() {
        show();
        raise();
        setFocus();
        AnimateIn();

        QEventLoop loop;
        m_loop = &loop;
        loop.exec();
        m_loop = nullptr;
    }

    void CloseWithSound() {
        if (closing) return;
        closing = true;
        UiSounds::Play(UiSounds::Name::Back);
        hide();
        if (m_loop && m_loop->isRunning()) {
            m_loop->quit();
        }
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent* event) override {
        if (event->mimeData()->hasUrls()) {
            const QList<QUrl> urls = event->mimeData()->urls();
            if (!urls.isEmpty()) {
                QString local_path = urls.first().toLocalFile();
                if (!local_path.isEmpty()) {
                    AvatarData imported_data;
                    QString error_msg;
                    if (MiiQR::ImportMiiFromFile(local_path, imported_data, &error_msg)) {
                        ApplyImportedAvatar(imported_data);
                        return;
                    } else {
                        UiSounds::Play(UiSounds::Name::Back);
                        QMessageBox::warning(this, tr("Import Failed"), tr("Could not import Mii: %1").arg(error_msg));
                    }
                }
            }
        } else if (event->mimeData()->hasImage()) {
            QImage img = qvariant_cast<QImage>(event->mimeData()->imageData());
            if (!img.isNull()) {
                AvatarData imported_data;
                QString error_msg;
                if (MiiQR::ImportMiiFromImage(img, imported_data, &error_msg)) {
                    ApplyImportedAvatar(imported_data);
                    return;
                } else {
                    UiSounds::Play(UiSounds::Name::Back);
                    QMessageBox::warning(this, tr("Import Failed"), tr("Could not import Mii: %1").arg(error_msg));
                }
            }
        }
    }

    void AnimateIn() {
        UiSounds::Play(UiSounds::Name::Notice);
        if (card_widget) {
            auto* fade = new QGraphicsOpacityEffect(card_widget);
            fade->setOpacity(0.0);
            card_widget->setGraphicsEffect(fade);
            auto* anim = new QPropertyAnimation(fade, "opacity", this);
            anim->setDuration(160);
            anim->setStartValue(0.0);
            anim->setEndValue(1.0);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            connect(anim, &QPropertyAnimation::finished, this, [this] {
                if (card_widget) card_widget->setGraphicsEffect(nullptr);
            });
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }

    void UpdateCardSize() {
        if (!card_widget) return;
        const int pw = parentWidget() ? parentWidget()->width() : width();
        const int ph = parentWidget() ? parentWidget()->height() : height();
        const int cw = std::clamp(pw - 32, 740, 840);
        const int ch = std::clamp(ph - 32, 480, 540);
        card_widget->setFixedSize(cw, ch);
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            setGeometry(parentWidget()->rect());
            UpdateCardSize();
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter p(this);
        const bool light = IsCurrentThemeLight();
        p.fillRect(rect(), QColor(0, 0, 0, light ? 140 : 180));
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            CloseWithSound();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            OnSaveAvatar();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (card_widget && !card_widget->geometry().contains(event->pos())) {
            CloseWithSound();
        }
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        event->accept();
    }

private:
    void SetupUi() {
        const bool light = IsCurrentThemeLight();
        const QString card_bg = light ? QStringLiteral("#FFFFFF") : QStringLiteral("#1E222A");
        const QString text_main = light ? QStringLiteral("#16181D") : QStringLiteral("#FFFFFF");
        const QString text_sub = light ? QStringLiteral("rgba(22, 24, 29, 0.65)") : QStringLiteral("rgba(255, 255, 255, 0.65)");
        const QString border_col = light ? QStringLiteral("rgba(0, 0, 0, 0.08)") : QStringLiteral("rgba(255, 255, 255, 0.10)");

        auto* root_layout = new QVBoxLayout(this);
        root_layout->setAlignment(Qt::AlignCenter);
        root_layout->setContentsMargins(24, 24, 24, 24);

        card_widget = new QWidget(this);
        card_widget->setObjectName(QStringLiteral("avatarStudioCard"));
        card_widget->setFixedSize(880, 580);
        card_widget->setStyleSheet(QStringLiteral(
            "#avatarStudioCard { background-color: %1; border-radius: 0px; border: 1px solid %2; }")
            .arg(card_bg, border_col));

        auto* card_layout = new QVBoxLayout(card_widget);
        card_layout->setContentsMargins(20, 16, 20, 14);
        card_layout->setSpacing(12);

        // Header (console taste, title "Mii Studio", Save Mii, Import QR, Export QR pills)
        auto* header_layout = new QHBoxLayout();
        auto* title_lbl = new QLabel(tr("Mii Studio"), card_widget);
        title_lbl->setStyleSheet(QStringLiteral(
            "font-size: 22px; font-weight: 700; color: %1; border: none; background: transparent;").arg(text_main));
        header_layout->addWidget(title_lbl);
        header_layout->addStretch();

        const QString pill_bg = light ? QStringLiteral("rgba(0, 0, 0, 0.05)") : QStringLiteral("rgba(255, 255, 255, 0.08)");
        const QString pill_hover = light ? QStringLiteral("rgba(0, 0, 0, 0.10)") : QStringLiteral("rgba(255, 255, 255, 0.15)");
        const QString pill_pressed = light ? QStringLiteral("rgba(0, 0, 0, 0.16)") : QStringLiteral("rgba(255, 255, 255, 0.22)");
        const QString pill_style = QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  border: none;"
            "  border-radius: 15px;"
            "  color: %2;"
            "  font-size: 13px;"
            "  font-weight: 600;"
            "  padding: 0 16px;"
            "}"
            "QPushButton:hover { background: %3; }"
            "QPushButton:pressed { background: %4; }")
            .arg(pill_bg, text_main, pill_hover, pill_pressed);

        auto* save_mii_btn = new QPushButton(tr("Save Mii"), card_widget);
        save_mii_btn->setFixedHeight(30);
        save_mii_btn->setCursor(Qt::PointingHandCursor);
        save_mii_btn->setStyleSheet(pill_style);
        save_mii_btn->setToolTip(tr("Save as standard Nintendo .mii file to user/miis/ and sync to 3DS game database"));
        connect(save_mii_btn, &QPushButton::clicked, this, &AvatarStudioModal::OnSaveMiiToUserFolder);
        header_layout->addWidget(save_mii_btn);

        auto* import_qr_btn = new QPushButton(tr("Import QR"), card_widget);
        import_qr_btn->setFixedHeight(30);
        import_qr_btn->setCursor(Qt::PointingHandCursor);
        import_qr_btn->setStyleSheet(pill_style);
        connect(import_qr_btn, &QPushButton::clicked, this, &AvatarStudioModal::OnImportQr);
        header_layout->addWidget(import_qr_btn);

        auto* export_qr_btn = new QPushButton(tr("Export QR"), card_widget);
        export_qr_btn->setFixedHeight(30);
        export_qr_btn->setCursor(Qt::PointingHandCursor);
        export_qr_btn->setStyleSheet(pill_style);
        connect(export_qr_btn, &QPushButton::clicked, this, &AvatarStudioModal::OnExportQr);
        header_layout->addWidget(export_qr_btn);

        card_layout->addLayout(header_layout);

        // Main Body: 3 Columns (2/10 mains, 5/10 character preview, 3/10 in-depth library)
        auto* body_layout = new QHBoxLayout();
        body_layout->setSpacing(16);

        // -------------------------------------------------------------
        // LEFT COLUMN: "Mains" Category List (2/10 width, vertically stacked, text only)
        // -------------------------------------------------------------
        auto* left_col = new QWidget(card_widget);
        auto* left_layout = new QVBoxLayout(left_col);
        left_layout->setContentsMargins(0, 0, 0, 0);
        left_layout->setSpacing(4);

        struct CategoryDef { QString id; QString title; };
        const std::vector<CategoryDef> categories = {
            {QStringLiteral("face"), tr("Face")},
            {QStringLiteral("hair"), tr("Hair")},
            {QStringLiteral("eyes"), tr("Eyes")},
            {QStringLiteral("eyebrows"), tr("Eyebrows")},
            {QStringLiteral("nose"), tr("Nose")},
            {QStringLiteral("mouth"), tr("Mouth")},
            {QStringLiteral("facial"), tr("Facial Hair")},
            {QStringLiteral("body"), tr("Body")},
            {QStringLiteral("style"), tr("Style")},
        };

        tab_btn_group = new QButtonGroup(this);
        for (int i = 0; i < (int)categories.size(); ++i) {
            auto* btn = new QPushButton(categories[i].title, left_col);
            btn->setCheckable(true);
            btn->setFixedHeight(34);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: transparent; border: none; border-radius: 10px; "
                "font-size: 13px; font-weight: 600; color: %1; text-align: left; padding-left: 14px; }"
                "QPushButton:hover { background: %2; }"
                "QPushButton:checked { background: #3C8CE0; color: #FFFFFF; font-weight: 700; border-radius: 10px; }")
                .arg(text_main, light ? QStringLiteral("rgba(0,0,0,0.05)") : QStringLiteral("rgba(255,255,255,0.06)")));
            tab_btn_group->addButton(btn, i);
            left_layout->addWidget(btn);
            if (i == 0) btn->setChecked(true);
        }

        connect(tab_btn_group, &QButtonGroup::idClicked, this, [this, categories](int id) {
            UiSounds::Play(UiSounds::Name::SelectTab);
            active_tab = categories[id].id;
            PopulateTabDeck();
        });

        left_layout->addStretch();
        body_layout->addWidget(left_col, 2);

        // -------------------------------------------------------------
        // CENTER COLUMN: Character Preview Panel (5/10 width)
        // -------------------------------------------------------------
        auto* center_col = new QWidget(card_widget);
        auto* center_layout = new QVBoxLayout(center_col);
        center_layout->setContentsMargins(0, 0, 0, 0);
        center_layout->setSpacing(0);
        center_layout->setAlignment(Qt::AlignCenter);

        // Stage frame: centered, transparent, sharp
        avatar_preview_label = new InteractiveAvatarPreview(center_col);
        avatar_preview_label->setFixedSize(390, 390);
        avatar_preview_label->setAlignment(Qt::AlignCenter);
        avatar_preview_label->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        center_layout->addWidget(avatar_preview_label, 0, Qt::AlignCenter);

        avatar_preview_label->on_drag_rotate = [this](int dx, int /*dy*/) {
            current_data.camera_yaw = (current_data.camera_yaw + dx) % 360;
            if (current_data.camera_yaw < 0) {
                current_data.camera_yaw += 360;
            }
            current_data.camera_pitch = 0;
            UpdatePreview();
        };

        avatar_preview_label->on_double_click_reset = [this] {
            current_data.camera_yaw = 0;
            current_data.camera_pitch = 0;
            UpdatePreview();
        };

        // Spacing to push the view mode buttons down
        center_layout->addSpacing(18);

        // View toggle: Head Only vs Body
        auto* view_toggle_container = new QWidget(center_col);
        auto* view_toggle_layout = new QHBoxLayout(view_toggle_container);
        view_toggle_layout->setContentsMargins(0, 0, 0, 0);
        view_toggle_layout->setSpacing(8);
        view_toggle_layout->setAlignment(Qt::AlignCenter);

        auto* head_btn = new QPushButton(tr("Head"), view_toggle_container);
        auto* body_btn = new QPushButton(tr("Body"), view_toggle_container);
        head_btn->setCheckable(true);
        body_btn->setCheckable(true);
        head_btn->setChecked(current_data.view_mode == 0);
        body_btn->setChecked(current_data.view_mode == 1);
        head_btn->setFixedHeight(28);
        body_btn->setFixedHeight(28);

        const QString vt_style = QStringLiteral(
            "QPushButton { background: %1; border: none; border-radius: 8px; font-size: 12px; font-weight: 600; padding: 0 16px; color: %2; }"
            "QPushButton:hover { background: %3; }"
            "QPushButton:checked { background: #3C8CE0; color: white; border-radius: 8px; }")
            .arg(light ? QStringLiteral("rgba(0,0,0,0.06)") : QStringLiteral("rgba(255,255,255,0.08)"),
                 text_main,
                 light ? QStringLiteral("rgba(0,0,0,0.12)") : QStringLiteral("rgba(255,255,255,0.15)"));
        head_btn->setStyleSheet(vt_style);
        body_btn->setStyleSheet(vt_style);

        connect(head_btn, &QPushButton::clicked, this, [=] {
            UiSounds::Play(UiSounds::Name::SelectMisc);
            head_btn->setChecked(true);
            body_btn->setChecked(false);
            current_data.view_mode = 0;
            UpdatePreview();
        });
        connect(body_btn, &QPushButton::clicked, this, [=] {
            UiSounds::Play(UiSounds::Name::SelectMisc);
            body_btn->setChecked(true);
            head_btn->setChecked(false);
            current_data.view_mode = 1;
            UpdatePreview();
        });
        view_toggle_layout->addWidget(head_btn);
        view_toggle_layout->addWidget(body_btn);
        center_layout->addWidget(view_toggle_container, 0, Qt::AlignCenter);

        body_layout->addWidget(center_col, 5);

        // -------------------------------------------------------------
        // RIGHT COLUMN: In-Depth Scrollable Library (3/10 width)
        // -------------------------------------------------------------
        auto* right_col = new QWidget(card_widget);
        auto* right_layout = new QVBoxLayout(right_col);
        right_layout->setContentsMargins(0, 0, 0, 0);
        right_layout->setSpacing(8);

        deck_scroll = new QScrollArea(right_col);
        deck_scroll->setWidgetResizable(true);
        deck_scroll->setFrameShape(QFrame::NoFrame);
        deck_scroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollBar:vertical { background: transparent; width: 6px; margin: 0px; }"
            "QScrollBar::handle:vertical { background: %1; min-height: 20px; border-radius: 3px; }"
            "QScrollBar::handle:vertical:hover { background: %2; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
            .arg(light ? QStringLiteral("rgba(0,0,0,0.18)") : QStringLiteral("rgba(255,255,255,0.22)"),
                 light ? QStringLiteral("rgba(0,0,0,0.35)") : QStringLiteral("rgba(255,255,255,0.40)")));

        deck_content_widget = new QWidget(deck_scroll);
        deck_layout = new QVBoxLayout(deck_content_widget);
        deck_layout->setContentsMargins(0, 0, 6, 0);
        deck_layout->setSpacing(12);
        deck_scroll->setWidget(deck_content_widget);

        right_layout->addWidget(deck_scroll, 1);
        body_layout->addWidget(right_col, 3);
        card_layout->addLayout(body_layout, 1);

        // -------------------------------------------------------------
        // FOOTER: Docked Console Actions ((B) Back & (A) Confirm)
        // -------------------------------------------------------------
        auto* footer_layout = new QHBoxLayout();
        footer_layout->setContentsMargins(0, 4, 0, 0);

        auto* legend_box = new QHBoxLayout();
        legend_box->setSpacing(12);

        auto add_legend_pill = [&](const QString& glyph, const QColor& col, const QString& verb) {
            auto* g_lbl = new QLabel(card_widget);
            g_lbl->setPixmap(MakeGlyphPixmap(glyph, col, Qt::white, 20));
            auto* t_lbl = new QLabel(verb, card_widget);
            t_lbl->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 500; color: %1;").arg(text_sub));
            legend_box->addWidget(g_lbl);
            legend_box->addWidget(t_lbl);
            legend_box->addSpacing(8);
        };

        add_legend_pill(QString::fromUtf8("✚"), QColor(0x88, 0x88, 0x88), tr("Select"));
        add_legend_pill(QStringLiteral("Drag"), QColor(0x88, 0x88, 0x88), tr("Rotate"));

        footer_layout->addLayout(legend_box);
        footer_layout->addStretch();

        // Action Buttons: Back (B) & Confirm (A)
        auto* back_btn = new QPushButton(card_widget);
        back_btn->setFixedHeight(36);
        back_btn->setMinimumWidth(95);
        back_btn->setCursor(Qt::PointingHandCursor);
        back_btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border: none; border-radius: 12px; padding: 0px; }"
            "QPushButton:hover { background: %2; }")
            .arg(light ? QStringLiteral("rgba(0,0,0,0.06)") : QStringLiteral("rgba(255,255,255,0.08)"),
                 light ? QStringLiteral("rgba(0,0,0,0.12)") : QStringLiteral("rgba(255,255,255,0.14)")));
        auto* back_btn_layout = new QHBoxLayout(back_btn);
        back_btn_layout->setContentsMargins(14, 0, 14, 0);
        back_btn_layout->setSpacing(8);
        auto* b_badge = new QLabel(back_btn);
        b_badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        b_badge->setPixmap(MakeGlyphPixmap(QStringLiteral("B"), QColor(0xe0, 0x3c, 0x3c), Qt::white, 20));
        auto* b_txt = new QLabel(tr("Back"), back_btn);
        b_txt->setAttribute(Qt::WA_TransparentForMouseEvents);
        b_txt->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1; background: transparent;").arg(text_main));
        back_btn_layout->addWidget(b_badge);
        back_btn_layout->addWidget(b_txt);
        connect(back_btn, &QPushButton::clicked, this, &AvatarStudioModal::CloseWithSound);
        footer_layout->addWidget(back_btn);

        auto* confirm_btn = new QPushButton(card_widget);
        confirm_btn->setFixedHeight(36);
        confirm_btn->setMinimumWidth(110);
        confirm_btn->setCursor(Qt::PointingHandCursor);
        confirm_btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #3C8CE0; border: none; border-radius: 12px; padding: 0px; }"
            "QPushButton:hover { background: #529FF0; }"));
        auto* confirm_btn_layout = new QHBoxLayout(confirm_btn);
        confirm_btn_layout->setContentsMargins(16, 0, 16, 0);
        confirm_btn_layout->setSpacing(8);
        auto* a_badge = new QLabel(confirm_btn);
        a_badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        a_badge->setPixmap(MakeGlyphPixmap(QStringLiteral("A"), Qt::white, QColor(0x1a, 0x6e, 0xc7), 20));
        auto* a_txt = new QLabel(tr("Confirm"), confirm_btn);
        a_txt->setAttribute(Qt::WA_TransparentForMouseEvents);
        a_txt->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: #FFFFFF; background: transparent;"));
        confirm_btn_layout->addWidget(a_badge);
        confirm_btn_layout->addWidget(a_txt);
        connect(confirm_btn, &QPushButton::clicked, this, &AvatarStudioModal::OnSaveAvatar);
        footer_layout->addWidget(confirm_btn);

        card_layout->addLayout(footer_layout);
        root_layout->addWidget(card_widget);

        // Initial setup
        active_tab = QStringLiteral("face");
        PopulateTabDeck();
        UpdatePreview();
        UpdateCardSize();
    }

    void UpdatePreview() {
        if (m_rendering) {
            m_pending_update = true;
            return;
        }
        m_rendering = true;
        m_pending_update = false;

        QPixmap full_avatar = RenderAvatar(current_data, 390, false);
        avatar_preview_label->setPixmap(full_avatar);

        m_rendering = false;
        if (m_pending_update) {
            QTimer::singleShot(0, this, [this] { UpdatePreview(); });
        }
    }

    // Helper: Clear current deck layout
    static void DeleteLayoutItemRecursive(QLayoutItem* item) {
        if (!item) return;
        if (item->layout()) {
            QLayout* l = item->layout();
            QLayoutItem* child;
            while ((child = l->takeAt(0)) != nullptr) {
                DeleteLayoutItemRecursive(child);
            }
        }
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    void ClearDeck() {
        QLayoutItem* item;
        while ((item = deck_layout->takeAt(0)) != nullptr) {
            DeleteLayoutItemRecursive(item);
        }
    }

    // Helper: Make section header
    QWidget* MakeSectionHeader(const QString& title) {
        auto* w = new QWidget(deck_content_widget);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 6, 0, 2);
        l->setSpacing(0);
        auto* lbl = new QLabel(title, w);
        lbl->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: %1;").arg(IsCurrentThemeLight() ? "#16181D" : "#FFFFFF"));
        l->addWidget(lbl);
        l->addStretch();
        return w;
    }

    // Helper: Color row (sharp square swatches in grid)
    QWidget* MakeColorRow(const std::vector<QColor>& colors, int selected_idx, std::function<void(int)> on_picked) {
        auto* w = new QWidget(deck_content_widget);
        auto* grid = new QGridLayout(w);
        grid->setContentsMargins(0, 2, 0, 6);
        grid->setSpacing(6);
        grid->setAlignment(Qt::AlignLeft);

        auto* group = new QButtonGroup(w);
        const int cols = 6;
        for (int i = 0; i < (int)colors.size(); ++i) {
            auto* btn = new QToolButton(w);
            btn->setFixedSize(28, 28);
            btn->setCheckable(true);
            btn->setCursor(Qt::PointingHandCursor);
            const QString border_style = (i == selected_idx)
                ? QStringLiteral("border: 3px solid #3C8CE0;")
                : QStringLiteral("border: 1px solid rgba(0,0,0,0.20);");
            btn->setStyleSheet(QStringLiteral(
                "QToolButton { background-color: %1; border-radius: 8px; %2 }"
                "QToolButton:hover { border: 2px solid #3C8CE0; }")
                .arg(colors[i].name(), border_style));
            group->addButton(btn, i);
            grid->addWidget(btn, i / cols, i % cols);
            if (i == selected_idx) btn->setChecked(true);
        }
        connect(group, &QButtonGroup::idClicked, this, [on_picked, this](int id) {
            UiSounds::Play(UiSounds::Name::SelectMisc);
            on_picked(id);
            UpdatePreview();
            PopulateTabDeck();
        });
        return w;
    }

    // Helper: Stepper control for Fine-Tuning
    QWidget* MakeStepper(const QString& label, int current_val, int min_val, int max_val, std::function<void(int)> on_change) {
        auto* w = new QWidget(deck_content_widget);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 2, 0, 2);
        l->setSpacing(8);

        auto* title = new QLabel(label, w);
        title->setFixedWidth(80);
        title->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 500; color: %1;").arg(IsCurrentThemeLight() ? "#16181D" : "#FFFFFF"));
        l->addWidget(title);

        auto* minus_btn = new QPushButton(QString::fromUtf8("−"), w);
        minus_btn->setFixedSize(28, 28);
        minus_btn->setStyleSheet(QStringLiteral("QPushButton { background: rgba(0,0,0,0.06); border: none; border-radius: 8px; font-weight: bold; }"));
        minus_btn->setEnabled(current_val > min_val);

        auto* val_lbl = new QLabel(QString::number(current_val), w);
        val_lbl->setFixedWidth(28);
        val_lbl->setAlignment(Qt::AlignCenter);
        val_lbl->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600;"));

        auto* plus_btn = new QPushButton(QString::fromUtf8("+"), w);
        plus_btn->setFixedSize(28, 28);
        plus_btn->setStyleSheet(QStringLiteral("QPushButton { background: rgba(0,0,0,0.06); border: none; border-radius: 8px; font-weight: bold; }"));
        plus_btn->setEnabled(current_val < max_val);

        connect(minus_btn, &QPushButton::clicked, this, [=] {
            UiSounds::Play(UiSounds::Name::PutDown);
            on_change(current_val - 1);
            UpdatePreview();
            PopulateTabDeck();
        });
        connect(plus_btn, &QPushButton::clicked, this, [=] {
            UiSounds::Play(UiSounds::Name::PickUp);
            on_change(current_val + 1);
            UpdatePreview();
            PopulateTabDeck();
        });

        l->addWidget(minus_btn);
        l->addWidget(val_lbl);
        l->addWidget(plus_btn);
        l->addStretch();
        return w;
    }

    // Helper: Slider control for smooth continuous values
    QWidget* MakeSlider(const QString& label, int current_val, int min_val, int max_val, std::function<void(int)> on_change) {
        auto* w = new QWidget(deck_content_widget);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 2, 0, 2);
        l->setSpacing(8);

        auto* title = new QLabel(label, w);
        title->setFixedWidth(90);
        title->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 500; color: %1;").arg(IsCurrentThemeLight() ? "#16181D" : "#FFFFFF"));
        l->addWidget(title);

        auto* slider = new QSlider(Qt::Horizontal, w);
        slider->setRange(min_val, max_val);
        slider->setValue(current_val);
        slider->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height: 4px; background: rgba(0,0,0,0.12); border-radius: 2px; }"
            "QSlider::sub-page:horizontal { background: #3C8CE0; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: #FFFFFF; border: 1px solid rgba(0,0,0,0.25); width: 16px; height: 16px; margin-top: -6px; margin-bottom: -6px; border-radius: 8px; }"));

        auto* val_lbl = new QLabel(QString::number(current_val), w);
        val_lbl->setFixedWidth(30);
        val_lbl->setAlignment(Qt::AlignCenter);
        val_lbl->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600;"));

        connect(slider, &QSlider::valueChanged, this, [=](int v) {
            val_lbl->setText(QString::number(v));
            on_change(v);
            UpdatePreview();
        });

        l->addWidget(slider, 1);
        l->addWidget(val_lbl);
        return w;
    }

    // Helper: Feature Grid with SVG Chip Thumbnails (sharp edges, 4 columns for compact 3/10 width)
    QWidget* MakeFeatureGrid(const QString& category, int count, int selected_idx, std::function<void(int)> on_selected) {
        auto* grid_widget = new QWidget(deck_content_widget);
        auto* grid = new QGridLayout(grid_widget);
        grid->setContentsMargins(0, 4, 0, 8);
        grid->setSpacing(5);

        auto* group = new QButtonGroup(grid_widget);
        const int cols = 4;
        for (int i = 0; i < count; ++i) {
            auto* btn = new QToolButton(grid_widget);
            btn->setFixedSize(48, 48);
            btn->setCheckable(true);
            btn->setCursor(Qt::PointingHandCursor);
            QPixmap thumb = RenderFeatureThumbnail(category, i, current_data, 42);
            btn->setIcon(QIcon(thumb));
            btn->setIconSize(QSize(42, 42));

            const bool is_sel = (i == selected_idx);
            const QString border_style = is_sel
                ? QStringLiteral("border: 2px solid #3C8CE0; background: rgba(60, 140, 224, 0.12);")
                : QStringLiteral("border: 1px solid rgba(0,0,0,0.12); background: rgba(0,0,0,0.02);");
            btn->setStyleSheet(QStringLiteral(
                "QToolButton { border-radius: 10px; %1 }"
                "QToolButton:hover { border: 2px solid #3C8CE0; background: rgba(60, 140, 224, 0.08); border-radius: 10px; }")
                .arg(border_style));
            group->addButton(btn, i);
            grid->addWidget(btn, i / cols, i % cols);
            if (is_sel) btn->setChecked(true);
        }

        connect(group, &QButtonGroup::idClicked, this, [on_selected, this](int id) {
            UiSounds::Play(UiSounds::Name::SelectPart);
            on_selected(id);
            UpdatePreview();
            PopulateTabDeck();
        });

        return grid_widget;
    }

    // Populate the active category in the right column deck
    void PopulateTabDeck() {
        ClearDeck();

        if (active_tab == QStringLiteral("face")) {
            deck_layout->addWidget(MakeSectionHeader(tr("Skin Color")));
            deck_layout->addWidget(MakeColorRow(GetSkinColors(), current_data.skin_color, [this](int id) {
                current_data.skin_color = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Face Shape")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("face"), 12, current_data.face_type, [this](int id) {
                current_data.face_type = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Makeup")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("makeup"), 12, current_data.makeup_type, [this](int id) {
                current_data.makeup_type = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Wrinkles")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("wrinkles"), 12, current_data.wrinkles_type, [this](int id) {
                current_data.wrinkles_type = id;
            }));
        } else if (active_tab == QStringLiteral("hair")) {
            deck_layout->addWidget(MakeSectionHeader(tr("Hair Color")));
            deck_layout->addWidget(MakeColorRow(GetHairColors(), current_data.hair_color, [this](int id) {
                current_data.hair_color = id;
                current_data.eyebrows_color = id;
                current_data.facial_hair_color = id;
            }));

            // Hair Flip toggle
            auto* flip_btn = new QPushButton(current_data.hair_flip ? tr("Flip Hair Direction: Yes") : tr("Flip Hair Direction: No"), deck_content_widget);
            flip_btn->setFixedHeight(32);
            flip_btn->setCursor(Qt::PointingHandCursor);
            flip_btn->setStyleSheet(QStringLiteral("QPushButton { background: rgba(0,0,0,0.06); border: none; border-radius: 10px; font-size: 13px; font-weight: 600; } QPushButton:hover { background: rgba(0,0,0,0.12); }"));
            connect(flip_btn, &QPushButton::clicked, this, [this] {
                UiSounds::Play(UiSounds::Name::SelectMisc);
                current_data.hair_flip = !current_data.hair_flip;
                UpdatePreview();
                PopulateTabDeck();
            });
            deck_layout->addWidget(flip_btn);

            deck_layout->addWidget(MakeSectionHeader(tr("Hairstyle")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("hair"), 132, current_data.hair_type, [this](int id) {
                current_data.hair_type = id;
            }));
        } else if (active_tab == QStringLiteral("eyes")) {
            deck_layout->addWidget(MakeSectionHeader(tr("Eye Color")));
            deck_layout->addWidget(MakeColorRow(GetEyeColors(), current_data.eyes_color, [this](int id) {
                current_data.eyes_color = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Eye Shape")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("eyes"), 60, current_data.eyes_type, [this](int id) {
                current_data.eyes_type = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Position & Fine Tuning")));
            deck_layout->addWidget(MakeStepper(tr("Height"), current_data.eyes_y, -5, 5, [this](int v) { current_data.eyes_y = v; }));
            deck_layout->addWidget(MakeStepper(tr("Spacing"), current_data.eyes_x, -5, 5, [this](int v) { current_data.eyes_x = v; }));
            deck_layout->addWidget(MakeStepper(tr("Size"), current_data.eyes_scale, -3, 3, [this](int v) { current_data.eyes_scale = v; }));
            deck_layout->addWidget(MakeStepper(tr("Rotation"), current_data.eyes_rotation, -5, 5, [this](int v) { current_data.eyes_rotation = v; }));
        } else if (active_tab == QStringLiteral("eyebrows")) {
            deck_layout->addWidget(MakeSectionHeader(tr("Eyebrow Color")));
            deck_layout->addWidget(MakeColorRow(GetHairColors(), current_data.eyebrows_color, [this](int id) {
                current_data.eyebrows_color = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Eyebrow Shape")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("eyebrows"), 24, current_data.eyebrows_type, [this](int id) {
                current_data.eyebrows_type = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Position & Fine Tuning")));
            deck_layout->addWidget(MakeStepper(tr("Height"), current_data.eyebrows_y, -5, 5, [this](int v) { current_data.eyebrows_y = v; }));
            deck_layout->addWidget(MakeStepper(tr("Spacing"), current_data.eyebrows_x, -5, 5, [this](int v) { current_data.eyebrows_x = v; }));
            deck_layout->addWidget(MakeStepper(tr("Size"), current_data.eyebrows_scale, -3, 3, [this](int v) { current_data.eyebrows_scale = v; }));
            deck_layout->addWidget(MakeStepper(tr("Rotation"), current_data.eyebrows_rotation, -5, 5, [this](int v) { current_data.eyebrows_rotation = v; }));
        } else if (active_tab == QStringLiteral("nose")) {
            deck_layout->addWidget(MakeSectionHeader(tr("Nose Shape")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("nose"), 18, current_data.nose_type, [this](int id) {
                current_data.nose_type = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Position & Size")));
            deck_layout->addWidget(MakeStepper(tr("Height"), current_data.nose_y, -5, 5, [this](int v) { current_data.nose_y = v; }));
            deck_layout->addWidget(MakeStepper(tr("Size"), current_data.nose_scale, -3, 3, [this](int v) { current_data.nose_scale = v; }));
        } else if (active_tab == QStringLiteral("mouth")) {
            deck_layout->addWidget(MakeSectionHeader(tr("Lip Color")));
            deck_layout->addWidget(MakeColorRow(GetMouthColors(), current_data.mouth_color, [this](int id) {
                current_data.mouth_color = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Mouth Shape")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("mouth"), 36, current_data.mouth_type, [this](int id) {
                current_data.mouth_type = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Position & Size")));
            deck_layout->addWidget(MakeStepper(tr("Height"), current_data.mouth_y, -5, 5, [this](int v) { current_data.mouth_y = v; }));
            deck_layout->addWidget(MakeStepper(tr("Size"), current_data.mouth_scale, -3, 3, [this](int v) { current_data.mouth_scale = v; }));
        } else if (active_tab == QStringLiteral("facial")) {
            deck_layout->addWidget(MakeSectionHeader(tr("Facial Hair Color")));
            deck_layout->addWidget(MakeColorRow(GetHairColors(), current_data.facial_hair_color, [this](int id) {
                current_data.facial_hair_color = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Mustache")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("mustache"), 6, current_data.mustache_type, [this](int id) {
                current_data.mustache_type = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Beard / Goatee")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("goatee"), 6, current_data.beard_type, [this](int id) {
                current_data.beard_type = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Glasses Color")));
            deck_layout->addWidget(MakeColorRow(GetGlassesColors(), current_data.glasses_color, [this](int id) {
                current_data.glasses_color = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Glasses Style")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("glasses"), 20, current_data.glasses_type, [this](int id) {
                current_data.glasses_type = id;
            }));
        } else if (active_tab == QStringLiteral("body")) {
            deck_layout->addWidget(MakeSectionHeader(tr("3D Facial Expression")));
            auto* expr_container = new QWidget(deck_content_widget);
            auto* expr_grid = new QGridLayout(expr_container);
            expr_grid->setContentsMargins(0, 0, 0, 0);
            expr_grid->setSpacing(6);
            const std::vector<std::pair<int, QString>> expressions = {
                {0, tr("Normal")},
                {1, tr("Smile")},
                {2, tr("Anger")},
                {3, tr("Sorrow")},
                {4, tr("Surprise")},
                {5, tr("Blink")},
                {6, tr("Wink")}
            };
            for (int i = 0; i < (int)expressions.size(); ++i) {
                const auto& [exp_id, exp_name] = expressions[i];
                auto* btn = new QPushButton(exp_name, expr_container);
                btn->setCheckable(true);
                btn->setChecked(current_data.expression == exp_id);
                btn->setFixedHeight(30);
                btn->setCursor(Qt::PointingHandCursor);
                btn->setStyleSheet(QStringLiteral(
                    "QPushButton { background: rgba(0,0,0,0.06); border: none; border-radius: 8px; font-weight: 600; font-size: 11px; padding: 0 4px; }"
                    "QPushButton:hover { background: rgba(0,0,0,0.12); }"
                    "QPushButton:checked { background: #3C8CE0; color: white; border-radius: 8px; }"));
                connect(btn, &QPushButton::clicked, this, [this, exp_id] {
                    UiSounds::Play(UiSounds::Name::SelectMisc);
                    current_data.expression = exp_id;
                    UpdatePreview();
                    PopulateTabDeck();
                });
                expr_grid->addWidget(btn, i / 3, i % 3);
            }
            deck_layout->addWidget(expr_container);

            deck_layout->addWidget(MakeSectionHeader(tr("Body Proportions")));
            deck_layout->addWidget(MakeSlider(tr("Height"), current_data.height, 0, 128, [this](int v) { current_data.height = v; }));
            deck_layout->addWidget(MakeSlider(tr("Build / Weight"), current_data.build, 0, 128, [this](int v) { current_data.build = v; }));

            deck_layout->addWidget(MakeSectionHeader(tr("Dress / Body Color")));
            deck_layout->addWidget(MakeColorRow(GetFavoriteColors(), current_data.favorite_color, [this](int id) {
                current_data.favorite_color = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("3D Shading Style")));
            auto* shader_container = new QWidget(deck_content_widget);
            auto* shader_grid = new QGridLayout(shader_container);
            shader_grid->setContentsMargins(0, 0, 0, 0);
            shader_grid->setSpacing(6);
            const std::vector<std::pair<int, QString>> shaders = {
                {0, tr("Wii U")},
                {1, tr("Switch")},
                {2, tr("Miitomo")}
            };
            for (int i = 0; i < (int)shaders.size(); ++i) {
                const auto& [sh_id, sh_name] = shaders[i];
                auto* btn = new QPushButton(sh_name, shader_container);
                btn->setCheckable(true);
                btn->setChecked(current_data.shader_style == sh_id);
                btn->setFixedHeight(30);
                btn->setCursor(Qt::PointingHandCursor);
                btn->setStyleSheet(QStringLiteral(
                    "QPushButton { background: rgba(0,0,0,0.06); border: none; border-radius: 8px; font-weight: 600; font-size: 11px; padding: 0 6px; }"
                    "QPushButton:hover { background: rgba(0,0,0,0.12); }"
                    "QPushButton:checked { background: #3C8CE0; color: white; border-radius: 8px; }"));
                connect(btn, &QPushButton::clicked, this, [this, sh_id] {
                    UiSounds::Play(UiSounds::Name::SelectMisc);
                    current_data.shader_style = sh_id;
                    UpdatePreview();
                    PopulateTabDeck();
                });
                shader_grid->addWidget(btn, i / 3, i % 3);
            }
            deck_layout->addWidget(shader_container);
        } else if (active_tab == QStringLiteral("style")) {
            deck_layout->addWidget(MakeSectionHeader(tr("Dress / Shirt Color")));
            deck_layout->addWidget(MakeColorRow(GetFavoriteColors(), current_data.favorite_color, [this](int id) {
                current_data.favorite_color = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("3D Headwear / Hat")));
            deck_layout->addWidget(MakeFeatureGrid(QStringLiteral("hat"), 10, current_data.hat_type, [this](int id) {
                current_data.hat_type = id;
            }));

            deck_layout->addWidget(MakeSectionHeader(tr("Gender")));
            auto* gender_container = new QWidget(deck_content_widget);
            auto* gender_row = new QHBoxLayout(gender_container);
            gender_row->setContentsMargins(0, 0, 0, 0);
            gender_row->setSpacing(6);
            auto* male_btn = new QPushButton(tr("Male"), gender_container);
            male_btn->setCheckable(true);
            male_btn->setChecked(current_data.gender == 0);
            male_btn->setCursor(Qt::PointingHandCursor);
            male_btn->setStyleSheet(QStringLiteral("QPushButton { background: rgba(0,0,0,0.06); border: none; border-radius: 10px; height: 32px; font-weight: 600; }"
                                                   "QPushButton:hover { background: rgba(0,0,0,0.12); }"
                                                   "QPushButton:checked { background: #3C8CE0; color: white; border-radius: 10px; }"));
            auto* female_btn = new QPushButton(tr("Female"), gender_container);
            female_btn->setCheckable(true);
            female_btn->setChecked(current_data.gender == 1);
            female_btn->setCursor(Qt::PointingHandCursor);
            female_btn->setStyleSheet(QStringLiteral("QPushButton { background: rgba(0,0,0,0.06); border: none; border-radius: 10px; height: 32px; font-weight: 600; }"
                                                     "QPushButton:hover { background: rgba(0,0,0,0.12); }"
                                                     "QPushButton:checked { background: #F55A7D; color: white; border-radius: 10px; }"));

            connect(male_btn, &QPushButton::clicked, this, [=] {
                current_data.gender = 0;
                male_btn->setChecked(true);
                female_btn->setChecked(false);
                UpdatePreview();
            });
            connect(female_btn, &QPushButton::clicked, this, [=] {
                current_data.gender = 1;
                female_btn->setChecked(true);
                male_btn->setChecked(false);
                UpdatePreview();
            });
            gender_row->addWidget(male_btn);
            gender_row->addWidget(female_btn);
            deck_layout->addWidget(gender_container);
        }

        deck_layout->addStretch();
    }

    void OnRandomize() {
        UiSounds::Play(UiSounds::Name::Sparkle);
        const QString nick = current_data.nickname;
        current_data = AvatarData::Random();
        current_data.nickname = nick;
        UpdatePreview();
        PopulateTabDeck();
    }

    void OnReset() {
        UiSounds::Play(UiSounds::Name::SelectMisc);
        const QString nick = current_data.nickname;
        current_data = AvatarData::Default();
        current_data.nickname = nick;
        UpdatePreview();
        PopulateTabDeck();
    }

    void OnExportPng() {
        UiSounds::Play(UiSounds::Name::SelectMisc);
        const QString save_path = QFileDialog::getSaveFileName(
            this, tr("Export Avatar Image"),
            QDir::homePath() + QStringLiteral("/avatar.png"),
            tr("PNG Images (*.png)"));
        if (!save_path.isEmpty()) {
            QPixmap full = RenderAvatar(current_data, 1024, false);
            full.save(save_path, "PNG");
            UiSounds::Play(UiSounds::Name::Finish);
        }
    }

    void OnExportQr() {
        UiSounds::Play(UiSounds::Name::Select);
        QImage qr_img;
        if (!MiiQR::ExportMiiQrCode(current_data, qr_img, 8)) {
            QMessageBox::warning(this, tr("Export QR Code"), tr("Failed to generate Mii QR code."));
            return;
        }

        // Show Export QR modal dialog (styled sharp, clean console UI)
        auto* dlg = new QDialog(this, Qt::Dialog | Qt::FramelessWindowHint);
        dlg->setAttribute(Qt::WA_TranslucentBackground, true);
        dlg->setModal(true);

        const bool light = IsCurrentThemeLight();
        const QString bg_col = light ? QStringLiteral("#FFFFFF") : QStringLiteral("#1E222A");
        const QString text_main = light ? QStringLiteral("#16181D") : QStringLiteral("#FFFFFF");
        const QString border_col = light ? QStringLiteral("rgba(0, 0, 0, 0.12)") : QStringLiteral("rgba(255, 255, 255, 0.12)");

        auto* root = new QVBoxLayout(dlg);
        root->setContentsMargins(16, 16, 16, 16);

        auto* box = new QWidget(dlg);
        box->setStyleSheet(QStringLiteral("background: %1; border: 1px solid %2; border-radius: 0px;").arg(bg_col, border_col));
        auto* box_layout = new QVBoxLayout(box);
        box_layout->setContentsMargins(24, 20, 24, 20);
        box_layout->setSpacing(14);
        box_layout->setAlignment(Qt::AlignCenter);

        auto* title = new QLabel(tr("Mii QR Code"), box);
        title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: %1; border: none; background: transparent;").arg(text_main));
        title->setAlignment(Qt::AlignCenter);
        box_layout->addWidget(title);

        auto* subtitle = new QLabel(current_data.nickname, box);
        subtitle->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600; color: #3C8CE0; border: none; background: transparent;"));
        subtitle->setAlignment(Qt::AlignCenter);
        box_layout->addWidget(subtitle);

        // QR Code display
        auto* qr_label = new QLabel(box);
        qr_label->setFixedSize(280, 280);
        qr_label->setScaledContents(true);
        qr_label->setPixmap(QPixmap::fromImage(qr_img));
        qr_label->setStyleSheet(QStringLiteral("border: 1px solid rgba(0,0,0,0.1); background: #FFFFFF; border-radius: 0px; padding: 4px;"));
        qr_label->setAlignment(Qt::AlignCenter);
        box_layout->addWidget(qr_label, 0, Qt::AlignCenter);

        // Buttons row
        auto* btn_layout = new QHBoxLayout();
        btn_layout->setSpacing(10);

        auto* copy_btn = new QPushButton(tr("Copy to Clipboard"), box);
        copy_btn->setFixedHeight(32);
        copy_btn->setCursor(Qt::PointingHandCursor);
        copy_btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border: none; border-radius: 14px; color: %2; font-size: 12px; font-weight: 600; padding: 0 14px; }"
            "QPushButton:hover { background: %3; }")
            .arg(light ? QStringLiteral("rgba(0,0,0,0.06)") : QStringLiteral("rgba(255,255,255,0.08)"),
                 text_main,
                 light ? QStringLiteral("rgba(0,0,0,0.12)") : QStringLiteral("rgba(255,255,255,0.14)")));
        connect(copy_btn, &QPushButton::clicked, dlg, [copy_btn, qr_img] {
            QGuiApplication::clipboard()->setImage(qr_img);
            copy_btn->setText(tr("Copied!"));
            UiSounds::Play(UiSounds::Name::Select);
        });
        btn_layout->addWidget(copy_btn);

        auto* save_btn = new QPushButton(tr("Save Image..."), box);
        save_btn->setFixedHeight(32);
        save_btn->setCursor(Qt::PointingHandCursor);
        save_btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #3C8CE0; border: none; border-radius: 14px; color: #FFFFFF; font-size: 12px; font-weight: 700; padding: 0 16px; }"
            "QPushButton:hover { background: #529FF0; }"));
        connect(save_btn, &QPushButton::clicked, dlg, [this, dlg, qr_img] {
            QString default_name = current_data.nickname.trimmed();
            if (default_name.isEmpty()) default_name = QStringLiteral("Mii");
            default_name += QStringLiteral("_QR.png");
            QString path = QFileDialog::getSaveFileName(dlg, tr("Save Mii QR Code"), default_name, tr("PNG Image (*.png)"));
            if (!path.isEmpty()) {
                if (qr_img.save(path, "PNG")) {
                    UiSounds::Play(UiSounds::Name::Notice);
                    QMessageBox::information(dlg, tr("Saved"), tr("QR Code saved successfully!"));
                } else {
                    QMessageBox::warning(dlg, tr("Error"), tr("Failed to save image."));
                }
            }
        });
        btn_layout->addWidget(save_btn);

        auto* close_btn = new QPushButton(tr("Close"), box);
        close_btn->setFixedHeight(32);
        close_btn->setCursor(Qt::PointingHandCursor);
        close_btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; border: none; border-radius: 14px; color: %2; font-size: 12px; font-weight: 600; padding: 0 14px; }"
            "QPushButton:hover { background: %3; }")
            .arg(light ? QStringLiteral("rgba(0,0,0,0.06)") : QStringLiteral("rgba(255,255,255,0.08)"),
                 text_main,
                 light ? QStringLiteral("rgba(0,0,0,0.12)") : QStringLiteral("rgba(255,255,255,0.14)")));
        connect(close_btn, &QPushButton::clicked, dlg, &QDialog::accept);
        btn_layout->addWidget(close_btn);

        box_layout->addLayout(btn_layout);
        root->addWidget(box);

        dlg->exec();
        dlg->deleteLater();
    }

    void OnImportQr() {
        UiSounds::Play(UiSounds::Name::Select);
        QString filter = tr("Mii QR Code or Data (*.png *.jpg *.jpeg *.bmp *.webp *.mii *.cfsd *.bin);;Images (*.png *.jpg *.jpeg *.bmp *.webp);;Raw Mii Files (*.mii *.cfsd *.bin);;All Files (*)");
        QString file_path = QFileDialog::getOpenFileName(this, tr("Import Mii QR Code"), QString(), filter);
        if (file_path.isEmpty()) {
            return;
        }

        AvatarData imported_data;
        QString error_msg;
        if (!MiiQR::ImportMiiFromFile(file_path, imported_data, &error_msg)) {
            UiSounds::Play(UiSounds::Name::Back);
            QMessageBox::warning(this, tr("Import Failed"), tr("Could not import Mii: %1").arg(error_msg));
            return;
        }

        ApplyImportedAvatar(imported_data);
    }

    void ApplyImportedAvatar(const AvatarData& imported_data) {
        current_data = imported_data;
        UiSounds::Play(UiSounds::Name::Notice);
        UpdatePreview();
        PopulateTabDeck();
        QMessageBox::information(this, tr("Mii Imported"),
            tr("Successfully imported Mii \"%1\"!").arg(current_data.nickname));
    }

    void OnSaveMiiToUserFolder() {
        UiSounds::Play(UiSounds::Name::Select);
        QString out_file;
        const bool ok_file = MiiQR::ExportMiiToStandardsFile(current_data, &out_file);
        const bool ok_db = MiiQR::SyncMiiToGameDatabase(current_data);

        if (ok_file || ok_db) {
            ConsoleUI::ShowAlertModal(
                this, tr("Mii Saved"),
                tr("Your Mii character was saved successfully!\n\n"
                   "• File: %1\n"
                   "• Games: Synced into emulated 3DS Mii database (CFL_DB.dat)\n\n"
                   "Any game requiring a Mii character will now automatically load it.")
                    .arg(out_file));
        } else {
            ConsoleUI::ShowAlertModal(
                this, tr("Save Error"),
                tr("Could not write Mii file to user folder."));
        }
    }

    void OnSaveAvatar() {
        UiSounds::Play(UiSounds::Name::Finish);
        QString out_png;
        SaveAvatarToUserDir(current_data, &out_png);

        // Export standard Nintendo .mii file and sync to 3DS game database
        MiiQR::ExportMiiToStandardsFile(current_data);
        MiiQR::SyncMiiToGameDatabase(current_data);

        // Update Citra UISettings
        UISettings::values.profile_avatar_path = out_png;
        UISettings::values.profile_nickname = current_data.nickname;

        if (system) {
            auto cfg = Service::CFG::GetModule(*system);
            if (cfg) {
                cfg->SetUsername(current_data.nickname.toStdU16String());
                cfg->UpdateConfigNANDSavegame();
            }
        }

        if (on_applied_cb) {
            on_applied_cb(current_data, RenderAvatar(current_data, 256, false));
        }

        closing = true;
        hide();
        if (m_loop && m_loop->isRunning()) {
            m_loop->quit();
        }
    }

private:
    Core::System* system = nullptr;
    std::function<void(const AvatarData&, const QPixmap&)> on_applied_cb;

    AvatarData current_data;
    QString active_tab{QStringLiteral("face")};

    QWidget* card_widget = nullptr;
    InteractiveAvatarPreview* avatar_preview_label = nullptr;
    QLabel* nickname_label = nullptr;
    bool m_rendering = false;
    bool m_pending_update = false;
    QWidget* tab_container = nullptr;
    QButtonGroup* tab_btn_group = nullptr;
    QScrollArea* deck_scroll = nullptr;
    QWidget* deck_content_widget = nullptr;
    QVBoxLayout* deck_layout = nullptr;

    QEventLoop* m_loop = nullptr;
    bool closing = false;
};

static bool s_is_avatar_studio_open = false;

void ShowAvatarStudioModal(QWidget* parent, Core::System* system,
                           std::function<void(const AvatarData&, const QPixmap&)> on_applied) {
    if (s_is_avatar_studio_open) {
        return;
    }
    s_is_avatar_studio_open = true;
    auto* modal = new AvatarStudioModal(parent, system, std::move(on_applied));
    modal->Exec();
    modal->deleteLater();
    s_is_avatar_studio_open = false;
}

} // namespace AvatarStudio
