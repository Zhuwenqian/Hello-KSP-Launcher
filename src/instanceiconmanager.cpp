#include "instanceiconmanager.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QFuture>
#include <QMutex>
#include <QMutexLocker>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QImageReader>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <commoncontrols.h>
#include <shellapi.h>

// IImageList 接口的 IID（commoncontrols.h 声明但未提供链接符号，这里自行按值定义）。
static const GUID k_IID_IImageList = {0x46eb5926, 0x582e, 0x4017,
                                      {0x9f, 0xdf, 0xe8, 0x99, 0x8d, 0xaa, 0x09, 0x50}};
#endif

InstanceIconManager& InstanceIconManager::instance()
{
    static InstanceIconManager s_instance;
    return s_instance;
}

InstanceIconManager::InstanceIconManager(QObject *parent)
    : QObject(parent)
{
}

// 静态图标文件在本机下的可能位置：优先随发布目录（exe 旁 instanceicons/），
// 其次源码目录（开发期直接运行）。
static QString iconFilePath(const QString& fileName)
{
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QLatin1String("/resources/instanceicons/") + fileName,
        QCoreApplication::applicationDirPath() + QLatin1String("/instanceicons/") + fileName,
        QStringLiteral("e:/Projects/Hello KSP Launcher/resources/instanceicons/") + fileName,
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    return QString();
}

// 后台线程：加载静态大图并缩放到适合列表显示的尺寸（保留透明/圆角）。
static QImage loadStaticIcon(const QString& fileName)
{
    const QString path = iconFilePath(fileName);
    if (path.isEmpty())
        return QImage();

    QImageReader reader(path);
    QImage img = reader.read();
    if (img.isNull())
        return QImage();

    // 统一缩放到约 80px 上限（后续按 40 * 设备像素比显示），避免大图占满内存/带宽。
    const int target = 80;
    if (qMax(img.width(), img.height()) > target) {
        img = img.scaled(target, target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return img;
}

#ifdef Q_OS_WIN
// 后台线程：将 HICON 转成 QImage（线程安全，仅用 GDI 绘制到 DIB）。
static QImage iconToQImage(HICON hIcon)
{
    if (!hIcon)
        return QImage();

    ICONINFO ii;
    if (!GetIconInfo(hIcon, &ii))
        return QImage();

    QImage result;
    BITMAP bm;
    if (ii.hbmColor && GetObjectW(ii.hbmColor, sizeof(BITMAP), &bm)) {
        const int w = bm.bmWidth;
        const int h = bm.bmHeight;

        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = w;
        bi.biHeight = -h;            // 自上而下（top-down）位图
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        HDC memDC = CreateCompatibleDC(nullptr);
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(memDC, reinterpret_cast<BITMAPINFO*>(&bi),
                                       DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dib && bits) {
            HGDIOBJ old = SelectObject(memDC, dib);
            DrawIconEx(memDC, 0, 0, hIcon, w, h, 0, nullptr, DI_NORMAL);
            // 32 位 DIB 每行 4 字节对齐，w*4 已是 4 的倍数。
            result = QImage(static_cast<const uchar*>(bits), w, h,
                            w * 4, QImage::Format_ARGB32_Premultiplied).copy();
            SelectObject(memDC, old);
        }
        if (dib) DeleteObject(dib);
        DeleteDC(memDC);
    }

    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask)  DeleteObject(ii.hbmMask);
    return result;
}
#endif

// 后台线程：从 KSP exe 中提取最大尺寸的图标帧（优先 256，退回 48/32）。
// 系统图像列表（SHGetFileInfo/SHGetImageList）非线程安全，多个图标同时并发提取会互相干扰，
// 因此串行化 Shell 调用（提取很快，串行开销可忽略）。
static QMutex s_shellMutex;
static QImage extractExeIconImage(const QString& exePath)
{
#ifdef Q_OS_WIN
    if (exePath.isEmpty() || !QFileInfo::exists(exePath))
        return QImage();

    QMutexLocker locker(&s_shellMutex);
    // Shell 调用建议先初始化 COM。
    HRESULT coInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    QImage result;

    // SHGetFileInfoW 要求规范的反斜杠分隔符，正斜杠路径会失败。
    const QString nativePath = QDir::toNativeSeparators(exePath);
    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW(reinterpret_cast<const wchar_t*>(nativePath.utf16()), 0,
                       &sfi, sizeof(sfi), SHGFI_SYSICONINDEX)) {
        // SHIL_JUMBO=256 → SHIL_EXTRALARGE=48 → SHIL_LARGE=32，取可用最大帧
        const int lists[] = { SHIL_JUMBO, SHIL_EXTRALARGE, SHIL_LARGE };
        for (int list : lists) {
            IImageList* il = nullptr;
            HRESULT hr = SHGetImageList(list, k_IID_IImageList,
                                        reinterpret_cast<void**>(&il));
            if (FAILED(hr) || !il)
                continue;
            HICON hIcon = nullptr;
            const bool got = SUCCEEDED(il->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon)) && hIcon;
            if (got) {
                result = iconToQImage(hIcon);
                if (!result.isNull())
                    return result;
            }
            // 该档位无图标则继续尝试更小的帧
        }
    }

    if (SUCCEEDED(coInit))
        CoUninitialize();
    return result;
