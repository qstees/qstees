// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/forms/ui_overviewpage.h>

#include <qt/qsteesunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactiontablemodel.h>
#include <qt/utilitydialog.h>
#include <qt/walletmodel.h>
#include <wallet/wallet.h>
#include <validation.h>
#include <interfaces/node.h>

#include <init.h>
#include <util.h>
#include <shutdown.h>
#include <instantx.h>
#include <masternode-sync.h>
#include <infinitynodeman.h>

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>

#define ICON_OFFSET 16
#define DECORATION_SIZE 54
#define NUM_ITEMS 3
#define NUM_ITEMS_ADV 7

Q_DECLARE_METATYPE(interfaces::WalletBalances)

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(BitcoinUnits::QSTEES),
        platformStyle(_platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        mainRect.moveLeft(ICON_OFFSET);
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace - ICON_OFFSET, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->QsteesgleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool())
        {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top()+ypad+halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);

        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::separatorAlways);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include <qt/overviewpage.moc>

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    timer(nullptr),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    txdelegate(new TxViewDelegate(platformStyle, this))
{
    ui->setupUi(this);
    //ui->pushButton_Website->setIcon(QIcon(GUIUtil::getThemeImage(":/icons/website")));
    ui->pushButton_Website->setStatusTip(tr("qstees Website"));
    ui->pushButton_Discord->setStatusTip(tr("qstees Discord Channel"));
    ui->pushButton_Telegram->setStatusTip(tr("qstees Telegram Channel"));
    ui->pushButton_Twitter->setStatusTip(tr("qstees Twitter Channel"));
    ui->pushButton_Explorer->setStatusTip(tr("qstees Block Explorer"));
    ui->pushButton_Reddit->setStatusTip(tr("qstees Reddit"));
    ui->pushButton_Facebook->setStatusTip(tr("qstees Facebook"));
    ui->pushButton_Youtube->setStatusTip(tr("qstees Youtube Channel"));


    m_balances.balance = -1;

    // use a QsteesgleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->QsteesgleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(infinityNodeStat()));
    timer->start(3000);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage()
{
    if(timer) disconnect(timer, SIGNAL(timeout()), this, SLOT(privateSendStatus()));
    delete ui;
}

void OverviewPage::infinityNodeStat()
{
    std::map<COutPoint, CInfinitynode> mapInfinitynodes = infnodeman.GetFullInfinitynodeMap();
    std::map<COutPoint, CInfinitynode> mapInfinitynodesNonMatured = infnodeman.GetFullInfinitynodeNonMaturedMap();
    int total = 0, totalBIG = 0, totalMID = 0, totalLIL = 0, totalUnknown = 0;
    for (auto& infpair : mapInfinitynodes) {
        ++total;
        CInfinitynode inf = infpair.second;
        int qsteestype = inf.getQSTEESType();
        if (qsteestype == 10) ++totalBIG;
        else if (qsteestype == 5) ++totalMID;
        else if (qsteestype == 1) ++totalLIL;
    }

    int totalNonMatured = 0, totalBIGNonMatured = 0, totalMIDNonMatured = 0, totalLILNonMatured = 0, totalUnknownNonMatured = 0;
    for (auto& infpair : mapInfinitynodesNonMatured) {
        ++totalNonMatured;
        CInfinitynode inf = infpair.second;
        int qsteestype = inf.getQSTEESType();
        if (qsteestype == 10) ++totalBIGNonMatured;
        else if (qsteestype == 5) ++totalMIDNonMatured;
        else if (qsteestype == 1) ++totalLILNonMatured;
    }

    QString strTotalNodeText(tr("Total: %1 nodes (Last Scan: %2)").arg(total + totalNonMatured).arg(infnodeman.getLastScanWithLimit()));
    QString strBIGNodeText(tr("%1").arg(totalBIG));
    QString strMIDNodeText(tr("%1").arg(totalMID));
    QString strLILNodeText(tr("%1").arg(totalLIL));

    QString strBIGNodeQueuedText(tr("Queued %1").arg(totalBIGNonMatured));
    QString strMIDNodeQueuedText(tr("Queued %1").arg(totalMIDNonMatured));
    QString strLILNodeQueuedText(tr("Queued %1").arg(totalLILNonMatured));

    ui->labelStatisticTotalNode->setText(strTotalNodeText);
    ui->labelBIGNode->setText(strBIGNodeText);
    ui->labelMIDNode->setText(strMIDNodeText);
    ui->labelLILNode->setText(strLILNodeText);

    ui->labelBIGNodeQueued->setText(strBIGNodeQueuedText);
    ui->labelMIDNodeQueued->setText(strMIDNodeQueuedText);
    ui->labelLILNodeQueued->setText(strLILNodeQueuedText);

    QString strBIGNodeROIText(tr("ROI %1 days").arg(infnodeman.getRoi(10, totalBIG)));
    QString strMIDNodeROIText(tr("ROI %1 days").arg(infnodeman.getRoi(5, totalMID)));
    QString strLILNodeROIText(tr("ROI %1 days").arg(infnodeman.getRoi(1, totalLIL)));

    ui->labelBIGNodeRoi->setText(strBIGNodeROIText);
    ui->labelMIDNodeRoi->setText(strMIDNodeROIText);
    ui->labelLILNodeRoi->setText(strLILNodeROIText);

    QString strBIGNodeSTMText(tr("Statement %1").arg(infnodeman.getLastStatement(10)));
    QString strMIDNodeSTMText(tr("Statement %1").arg(infnodeman.getLastStatement(5)));
    QString strLILNodeSTMText(tr("Statement %1").arg(infnodeman.getLastStatement(1)));

    ui->labelBIGNodeSTM->setText(strBIGNodeSTMText);
    ui->labelMIDNodeSTM->setText(strMIDNodeSTMText);
    ui->labelLILNodeSTM->setText(strLILNodeSTMText);

}

void OverviewPage::setBalance(const interfaces::WalletBalances& balances)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    m_balances = balances;
    ui->labelBalance->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.balance, false, BitcoinUnits::separatorAlways));
    ui->labelUnconfirmed->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.unconfirmed_balance, false, BitcoinUnits::separatorAlways));
    ui->labelImmature->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.immature_balance, false, BitcoinUnits::separatorAlways));
    ui->labelTotal->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.balance + balances.unconfirmed_balance + balances.immature_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchAvailable->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.watch_only_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchPending->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.unconfirmed_watch_only_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchImmature->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.immature_watch_only_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchTotal->setText(BitcoinUnits::floorHtmlWithUnit(unit, balances.watch_only_balance + balances.unconfirmed_watch_only_balance + balances.immature_watch_only_balance, false, BitcoinUnits::separatorAlways));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = balances.immature_balance != 0;
    bool showWatchOnlyImmature = balances.immature_watch_only_balance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance

    static int cachedTxLocks = 0;

    if(cachedTxLocks != nCompleteTXLocks){
        cachedTxLocks = nCompleteTXLocks;
        ui->listTransactions->update();
    }
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance

    if (!showWatchOnly){
        ui->labelWatchImmature->hide();
    }
    else{
        ui->labelBalance->setIndent(20);
        ui->labelUnconfirmed->setIndent(20);
        ui->labelImmature->setIndent(20);
        ui->labelTotal->setIndent(20);
    }
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        interfaces::Wallet& wallet = model->wallet();
        interfaces::WalletBalances balances = wallet.getBalances();
        setBalance(balances);
        connect(model, SIGNAL(balanceChanged(interfaces::WalletBalances)), this, SLOT(setBalance(interfaces::WalletBalances)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateWatchOnlyLabels(wallet.haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));

        // that's it for litemode
        if(fLiteMode) return;
    }
    // update the display unit, to not use the default ("QSTEES")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if (m_balances.balance != -1) {
            setBalance(m_balances);
        }

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::SetupTransactionList(int nNumItems) {
    ui->listTransactions->setMinimumHeight(nNumItems * (DECORATION_SIZE + 2));

    if(walletModel && walletModel->getOptionsModel()) {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(walletModel->getTransactionTableModel());
        filter->setLimit(nNumItems);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);
    }
}

