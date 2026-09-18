#include "moddecision.h"

#include <QMessageBox>
#include <QPushButton>
#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QObject>
#include <QDir>
#include <QDebug>

namespace {
// 字节数格式化为可读字符串（B/KB/MB/GB）
QString formatBytes(qint64 bytes)
{
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB")
        .arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

// 冲突弹窗（3 选项）：全部覆盖 / 全部删除旧的保留新的 / 取消。
moddecision::ConflictChoice askConflict(const QStringList &conflicts)
{
    QString list;
    for (const QString &c : conflicts)
        list += QStringLiteral("GameData/") + c + QLatin1Char('\n');
    QMessageBox box;
    box.setWindowTitle(QObject::tr("发现文件夹冲突"));
    box.setText(QObject::tr("下载完成后检查到以下文件夹已被手动安装的模组占用：\n\n%1\n\n请选择处理方式：")
                    .arg(list.trimmed()));
    QAbstractButton *allCover = box.addButton(QObject::tr("全部覆盖（保留额外文件）"), QMessageBox::AcceptRole);
    QAbstractButton *allDelete = box.addButton(QObject::tr("全部删除旧的保留新的"), QMessageBox::DestructiveRole);
    QAbstractButton *cancel = box.addButton(QObject::tr("取消"), QMessageBox::RejectRole);
    box.exec();
    QAbstractButton *clicked = box.clickedButton();
    if (clicked == cancel) return { moddecision::ConflictAction::Cancel, {} };
    if (clicked == allDelete)
        return { moddecision::ConflictAction::DeleteOld, conflicts }; // 全部删除旧的保留新的
    return { moddecision::ConflictAction::OverwriteAll, {} };        // 全部覆盖：不删除任何文件夹
}

// 可选模组勾选弹窗（Recommends / Suggests 共用）：每个模组一个复选框（默认勾选），
// 列表顶部一个"全选/全不选"切换按钮（随当前勾选状态动态切换文案）。
// cancelled 输出用户是否取消（区别于"全都不选"）。
QVector<ckan::CkanModule> askOptionalModules(const QString &title, const QString &info,
                                             const QVector<ckan::CkanModule> &modules, bool *cancelled)
{
    *cancelled = false;
    if (modules.isEmpty()) return {};

    QDialog dlg;
    dlg.setWindowTitle(title);
    dlg.setMinimumWidth(560);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QLabel *infoLabel = new QLabel(info, &dlg);
    infoLabel->setWordWrap(true);
    lay->addWidget(infoLabel);

    QScrollArea *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    QWidget *listHost = new QWidget(scroll);
    QVBoxLayout *listLay = new QVBoxLayout(listHost);
    QVector<QCheckBox*> boxes;
    // "全选/全不选"切换按钮：当前全部勾选 → 显示"全不选"，否则显示"全选"，点击后按需切换。
    QPushButton *toggleBtn = new QPushButton(listHost);
    auto updateToggle = [&]() {
        bool all = !boxes.isEmpty();
        for (const QCheckBox *cb : boxes)
            if (!cb->isChecked()) { all = false; break; }
        toggleBtn->setText(all ? QObject::tr("全不选") : QObject::tr("全选"));
        toggleBtn->setEnabled(!boxes.isEmpty());
    };
    QObject::connect(toggleBtn, &QPushButton::clicked, listHost, [&]() {
        bool all = !boxes.isEmpty();
        for (const QCheckBox *cb : boxes)
            if (!cb->isChecked()) { all = false; break; }
        for (QCheckBox *cb : boxes) cb->setChecked(!all);
        updateToggle();
    });
    listLay->addWidget(toggleBtn, 0, Qt::AlignLeft);
    for (const ckan::CkanModule &m : modules) {
        QString text = m.name + QStringLiteral("  (") + m.identifier
                     + QStringLiteral(" ") + m.version + QStringLiteral(")");
        if (!m.abstract.isEmpty()) text += QStringLiteral("\n    ") + m.abstract;
        QCheckBox *cb = new QCheckBox(text, listHost);
        cb->setChecked(true);
        boxes.append(cb);
        listLay->addWidget(cb);
    }
    updateToggle();
    listLay->addStretch();
    scroll->setWidget(listHost);
    lay->addWidget(scroll, 1);

    // 推荐/建议窗口不含"取消"：只能点"安装所选"退出，如需放弃回到启动器模组管理界面取消。
    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText(QObject::tr("安装所选"));
    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(btnBox);

    if (dlg.exec() != QDialog::Accepted) {
        *cancelled = true;
        return {};
    }
    QVector<ckan::CkanModule> sel;
    for (int i = 0; i < boxes.size(); ++i)
        if (boxes.at(i)->isChecked()) sel.append(modules.at(i));
    qInfo() << "[decide] 可选模组弹窗" << title << "：共" << boxes.size()
            << "个，用户选择" << sel.size() << "个";
    return sel;
}

// 推荐安装模组勾选弹窗（Recommends）：默认全选，用户可按需取消个别或全部。
QVector<ckan::CkanModule> askRecommends(const QVector<ckan::CkanModule> &recommends, bool *cancelled,
                                        const QString &parentName)
{
    const QString parent = parentName.isEmpty() ? QObject::tr("所选模组") : parentName;
    return askOptionalModules(QObject::tr("%1 推荐安装的模组").arg(parent),
                              QObject::tr("%1 推荐安装以下模组（Recommends），默认全部勾选，可按需选择：")
                                  .arg(parent),
                              recommends, cancelled);
}

// 建议安装模组勾选弹窗（Suggests）：默认全选，用户可按需取消个别或全部。
QVector<ckan::CkanModule> askSuggests(const QVector<ckan::CkanModule> &suggests, bool *cancelled,
                                      const QString &parentName)
{
    const QString parent = parentName.isEmpty() ? QObject::tr("所选模组") : parentName;
    return askOptionalModules(QObject::tr("%1 建议安装的模组").arg(parent),
                              QObject::tr("%1 建议安装以下模组（Suggests），默认全部勾选，可按需选择：")
                                  .arg(parent),
                              suggests, cancelled);
}

// 多提供者选择弹窗：每个虚拟包一行，用下拉框从候选提供者中选一个。
QVector<ckan::CkanModule> askProviders(const QVector<ckan::ProviderChoice> &choices, bool *cancelled)
{
    *cancelled = false;
    if (choices.isEmpty()) return {};

    QDialog dlg;
    dlg.setWindowTitle(QObject::tr("选择提供者"));
    dlg.setMinimumWidth(620);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QLabel *info = new QLabel(
        QObject::tr("以下依赖由多个模组同时提供，请为每个虚拟包选择要安装的提供者："), &dlg);
    info->setWordWrap(true);
    lay->addWidget(info);

    QScrollArea *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    QWidget *listHost = new QWidget(scroll);
    QVBoxLayout *listLay = new QVBoxLayout(listHost);

    QVector<QComboBox*> combos;
    for (const ckan::ProviderChoice &pc : choices) {
        QString head = pc.provides;
        if (!pc.requirement.isEmpty()) head += QStringLiteral("（要求：%1）").arg(pc.requirement);
        if (!pc.requiredBy.isEmpty())
            head += QStringLiteral("\n    依赖方：%1").arg(pc.requiredBy.join(QLatin1Char(',')));
        QLabel *lbl = new QLabel(head, listHost);
        lbl->setWordWrap(true);
        listLay->addWidget(lbl);

        QComboBox *combo = new QComboBox(listHost);
        for (int i = 0; i < pc.candidates.size(); ++i) {
            const ckan::CkanModule &c = pc.candidates.at(i);
            combo->addItem(QStringLiteral("%1 (%2 %3)").arg(c.name, c.identifier, c.version), i);
        }
        combos.append(combo);
        listLay->addWidget(combo);
    }
    listLay->addStretch();
    scroll->setWidget(listHost);
    lay->addWidget(scroll, 1);

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText(QObject::tr("安装所选"));
    btnBox->button(QDialogButtonBox::Cancel)->setText(QObject::tr("取消"));
    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btnBox);

    if (dlg.exec() != QDialog::Accepted) {
        *cancelled = true;
        return {};
    }
    QVector<ckan::CkanModule> sel;
    for (int i = 0; i < choices.size(); ++i) {
        const int idx = combos.at(i)->currentData().toInt();
        sel.append(choices.at(i).candidates.at(idx));
    }
    return sel;
}

// 磁盘空间不足警告弹窗：显示所需/可用空间，用户可选择"忽略并继续"或"取消"。
bool askDiskSpaceWarning(const moddecision::DiskSpacePrompt &prompt)
{
    QMessageBox box;
    box.setWindowTitle(QObject::tr("磁盘空间不足"));
    box.setText(QObject::tr("%1磁盘（%2）剩余空间不足：\n\n"
                            "    检查路径：%3\n"
                            "    所需空间：%4\n"
                            "    剩余空间：%5\n\n"
                            "是否仍要继续？")
                    .arg(prompt.forDownload ? QObject::tr("下载缓存") : QObject::tr("游戏"))
                    .arg(prompt.rootPath)
                    .arg(QDir::toNativeSeparators(prompt.path))
                    .arg(formatBytes(prompt.required))
                    .arg(formatBytes(prompt.available)));
    QPushButton *ignore = static_cast<QPushButton *>(box.addButton(QObject::tr("忽略并继续"), QMessageBox::AcceptRole));
    QPushButton *cancel = static_cast<QPushButton *>(box.addButton(QObject::tr("取消"), QMessageBox::RejectRole));
    box.setDefaultButton(cancel);
    box.exec();
    return box.clickedButton() == ignore;
}

bool askConfirm(const QString &title, const QString &message)
{
    return QMessageBox::question(nullptr, title, message,
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
}

} // namespace

namespace moddecision {

Hooks makeDefaultModDecisions()
{
    Hooks h;
    h.conflict = &askConflict;
    h.recommends = &askRecommends;
    h.suggests = &askSuggests;
    h.providers = &askProviders;
    h.diskSpace = &askDiskSpaceWarning;
    h.confirm = &askConfirm;
    return h;
}

} // namespace moddecision