#else
    Q_UNUSED(exePath)
    return QImage(); // 非 Windows 平台不支持 exe 图标提取，留空
#endif
}

InstanceIconManager::Source InstanceIconManager::resolveSource(const QString& instanceName)
{
    const QString lower = instanceName.toLower();
    // 优先级：RP-1 > RSS（Sol 是 RSS 的超级美化分支，同样使用 RSS 图标）
    if (lower.contains(QLatin1String("rp-1")))
        return Source::Rp1;
    if (lower.contains(QLatin1String("rss")) || lower.contains(QLatin1String("sol")))
        return Source::Rss;
    return Source::Exe;
}

void InstanceIconManager::requestIcon(const QString& instanceId, const QString& instanceName,
                                      const QString& exePath)
{
    const Source src = resolveSource(instanceName);

    // 打包需要加载的函数。
    const auto deliver = [this](const QString& id, const QImage& img) {
        m_pending.remove(id);
        emit iconReady(id, img);
    };

    if (src == Source::Rss) {
        const QString path = iconFilePath(QStringLiteral("RSS.png"));
        auto it = m_staticCache.constFind(path);
        if (it != m_staticCache.constEnd()) {
            deliver(instanceId, *it);
            return;
        }
        if (m_pending.contains(instanceId))
            return;
        m_pending.insert(instanceId);
        QFutureWatcher<QImage>* watcher = new QFutureWatcher<QImage>(this);
        connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, deliver, watcher, instanceId, path] {
            const QImage img = watcher->result();
            watcher->deleteLater();
            m_staticCache.insert(path, img);   // 失败也缓存空，避免重复加载
            deliver(instanceId, img);
        });
        watcher->setFuture(QtConcurrent::run([path] { return loadStaticIcon(QStringLiteral("RSS.png")); }));
        return;
    }

    if (src == Source::Rp1) {
        const QString path = iconFilePath(QStringLiteral("RP-1.png"));
        auto it = m_staticCache.constFind(path);
        if (it != m_staticCache.constEnd()) {
            deliver(instanceId, *it);
            return;
        }
        if (m_pending.contains(instanceId))
            return;
        m_pending.insert(instanceId);
        QFutureWatcher<QImage>* watcher = new QFutureWatcher<QImage>(this);
        connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, deliver, watcher, instanceId, path] {
            const QImage img = watcher->result();
            watcher->deleteLater();
            m_staticCache.insert(path, img);
            deliver(instanceId, img);
        });
        watcher->setFuture(QtConcurrent::run([path] { return loadStaticIcon(QStringLiteral("RP-1.png")); }));
        return;
    }

    // Source::Exe
    if (exePath.isEmpty()) {
        deliver(instanceId, QImage());
        return;
    }
    auto it = m_exeCache.constFind(exePath);
    if (it != m_exeCache.constEnd()) {
        deliver(instanceId, *it);
        return;
    }
    if (m_pending.contains(instanceId))
        return;
    m_pending.insert(instanceId);
    QFutureWatcher<QImage>* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, deliver, watcher, instanceId, exePath] {
        const QImage img = watcher->result();
        watcher->deleteLater();
        m_exeCache.insert(exePath, img);   // 失败也缓存，避免每帧刷新反复提取
        deliver(instanceId, img);
    });
    watcher->setFuture(QtConcurrent::run([exePath] {
        return extractExeIconImage(exePath);
    }));
}