void OverviewPage::on_pushButton_Website_clicked() {
    QDesktopServices::openUrl(QUrl("https://qsteesovate.io/", QUrl::TolerantMode));
}
void OverviewPage::on_pushButton_Discord_clicked() {
    QDesktopServices::openUrl(QUrl("https://qsteesovate.io/links/discord", QUrl::TolerantMode));
}
void OverviewPage::on_pushButton_Telegram_clicked() {
    QDesktopServices::openUrl(QUrl("https://qsteesovate.io/links/telegram", QUrl::TolerantMode));
}
void OverviewPage::on_pushButton_Twitter_clicked() {
    QDesktopServices::openUrl(QUrl("https://qsteesovate.io/links/twitter", QUrl::TolerantMode));
}
void OverviewPage::on_pushButton_Explorer_clicked() {
    QDesktopServices::openUrl(QUrl("http://qsteesovate.io/links/explorer", QUrl::TolerantMode));
}
void OverviewPage::on_pushButton_Btctalk_clicked() {
    QDesktopServices::openUrl(QUrl("https://qsteesovate.io/links/btctalk", QUrl::TolerantMode));
}
void OverviewPage::on_pushButton_Reddit_clicked() {
    QDesktopServices::openUrl(QUrl("https://qsteesovate.io/links/reddit", QUrl::TolerantMode));
}
void OverviewPage::on_pushButton_Youtube_clicked() {
    QDesktopServices::openUrl(QUrl("https://qsteesovate.io/links/youtube", QUrl::TolerantMode));
}
void OverviewPage::on_pushButton_Facebook_clicked() {
    QDesktopServices::openUrl(QUrl("https://qsteesovate.io/links/facebook", QUrl::TolerantMode));
}