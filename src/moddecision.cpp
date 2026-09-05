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

// 级联建议勾选弹窗：每个建议模组一个复选框（默认勾选）。
// cancelled 输出用户是否取消（区别于"全都不选"）。
QVector<ckan::CkanModule> askSuggests(const QVector<ckan::CkanModule> &suggests, bool *cancelled)
{
    *cancelled = false;
    if (suggests.isEmpty()) return {};

    QDialog dlg;
    dlg.setWindowTitle(QObject::tr("建议安装的模组"));
    dlg.setMinimumWidth(560);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QLabel *info = new QLabel(QObject::tr("以下模组为可选建议（Suggests），可按需勾选："), &dlg);
    info->setWordWrap(true);
    lay->addWidget(info);

    QScrollArea *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    QWidget *listHost = new QWidget(scroll);
    QVBoxLayout *listLay = new QVBoxLayout(listHost);
    QVector<QCheckBox*> boxes;
    for (const ckan::CkanModule &m : suggests) {
        QString text = m.name + QStringLiteral("  (") + m.identifier
                     + QStringLiteral(" ") + m.version + QStringLiteral(")");
        if (!m.abstract.isEmpty()) text += QStringLiteral("\n    ") + m.abstract;
        QCheckBox *cb = new QCheckBox(text, listHost);
        cb->setChecked(true);
        boxes.append(cb);
        listLay->addWidget(cb);
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
    for (int i = 0; i < boxes.size(); ++i)
        if (boxes.at(i)->isChecked()) sel.append(suggests.at(i));
    return sel;
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
    h.suggests = &askSuggests;
    h.providers = &askProviders;
    h.diskSpace = &askDiskSpaceWarning;
    h.confirm = &askConfirm;
    return h;
}

} // namespace moddecision