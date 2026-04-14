#include "settings-dialog.hpp"
#include "switcher.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTimer>
#include <QSplitter>
#include <obs-frontend-api.h>

namespace BitrateSwitch {

static const char *kStyleSheet =
    "QDialog {"
    "  background-color: #1e1e2e;"
    "  color: #cdd6f4;"
    "}"
    "QTreeWidget {"
    "  background-color: #181825;"
    "  color: #cdd6f4;"
    "  border: 1px solid #313244;"
    "  border-radius: 8px;"
    "  outline: none;"
    "  font-size: 13px;"
    "}"
    "QTreeWidget::item {"
    "  padding: 9px 14px;"
    "  border-radius: 4px;"
    "  margin: 1px 4px;"
    "}"
    "QTreeWidget::item:selected {"
    "  background-color: #313244;"
    "  color: #89b4fa;"
    "}"
    "QTreeWidget::item:hover:!selected {"
    "  background-color: #1e1e2e;"
    "}"
    "QTreeWidget::branch {"
    "  background-color: transparent;"
    "}"
    "QGroupBox {"
    "  background-color: #181825;"
    "  border: 1px solid #313244;"
    "  border-radius: 8px;"
    "  margin-top: 16px;"
    "  padding: 20px 16px 16px 16px;"
    "  font-weight: bold;"
    "  color: #cdd6f4;"
    "}"
    "QGroupBox::title {"
    "  subcontrol-origin: margin;"
    "  left: 12px;"
    "  padding: 0 6px;"
    "  color: #89b4fa;"
    "  font-size: 13px;"
    "}"
    "QLabel {"
    "  color: #bac2de;"
    "}"
    "QCheckBox {"
    "  color: #cdd6f4;"
    "  spacing: 8px;"
    "}"
    "QCheckBox::indicator {"
    "  width: 18px;"
    "  height: 18px;"
    "  border-radius: 4px;"
    "  border: 2px solid #585b70;"
    "  background-color: #313244;"
    "}"
    "QCheckBox::indicator:checked {"
    "  background-color: #a6e3a1;"
    "  border-color: #a6e3a1;"
    "}"
    "QSpinBox, QComboBox, QLineEdit {"
    "  background-color: #313244;"
    "  color: #cdd6f4;"
    "  border: 1px solid #45475a;"
    "  border-radius: 6px;"
    "  padding: 6px 10px;"
    "  min-height: 28px;"
    "  min-width: 220px;"
    "  selection-background-color: #89b4fa;"
    "}"
    "QSpinBox:focus, QComboBox:focus, QLineEdit:focus {"
    "  border-color: #89b4fa;"
    "}"
    "QComboBox::drop-down { border: none; padding-right: 8px; }"
    "QComboBox QAbstractItemView {"
    "  background-color: #313244;"
    "  color: #cdd6f4;"
    "  selection-background-color: #45475a;"
    "  border: 1px solid #45475a;"
    "}"
    "QPushButton {"
    "  background-color: #45475a;"
    "  color: #cdd6f4;"
    "  border: 1px solid #585b70;"
    "  border-radius: 6px;"
    "  padding: 8px 20px;"
    "  font-weight: bold;"
    "  min-height: 20px;"
    "}"
    "QPushButton:hover {"
    "  background-color: #585b70;"
    "  border-color: #89b4fa;"
    "}"
    "QPushButton:pressed { background-color: #313244; }"
    "QTableWidget {"
    "  background-color: #181825;"
    "  color: #cdd6f4;"
    "  gridline-color: #313244;"
    "  border: 1px solid #313244;"
    "  border-radius: 6px;"
    "  selection-background-color: #45475a;"
    "}"
    "QTableWidget::item { padding: 4px; }"
    "QHeaderView::section {"
    "  background-color: #313244;"
    "  color: #89b4fa;"
    "  border: 1px solid #45475a;"
    "  padding: 6px;"
    "  font-weight: bold;"
    "}"
    "QScrollBar:vertical {"
    "  background-color: #1e1e2e;"
    "  width: 10px;"
    "  border-radius: 5px;"
    "}"
    "QScrollBar::handle:vertical {"
    "  background-color: #45475a;"
    "  border-radius: 5px;"
    "  min-height: 20px;"
    "}"
    "QStackedWidget { background-color: transparent; }"
    "QSplitter::handle { background-color: #313244; width: 1px; }";

// ────────────────────────────────────────────────────────────
// Construction
// ────────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(Config *config, Switcher *switcher, QWidget *parent)
    : QDialog(parent)
    , config_(config)
    , switcher_(switcher)
{
    setWindowTitle("Bitrate Scene Switch Settings");
    setMinimumSize(880, 640);
    resize(920, 680);
    setStyleSheet(kStyleSheet);

    setupUI();
    loadSettings();

    statusTimer_ = new QTimer(this);
    connect(statusTimer_, &QTimer::timeout, this, &SettingsDialog::refreshStatus);
    statusTimer_->start(1000);
    refreshStatus();
}

SettingsDialog::~SettingsDialog()
{
    statusTimer_->stop();
}

// ────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────

QWidget *SettingsDialog::wrapInScrollArea(QWidget *content)
{
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background-color: transparent; border: none; }");
    scroll->setWidget(content);
    return scroll;
}

void SettingsDialog::populateServerTypeCombo(QComboBox *combo)
{
    combo->addItem("IRLHosting",        static_cast<int>(ServerType::IrlHosting));
    combo->addItem("BELABOX",           static_cast<int>(ServerType::Belabox));
    combo->addItem("NGINX",             static_cast<int>(ServerType::Nginx));
    combo->addItem("SRT Live Server",   static_cast<int>(ServerType::SrtLiveServer));
    combo->addItem("MediaMTX",          static_cast<int>(ServerType::Mediamtx));
    combo->addItem("Node Media Server", static_cast<int>(ServerType::NodeMediaServer));
    combo->addItem("Nimble",            static_cast<int>(ServerType::Nimble));
    combo->addItem("RIST",              static_cast<int>(ServerType::Rist));
    combo->addItem("OpenIRL",           static_cast<int>(ServerType::OpenIRL));
    combo->addItem("Xiu",              static_cast<int>(ServerType::Xiu));
}

void SettingsDialog::populateSceneComboBox(QComboBox *combo, bool allowEmpty)
{
    combo->clear();
    if (allowEmpty)
        combo->addItem("(None)", "");

    obs_frontend_source_list scenes = {};
    obs_frontend_get_scenes(&scenes);
    for (size_t i = 0; i < scenes.sources.num; i++) {
        const char *name = obs_source_get_name(scenes.sources.array[i]);
        if (name)
            combo->addItem(QString::fromUtf8(name), QString::fromUtf8(name));
    }
    obs_frontend_source_list_free(&scenes);
}

// ────────────────────────────────────────────────────────────
// Main layout: sidebar + content
// ────────────────────────────────────────────────────────────

void SettingsDialog::setupUI()
{
    QVBoxLayout *root = new QVBoxLayout(this);

    // Status bar
    QGroupBox *statusGroup = new QGroupBox("Status", this);
    statusGroup->setStyleSheet(
        "QGroupBox { background-color: #11111b; border: 1px solid #313244; }"
        "QGroupBox::title { color: #f9e2af; }");
    QHBoxLayout *statusLayout = new QHBoxLayout(statusGroup);
    statusLabel_ = new QLabel("Status: Unknown", this);
    statusLabel_->setStyleSheet("color: #a6e3a1; font-weight: bold; font-size: 13px;");
    bitrateLabel_ = new QLabel("Bitrate: --", this);
    bitrateLabel_->setStyleSheet("color: #89dceb; font-weight: bold; font-size: 13px;");
    statusLayout->addWidget(statusLabel_);
    statusLayout->addWidget(bitrateLabel_);
    statusLayout->addStretch();
    root->addWidget(statusGroup);

    // Splitter: sidebar | content
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // --- sidebar ---
    QWidget *sidebarWidget = new QWidget();
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(6);

    navTree_ = new QTreeWidget();
    navTree_->setHeaderHidden(true);
    navTree_->setRootIsDecorated(true);
    navTree_->setIndentation(16);
    navTree_->setMinimumWidth(180);
    navTree_->setMaximumWidth(230);
    navTree_->setFocusPolicy(Qt::NoFocus);
    navTree_->setAnimated(true);

    auto addNavItem = [&](const QString &label) -> QTreeWidgetItem * {
        auto *item = new QTreeWidgetItem(navTree_, {label});
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    };

    addNavItem("General");
    addNavItem("Triggers");
    addNavItem("Scenes");

    serversParent_ = addNavItem("Servers");
    serversParent_->setExpanded(true);

    addNavItem("Chat");
    addNavItem("Commands");
    addNavItem("Messages");
    addNavItem("Advanced");

    sidebarLayout->addWidget(navTree_);

    // server add/remove buttons
    QHBoxLayout *srvBtnRow = new QHBoxLayout();
    QPushButton *addSrvBtn = new QPushButton("+ Add");
    addSrvBtn->setStyleSheet(
        "QPushButton { background-color: #a6e3a1; color: #1e1e2e; border: none; padding: 6px 12px; font-size: 12px; }"
        "QPushButton:hover { background-color: #94e2d5; }");
    QPushButton *rmSrvBtn = new QPushButton("- Remove");
    rmSrvBtn->setStyleSheet(
        "QPushButton { background-color: #f38ba8; color: #1e1e2e; border: none; padding: 6px 12px; font-size: 12px; }"
        "QPushButton:hover { background-color: #eba0ac; }");
    srvBtnRow->addWidget(addSrvBtn);
    srvBtnRow->addWidget(rmSrvBtn);
    srvBtnRow->addStretch();
    sidebarLayout->addLayout(srvBtnRow);

    connect(addSrvBtn, &QPushButton::clicked, this, [this]() { addServerEntry(); });
    connect(rmSrvBtn, &QPushButton::clicked, this, &SettingsDialog::removeSelectedServer);

    splitter->addWidget(sidebarWidget);

    // --- content ---
    contentStack_ = new QStackedWidget();
    contentStack_->addWidget(wrapInScrollArea(createGeneralPage()));   // 0
    contentStack_->addWidget(wrapInScrollArea(createTriggersPage()));  // 1
    contentStack_->addWidget(wrapInScrollArea(createScenesPage()));    // 2
    // index 3 is reserved — "Servers" parent click shows first server or empty
    QLabel *emptyServers = new QLabel("Add a server using the + button below the sidebar.");
    emptyServers->setAlignment(Qt::AlignCenter);
    emptyServers->setStyleSheet("color: #585b70; font-size: 14px;");
    contentStack_->addWidget(emptyServers);                           // 3
    contentStack_->addWidget(wrapInScrollArea(createChatPage()));      // 4
    contentStack_->addWidget(wrapInScrollArea(createCommandsPage()));  // 5
    contentStack_->addWidget(wrapInScrollArea(createMessagesPage()));  // 6
    contentStack_->addWidget(wrapInScrollArea(createAdvancedPage()));  // 7

    splitter->addWidget(contentStack_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    connect(navTree_, &QTreeWidget::itemClicked, this, &SettingsDialog::onNavItemClicked);

    // Dialog buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *applyBtn = new QPushButton("Apply", this);
    applyBtn->setStyleSheet(
        "QPushButton { background-color: #89b4fa; color: #1e1e2e; border: none; padding: 10px 30px; font-size: 14px; }"
        "QPushButton:hover { background-color: #74c7ec; }"
        "QPushButton:pressed { background-color: #89dceb; }");
    connect(applyBtn, &QPushButton::clicked, this, &SettingsDialog::onApply);

    QPushButton *saveBtn = new QPushButton("Save", this);
    saveBtn->setStyleSheet(
        "QPushButton { background-color: #a6e3a1; color: #1e1e2e; border: none; padding: 10px 30px; font-size: 14px; }"
        "QPushButton:hover { background-color: #94e2d5; }"
        "QPushButton:pressed { background-color: #74c7ec; }");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);

    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #f38ba8; color: #1e1e2e; border: none; padding: 10px 30px; font-size: 14px; }"
        "QPushButton:hover { background-color: #eba0ac; }"
        "QPushButton:pressed { background-color: #f2cdcd; }");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    root->addLayout(btnLayout);

    // Update notification
    updateLabel_ = new QLabel(this);
    updateLabel_->setStyleSheet(
        "QLabel { background-color: #f9e2af; color: #1e1e2e; padding: 10px; border-radius: 6px; font-weight: bold; }");
    updateLabel_->setOpenExternalLinks(true);
    updateLabel_->setAlignment(Qt::AlignCenter);
    updateLabel_->setVisible(false);
    root->addWidget(updateLabel_);

    // Branding
    QLabel *branding = new QLabel(this);
    branding->setText(
        QString("<div style='text-align: center; color: #585b70;'>"
        "Bitrate Scene Switch v%1 | Powered by "
        "<a href='https://irlhosting.com' style='color: #cba6f7;'>IRLHosting.com</a>"
        "</div>").arg(QString::fromStdString(UpdateChecker::getCurrentVersion())));
    branding->setOpenExternalLinks(true);
    branding->setAlignment(Qt::AlignCenter);
    root->addWidget(branding);

    updateChecker_.checkForUpdates([this](const UpdateInfo &info) {
        if (info.hasUpdate) {
            QMetaObject::invokeMethod(this, [this, info]() {
                updateLabel_->setText(
                    QString("Update available: v%1 \u2192 v%2  |  "
                    "<a href='https://github.com/sniffingpickles/BitrateSceneSwitch/releases/latest' "
                    "style='color: #1e1e2e; text-decoration: underline;'>Download Now</a>")
                    .arg(info.currentVersion.c_str()).arg(info.latestVersion.c_str()));
                updateLabel_->setVisible(true);
            }, Qt::QueuedConnection);
        }
    });

    navTree_->setCurrentItem(navTree_->topLevelItem(0));
    contentStack_->setCurrentIndex(0);
}

// ────────────────────────────────────────────────────────────
// Navigation
// ────────────────────────────────────────────────────────────

void SettingsDialog::onNavItemClicked(QTreeWidgetItem *item, int)
{
    if (!item)
        return;

    // Check if it's a server child item
    if (item->parent() == serversParent_) {
        for (const auto &sp : serverPages_) {
            if (sp.treeItem == item) {
                contentStack_->setCurrentIndex(sp.stackIndex);
                return;
            }
        }
        return;
    }

    // Top-level nav items map to fixed indices:
    // General=0, Triggers=1, Scenes=2, Servers=3, Chat=4, Commands=5, Messages=6, Advanced=7
    int tlIndex = navTree_->indexOfTopLevelItem(item);
    if (tlIndex < 0)
        return;

    if (item == serversParent_) {
        if (!serverPages_.empty()) {
            contentStack_->setCurrentIndex(serverPages_.front().stackIndex);
            navTree_->setCurrentItem(serverPages_.front().treeItem);
        } else {
            contentStack_->setCurrentIndex(3);
        }
        return;
    }

    // Map top-level index to stack index, skipping servers parent
    // 0=General, 1=Triggers, 2=Scenes, 3=Servers(parent), 4=Chat, 5=Commands, 6=Messages, 7=Advanced
    static const int kPageMap[] = {0, 1, 2, 3, 4, 5, 6, 7};
    if (tlIndex >= 0 && tlIndex < 8)
        contentStack_->setCurrentIndex(kPageMap[tlIndex]);
}

// ────────────────────────────────────────────────────────────
// Server management
// ────────────────────────────────────────────────────────────

void SettingsDialog::addServerEntry(const StreamServerConfig *initial)
{
    QWidget *page = createServerPage(initial);
    int stackIdx = contentStack_->addWidget(wrapInScrollArea(page));

    ServerPage sp;
    sp.widget = page;
    sp.stackIndex = stackIdx;

    // grab widgets from the page (they are the only children of their type in the form)
    sp.enabledCheck  = page->findChild<QCheckBox *>("srv_enabled");
    sp.typeCombo     = page->findChild<QComboBox *>("srv_type");
    sp.nameEdit      = page->findChild<QLineEdit *>("srv_name");
    sp.urlEdit       = page->findChild<QLineEdit *>("srv_url");
    sp.keyEdit       = page->findChild<QLineEdit *>("srv_key");
    sp.prioritySpin  = page->findChild<QSpinBox *>("srv_priority");

    QString label = initial ? QString::fromStdString(initial->name) : "New Server";
    sp.treeItem = new QTreeWidgetItem(serversParent_, {label});
    sp.treeItem->setFlags(sp.treeItem->flags() & ~Qt::ItemIsEditable);
    serversParent_->setExpanded(true);

    // keep sidebar label in sync with the name field
    connect(sp.nameEdit, &QLineEdit::textChanged, this, [item = sp.treeItem](const QString &text) {
        item->setText(0, text.isEmpty() ? "Unnamed" : text);
    });

    serverPages_.push_back(sp);

    contentStack_->setCurrentIndex(stackIdx);
    navTree_->setCurrentItem(sp.treeItem);
}

void SettingsDialog::removeSelectedServer()
{
    QTreeWidgetItem *cur = navTree_->currentItem();
    if (!cur || cur->parent() != serversParent_)
        return;

    for (auto it = serverPages_.begin(); it != serverPages_.end(); ++it) {
        if (it->treeItem == cur) {
            contentStack_->removeWidget(contentStack_->widget(it->stackIndex));
            serversParent_->removeChild(cur);
            delete cur;

            // re-index everything after removal
            serverPages_.erase(it);
            for (size_t i = 0; i < serverPages_.size(); i++)
                serverPages_[i].stackIndex = contentStack_->indexOf(
                    contentStack_->widget(contentStack_->indexOf(
                        serverPages_[i].widget->parentWidget())));

            if (!serverPages_.empty()) {
                contentStack_->setCurrentIndex(serverPages_.back().stackIndex);
                navTree_->setCurrentItem(serverPages_.back().treeItem);
            } else {
                contentStack_->setCurrentIndex(3);
                navTree_->setCurrentItem(serversParent_);
            }
            break;
        }
    }
}

// ────────────────────────────────────────────────────────────
// Page builders
// ────────────────────────────────────────────────────────────

QWidget *SettingsDialog::createGeneralPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *group = new QGroupBox("Switcher Settings", page);
    QFormLayout *form = new QFormLayout(group);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    enabledCheckbox_ = new QCheckBox("Enable automatic scene switching", page);
    onlyWhenStreamingCheckbox_ = new QCheckBox("Only switch when streaming", page);
    instantRecoverCheckbox_ = new QCheckBox("Instantly switch on bitrate recovery", page);
    autoNotifyCheckbox_ = new QCheckBox("Enable auto-switch notifications", page);

    retryAttemptsSpinBox_ = new QSpinBox(page);
    retryAttemptsSpinBox_->setRange(1, 30);
    retryAttemptsSpinBox_->setValue(5);
    retryAttemptsSpinBox_->setToolTip("Number of consecutive checks before switching");

    form->addRow(enabledCheckbox_);
    form->addRow(onlyWhenStreamingCheckbox_);
    form->addRow(instantRecoverCheckbox_);
    form->addRow(autoNotifyCheckbox_);
    form->addRow("Retry Attempts:", retryAttemptsSpinBox_);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createTriggersPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *group = new QGroupBox("Bitrate Triggers", page);
    QFormLayout *form = new QFormLayout(group);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    lowBitrateSpinBox_ = new QSpinBox(page);
    lowBitrateSpinBox_->setRange(0, 50000);
    lowBitrateSpinBox_->setValue(800);
    lowBitrateSpinBox_->setSuffix(" kbps");

    rttThresholdSpinBox_ = new QSpinBox(page);
    rttThresholdSpinBox_->setRange(0, 10000);
    rttThresholdSpinBox_->setValue(2500);
    rttThresholdSpinBox_->setSuffix(" ms");

    offlineBitrateSpinBox_ = new QSpinBox(page);
    offlineBitrateSpinBox_->setRange(0, 10000);
    offlineBitrateSpinBox_->setValue(0);
    offlineBitrateSpinBox_->setSuffix(" kbps");

    rttOfflineSpinBox_ = new QSpinBox(page);
    rttOfflineSpinBox_->setRange(0, 30000);
    rttOfflineSpinBox_->setValue(0);
    rttOfflineSpinBox_->setSuffix(" ms");

    form->addRow("Low Bitrate Threshold:", lowBitrateSpinBox_);
    form->addRow("RTT Threshold (Low):", rttThresholdSpinBox_);
    form->addRow("Offline Bitrate Threshold:", offlineBitrateSpinBox_);
    form->addRow("RTT Threshold (Offline):", rttOfflineSpinBox_);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createScenesPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *mainGrp = new QGroupBox("Switching Scenes", page);
    QFormLayout *mainForm = new QFormLayout(mainGrp);
    mainForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    normalSceneCombo_ = new QComboBox(page);
    lowSceneCombo_ = new QComboBox(page);
    offlineSceneCombo_ = new QComboBox(page);
    populateSceneComboBox(normalSceneCombo_);
    populateSceneComboBox(lowSceneCombo_);
    populateSceneComboBox(offlineSceneCombo_);

    mainForm->addRow("Normal Scene:", normalSceneCombo_);
    mainForm->addRow("Low Bitrate Scene:", lowSceneCombo_);
    mainForm->addRow("Offline Scene:", offlineSceneCombo_);
    layout->addWidget(mainGrp);

    QGroupBox *optGrp = new QGroupBox("Optional Scenes", page);
    QFormLayout *optForm = new QFormLayout(optGrp);
    optForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    startingSceneCombo_ = new QComboBox(page);
    endingSceneCombo_ = new QComboBox(page);
    privacySceneCombo_ = new QComboBox(page);
    refreshSceneCombo_ = new QComboBox(page);
    populateSceneComboBox(startingSceneCombo_, true);
    populateSceneComboBox(endingSceneCombo_, true);
    populateSceneComboBox(privacySceneCombo_, true);
    populateSceneComboBox(refreshSceneCombo_, true);

    optForm->addRow("Starting Scene:", startingSceneCombo_);
    optForm->addRow("Ending Scene:", endingSceneCombo_);
    optForm->addRow("Privacy Scene:", privacySceneCombo_);
    optForm->addRow("Refresh Scene:", refreshSceneCombo_);
    layout->addWidget(optGrp);

    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createServerPage(const StreamServerConfig *initial)
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *group = new QGroupBox("Server Configuration", page);
    QFormLayout *form = new QFormLayout(group);
    form->setVerticalSpacing(14);
    form->setContentsMargins(20, 30, 20, 20);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    QCheckBox *enabled = new QCheckBox("Enabled", page);
    enabled->setObjectName("srv_enabled");
    enabled->setChecked(initial ? initial->enabled : true);

    QComboBox *type = new QComboBox(page);
    type->setObjectName("srv_type");
    populateServerTypeCombo(type);
    if (initial) {
        for (int i = 0; i < type->count(); i++) {
            if (type->itemData(i).toInt() == static_cast<int>(initial->type)) {
                type->setCurrentIndex(i);
                break;
            }
        }
    }

    QLineEdit *name = new QLineEdit(page);
    name->setObjectName("srv_name");
    name->setPlaceholderText("e.g. My SRT Server");
    if (initial) name->setText(QString::fromStdString(initial->name));

    QLineEdit *url = new QLineEdit(page);
    url->setObjectName("srv_url");
    url->setPlaceholderText("http://localhost:8181/stats");
    if (initial) url->setText(QString::fromStdString(initial->statsUrl));

    QLineEdit *key = new QLineEdit(page);
    key->setObjectName("srv_key");
    key->setPlaceholderText("e.g. publish/live/feed1");
    if (initial) key->setText(QString::fromStdString(initial->key));

    QSpinBox *priority = new QSpinBox(page);
    priority->setObjectName("srv_priority");
    priority->setRange(0, 100);
    priority->setToolTip("Higher priority servers are checked first");
    if (initial) priority->setValue(initial->priority);

    form->addRow(enabled);
    form->addRow("Server Type:", type);
    form->addRow("Name:", name);
    form->addRow("Stats URL:", url);
    form->addRow("Publisher:", key);
    form->addRow("Priority:", priority);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createChatPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *connGroup = new QGroupBox("Chat Connection", page);
    QFormLayout *connForm = new QFormLayout(connGroup);
    connForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    connForm->setVerticalSpacing(10);
    connForm->setContentsMargins(12, 24, 12, 12);

    chatEnabledCheckbox_ = new QCheckBox("Enable chat integration", page);
    chatPlatformCombo_ = new QComboBox(page);
    chatPlatformCombo_->addItems({"Twitch", "Kick"});

    chatChannelEdit_ = new QLineEdit(page);
    chatChannelEdit_->setPlaceholderText("channel login or Kick slug");

    chatBotUsernameEdit_ = new QLineEdit(page);
    chatBotUsernameEdit_->setPlaceholderText("(optional - uses channel name if empty)");
    chatOauthEdit_ = new QLineEdit(page);
    chatOauthEdit_->setEchoMode(QLineEdit::Password);
    chatOauthEdit_->setPlaceholderText("oauth:xxxxxxxxxxxxxx");

    kickChannelIdEdit_ = new QLineEdit(page);
    kickChannelIdEdit_->setPlaceholderText("numeric channel id");
    kickChatroomIdEdit_ = new QLineEdit(page);
    kickChatroomIdEdit_->setPlaceholderText("numeric chatroom id");

    twitchBotLabel_ = new QLabel("Bot Username:", page);
    twitchOauthLabel_ = new QLabel("OAuth Token:", page);
    kickChannelLabel_ = new QLabel("Channel ID:", page);
    kickChatroomLabel_ = new QLabel("Chatroom ID:", page);

    connForm->addRow(chatEnabledCheckbox_);
    connForm->addRow("Platform:", chatPlatformCombo_);
    connForm->addRow("Channel:", chatChannelEdit_);
    connForm->addRow(twitchBotLabel_, chatBotUsernameEdit_);
    connForm->addRow(twitchOauthLabel_, chatOauthEdit_);
    connForm->addRow(kickChannelLabel_, kickChannelIdEdit_);
    connForm->addRow(kickChatroomLabel_, kickChatroomIdEdit_);
    connect(chatPlatformCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::updateChatPlatformUi);
    layout->addWidget(connGroup);

    QGroupBox *permGroup = new QGroupBox("Permissions", page);
    QFormLayout *permForm = new QFormLayout(permGroup);
    permForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    permForm->setVerticalSpacing(10);
    permForm->setContentsMargins(12, 24, 12, 12);

    chatAdminsEdit_ = new QLineEdit(page);
    chatAdminsEdit_->setPlaceholderText("user1, user2 (empty = channel owner only)");
    chatAnnounceCheckbox_ = new QCheckBox("Announce scene changes in chat", page);
    chatAutoStopRaidCheckbox_ = new QCheckBox("Stop stream when raiding / hosting out", page);
    chatAnnounceRaidStopCheckbox_ = new QCheckBox("Announce raid stop in chat (Twitch only)", page);

    permForm->addRow("Allowed Users:", chatAdminsEdit_);
    permForm->addRow(chatAnnounceCheckbox_);
    permForm->addRow(chatAutoStopRaidCheckbox_);
    permForm->addRow(chatAnnounceRaidStopCheckbox_);
    layout->addWidget(permGroup);

    updateChatPlatformUi();

    QGroupBox *cmdInfo = new QGroupBox("Default Commands", page);
    QVBoxLayout *cmdLay = new QVBoxLayout(cmdInfo);
    cmdLay->addWidget(new QLabel(
        "Default chat commands (customizable in Commands):\n"
        "  !live  - Switch to Live scene\n"
        "  !low   - Switch to Low scene\n"
        "  !brb   - Switch to BRB/Offline scene\n"
        "  !privacy - Switch to Privacy scene\n"
        "  !s <name> - Switch to any scene (!ss also works)\n"
        "  !refresh / !fix / !status / !trigger / !start / !stop", page));
    layout->addWidget(cmdInfo);

    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createCommandsPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    QLabel *hint = new QLabel("Customize the chat command triggers.", page);
    hint->setStyleSheet("color: #a6adc8; font-size: 11px; padding: 4px;");
    layout->addWidget(hint);

    QGroupBox *scGrp = new QGroupBox("Scene Commands", page);
    QFormLayout *scForm = new QFormLayout(scGrp);
    scForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    scForm->setVerticalSpacing(10);
    scForm->setContentsMargins(12, 24, 12, 12);
    cmdLiveEdit_ = new QLineEdit(page);
    cmdLowEdit_ = new QLineEdit(page);
    cmdBrbEdit_ = new QLineEdit(page);
    cmdPrivacyEdit_ = new QLineEdit(page);
    cmdSwitchSceneEdit_ = new QLineEdit(page);
    scForm->addRow("Live:", cmdLiveEdit_);
    scForm->addRow("Low:", cmdLowEdit_);
    scForm->addRow("BRB:", cmdBrbEdit_);
    scForm->addRow("Privacy:", cmdPrivacyEdit_);
    scForm->addRow("Switch Scene:", cmdSwitchSceneEdit_);
    layout->addWidget(scGrp);

    QGroupBox *acGrp = new QGroupBox("Action Commands", page);
    QFormLayout *acForm = new QFormLayout(acGrp);
    acForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    acForm->setVerticalSpacing(10);
    acForm->setContentsMargins(12, 24, 12, 12);
    cmdRefreshEdit_ = new QLineEdit(page);
    cmdStatusEdit_ = new QLineEdit(page);
    cmdTriggerEdit_ = new QLineEdit(page);
    cmdFixEdit_ = new QLineEdit(page);
    cmdStartEdit_ = new QLineEdit(page);
    cmdStopEdit_ = new QLineEdit(page);
    acForm->addRow("Refresh:", cmdRefreshEdit_);
    acForm->addRow("Status:", cmdStatusEdit_);
    acForm->addRow("Trigger:", cmdTriggerEdit_);
    acForm->addRow("Fix:", cmdFixEdit_);
    acForm->addRow("Start:", cmdStartEdit_);
    acForm->addRow("Stop:", cmdStopEdit_);
    layout->addWidget(acGrp);

    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::createMessagesPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    QGroupBox *refGrp = new QGroupBox("Available Placeholders", page);
    QVBoxLayout *refLay = new QVBoxLayout(refGrp);
    QLabel *refLabel = new QLabel(
        "<span style='color: #89b4fa;'>{bitrate}</span> - Current bitrate&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{rtt}</span> - RTT&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{scene}</span> - Current scene&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{prev_scene}</span> - Previous scene<br>"
        "<span style='color: #89b4fa;'>{server}</span> - Active server&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{status}</span> - Online/Offline&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{target}</span> - Raid target", page);
    refLabel->setWordWrap(true);
    refLabel->setStyleSheet("color: #a6adc8; font-size: 11px; padding: 4px;");
    refLay->addWidget(refLabel);
    layout->addWidget(refGrp);

    QGroupBox *autoGrp = new QGroupBox("Auto-Switch Messages", page);
    QFormLayout *autoForm = new QFormLayout(autoGrp);
    autoForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    autoForm->setVerticalSpacing(10);
    autoForm->setContentsMargins(12, 24, 12, 12);
    msgSwitchedLiveEdit_ = new QLineEdit(page);
    msgSwitchedLowEdit_ = new QLineEdit(page);
    msgSwitchedOfflineEdit_ = new QLineEdit(page);
    autoForm->addRow("Switched to Live:", msgSwitchedLiveEdit_);
    autoForm->addRow("Switched to Low:", msgSwitchedLowEdit_);
    autoForm->addRow("Switched to Offline:", msgSwitchedOfflineEdit_);
    layout->addWidget(autoGrp);

    QGroupBox *cmdGrp = new QGroupBox("Command Response Messages", page);
    QFormLayout *cmdForm = new QFormLayout(cmdGrp);
    cmdForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    cmdForm->setVerticalSpacing(10);
    cmdForm->setContentsMargins(12, 24, 12, 12);
    msgStatusEdit_ = new QLineEdit(page);
    msgStatusOfflineEdit_ = new QLineEdit(page);
    msgRefreshingEdit_ = new QLineEdit(page);
    msgFixEdit_ = new QLineEdit(page);
    msgStreamStartedEdit_ = new QLineEdit(page);
    msgStreamStoppedEdit_ = new QLineEdit(page);
    msgRaidStopEdit_ = new QLineEdit(page);
    msgSceneSwitchedEdit_ = new QLineEdit(page);
    cmdForm->addRow("Status (online):", msgStatusEdit_);
    cmdForm->addRow("Status (offline):", msgStatusOfflineEdit_);
    cmdForm->addRow("Refreshing:", msgRefreshingEdit_);
    cmdForm->addRow("Fix attempt:", msgFixEdit_);
    cmdForm->addRow("Stream started:", msgStreamStartedEdit_);
    cmdForm->addRow("Stream stopped:", msgStreamStoppedEdit_);
    cmdForm->addRow("Raid stop:", msgRaidStopEdit_);
    cmdForm->addRow("Scene switched:", msgSceneSwitchedEdit_);
    layout->addWidget(cmdGrp);

    // Custom commands (kept as a table -- it's the right widget for this)
    QGroupBox *customGrp = new QGroupBox("Custom Commands", page);
    QVBoxLayout *customLay = new QVBoxLayout(customGrp);
    customLay->setContentsMargins(12, 24, 12, 12);

    customCmdTable_ = new QTableWidget(0, 3, page);
    customCmdTable_->setHorizontalHeaderLabels({"Enabled", "Trigger", "Response"});
    customCmdTable_->verticalHeader()->setVisible(false);
    customCmdTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    customCmdTable_->setColumnWidth(0, 60);
    customCmdTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    customCmdTable_->setColumnWidth(1, 120);
    customCmdTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    customCmdTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    customCmdTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    customCmdTable_->setMinimumHeight(120);
    customLay->addWidget(customCmdTable_);

    QHBoxLayout *cBtnLay = new QHBoxLayout();
    addCustomCmdBtn_ = new QPushButton("+ Add Command", page);
    addCustomCmdBtn_->setStyleSheet(
        "QPushButton { background-color: #a6e3a1; color: #1e1e2e; border: none; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #94e2d5; }");
    removeCustomCmdBtn_ = new QPushButton("Remove", page);
    removeCustomCmdBtn_->setStyleSheet(
        "QPushButton { background-color: #f38ba8; color: #1e1e2e; border: none; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #eba0ac; }");
    cBtnLay->addWidget(addCustomCmdBtn_);
    cBtnLay->addWidget(removeCustomCmdBtn_);
    cBtnLay->addStretch();
    customLay->addLayout(cBtnLay);

    connect(addCustomCmdBtn_, &QPushButton::clicked, this, [this]() {
        int row = customCmdTable_->rowCount();
        customCmdTable_->insertRow(row);
        QCheckBox *chk = new QCheckBox();
        chk->setChecked(true);
        customCmdTable_->setCellWidget(row, 0, chk);
        customCmdTable_->setItem(row, 1, new QTableWidgetItem("!mycommand"));
        customCmdTable_->setItem(row, 2, new QTableWidgetItem("Bitrate: {bitrate} kbps"));
    });
    connect(removeCustomCmdBtn_, &QPushButton::clicked, this, [this]() {
        int row = customCmdTable_->currentRow();
        if (row >= 0) customCmdTable_->removeRow(row);
    });

    layout->addWidget(customGrp);
    return page;
}

QWidget *SettingsDialog::createAdvancedPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    QGroupBox *group = new QGroupBox("Advanced Options", page);
    QFormLayout *form = new QFormLayout(group);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    offlineTimeoutSpinBox_ = new QSpinBox(page);
    offlineTimeoutSpinBox_->setRange(0, 120);
    offlineTimeoutSpinBox_->setSuffix(" min");
    offlineTimeoutSpinBox_->setToolTip("Auto-stop stream after X minutes offline (0 = disabled)");

    recordWhileStreamingCheckbox_ = new QCheckBox("Auto-record while streaming", page);
    switchToStartingCheckbox_ = new QCheckBox("Switch to Starting scene on stream start", page);
    switchFromStartingCheckbox_ = new QCheckBox("Auto-switch from Starting to Live when feed detected", page);

    form->addRow("Offline Timeout:", offlineTimeoutSpinBox_);
    form->addRow(recordWhileStreamingCheckbox_);
    form->addRow(switchToStartingCheckbox_);
    form->addRow(switchFromStartingCheckbox_);
    layout->addWidget(group);

    QGroupBox *ristGrp = new QGroupBox("RIST Stale Frame Fix", page);
    QFormLayout *ristForm = new QFormLayout(ristGrp);
    ristForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    ristStaleFrameFixSpinBox_ = new QSpinBox(page);
    ristStaleFrameFixSpinBox_->setRange(0, 300);
    ristStaleFrameFixSpinBox_->setSuffix(" sec");
    ristStaleFrameFixSpinBox_->setToolTip("Auto-refresh RIST media sources after going offline (0 = disabled)");

    QLabel *ristHint = new QLabel(
        "RIST encoders leave a frozen frame when the stream stops. "
        "This will refresh the source after the configured delay. Set to 0 to disable.", page);
    ristHint->setWordWrap(true);
    ristHint->setStyleSheet("color: #a6adc8; font-size: 11px; padding: 4px;");

    ristForm->addRow("Auto-Fix Delay:", ristStaleFrameFixSpinBox_);
    ristForm->addRow(ristHint);
    layout->addWidget(ristGrp);

    layout->addStretch();
    return page;
}

// ────────────────────────────────────────────────────────────
// Load / Save
// ────────────────────────────────────────────────────────────

void SettingsDialog::loadSettings()
{
    enabledCheckbox_->setChecked(config_->enabled);
    onlyWhenStreamingCheckbox_->setChecked(config_->onlyWhenStreaming);
    instantRecoverCheckbox_->setChecked(config_->instantRecover);
    autoNotifyCheckbox_->setChecked(config_->autoNotify);
    retryAttemptsSpinBox_->setValue(config_->retryAttempts);

    lowBitrateSpinBox_->setValue(config_->triggers.low);
    rttThresholdSpinBox_->setValue(config_->triggers.rtt);
    offlineBitrateSpinBox_->setValue(config_->triggers.offline);
    rttOfflineSpinBox_->setValue(config_->triggers.rttOffline);

    auto setCombo = [](QComboBox *c, const std::string &v) {
        int idx = c->findText(QString::fromStdString(v));
        if (idx >= 0) c->setCurrentIndex(idx);
    };
    setCombo(normalSceneCombo_, config_->scenes.normal);
    setCombo(lowSceneCombo_, config_->scenes.low);
    setCombo(offlineSceneCombo_, config_->scenes.offline);
    setCombo(startingSceneCombo_, config_->optionalScenes.starting);
    setCombo(endingSceneCombo_, config_->optionalScenes.ending);
    setCombo(privacySceneCombo_, config_->optionalScenes.privacy);
    setCombo(refreshSceneCombo_, config_->optionalScenes.refresh);

    offlineTimeoutSpinBox_->setValue(config_->options.offlineTimeoutMinutes);
    recordWhileStreamingCheckbox_->setChecked(config_->options.recordWhileStreaming);
    switchToStartingCheckbox_->setChecked(config_->options.switchToStartingOnStreamStart);
    switchFromStartingCheckbox_->setChecked(config_->options.switchFromStartingToLive);
    ristStaleFrameFixSpinBox_->setValue(config_->options.ristStaleFrameFixSec);

    // Servers — create a page for each
    for (const auto &srv : config_->servers)
        addServerEntry(&srv);

    // Chat
    chatEnabledCheckbox_->setChecked(config_->chat.enabled);
    chatPlatformCombo_->setCurrentIndex(static_cast<int>(config_->chat.platform));
    chatChannelEdit_->setText(QString::fromStdString(config_->chat.channel));
    chatBotUsernameEdit_->setText(QString::fromStdString(config_->chat.botUsername));
    chatOauthEdit_->setText(QString::fromStdString(config_->chat.oauthToken));
    chatAnnounceCheckbox_->setChecked(config_->chat.announceSceneChanges);
    kickChannelIdEdit_->setText(QString::number(config_->chat.kickChannelId));
    kickChatroomIdEdit_->setText(QString::number(config_->chat.kickChatroomId));
    chatAutoStopRaidCheckbox_->setChecked(config_->chat.autoStopStreamOnRaid);
    chatAnnounceRaidStopCheckbox_->setChecked(config_->chat.announceRaidStop);
    updateChatPlatformUi();

    QString adminsStr;
    for (size_t i = 0; i < config_->chat.admins.size(); i++) {
        if (i > 0) adminsStr += ", ";
        adminsStr += QString::fromStdString(config_->chat.admins[i]);
    }
    chatAdminsEdit_->setText(adminsStr);

    cmdLiveEdit_->setText(QString::fromStdString(config_->chat.cmdLive));
    cmdLowEdit_->setText(QString::fromStdString(config_->chat.cmdLow));
    cmdBrbEdit_->setText(QString::fromStdString(config_->chat.cmdBrb));
    cmdPrivacyEdit_->setText(QString::fromStdString(config_->chat.cmdPrivacy));
    cmdRefreshEdit_->setText(QString::fromStdString(config_->chat.cmdRefresh));
    cmdStatusEdit_->setText(QString::fromStdString(config_->chat.cmdStatus));
    cmdTriggerEdit_->setText(QString::fromStdString(config_->chat.cmdTrigger));
    cmdFixEdit_->setText(QString::fromStdString(config_->chat.cmdFix));
    cmdSwitchSceneEdit_->setText(QString::fromStdString(config_->chat.cmdSwitchScene));
    cmdStartEdit_->setText(QString::fromStdString(config_->chat.cmdStart));
    cmdStopEdit_->setText(QString::fromStdString(config_->chat.cmdStop));

    msgSwitchedLiveEdit_->setText(QString::fromStdString(config_->messages.switchedToLive));
    msgSwitchedLowEdit_->setText(QString::fromStdString(config_->messages.switchedToLow));
    msgSwitchedOfflineEdit_->setText(QString::fromStdString(config_->messages.switchedToOffline));
    msgStatusEdit_->setText(QString::fromStdString(config_->messages.statusResponse));
    msgStatusOfflineEdit_->setText(QString::fromStdString(config_->messages.statusOffline));
    msgRefreshingEdit_->setText(QString::fromStdString(config_->messages.refreshing));
    msgFixEdit_->setText(QString::fromStdString(config_->messages.fixAttempt));
    msgStreamStartedEdit_->setText(QString::fromStdString(config_->messages.streamStarted));
    msgStreamStoppedEdit_->setText(QString::fromStdString(config_->messages.streamStopped));
    msgRaidStopEdit_->setText(QString::fromStdString(config_->messages.raidStop));
    msgSceneSwitchedEdit_->setText(QString::fromStdString(config_->messages.sceneSwitched));

    customCmdTable_->setRowCount(0);
    for (const auto &cmd : config_->customCommands) {
        int row = customCmdTable_->rowCount();
        customCmdTable_->insertRow(row);
        QCheckBox *chk = new QCheckBox();
        chk->setChecked(cmd.enabled);
        customCmdTable_->setCellWidget(row, 0, chk);
        customCmdTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(cmd.trigger)));
        customCmdTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(cmd.response)));
    }

    // Start on General page
    navTree_->setCurrentItem(navTree_->topLevelItem(0));
    contentStack_->setCurrentIndex(0);
}

void SettingsDialog::saveSettings()
{
    config_->lockWrite();

    config_->enabled = enabledCheckbox_->isChecked();
    config_->onlyWhenStreaming = onlyWhenStreamingCheckbox_->isChecked();
    config_->instantRecover = instantRecoverCheckbox_->isChecked();
    config_->autoNotify = autoNotifyCheckbox_->isChecked();
    config_->retryAttempts = retryAttemptsSpinBox_->value();

    config_->triggers.low = lowBitrateSpinBox_->value();
    config_->triggers.rtt = rttThresholdSpinBox_->value();
    config_->triggers.offline = offlineBitrateSpinBox_->value();
    config_->triggers.rttOffline = rttOfflineSpinBox_->value();

    config_->scenes.normal = normalSceneCombo_->currentText().toStdString();
    config_->scenes.low = lowSceneCombo_->currentText().toStdString();
    config_->scenes.offline = offlineSceneCombo_->currentText().toStdString();

    config_->optionalScenes.starting = startingSceneCombo_->currentData().toString().toStdString();
    config_->optionalScenes.ending = endingSceneCombo_->currentData().toString().toStdString();
    config_->optionalScenes.privacy = privacySceneCombo_->currentData().toString().toStdString();
    config_->optionalScenes.refresh = refreshSceneCombo_->currentData().toString().toStdString();

    config_->options.offlineTimeoutMinutes = offlineTimeoutSpinBox_->value();
    config_->options.recordWhileStreaming = recordWhileStreamingCheckbox_->isChecked();
    config_->options.switchToStartingOnStreamStart = switchToStartingCheckbox_->isChecked();
    config_->options.switchFromStartingToLive = switchFromStartingCheckbox_->isChecked();
    config_->options.ristStaleFrameFixSec = ristStaleFrameFixSpinBox_->value();

    // Servers from sidebar pages
    config_->servers.clear();
    for (const auto &sp : serverPages_) {
        StreamServerConfig srv;
        srv.enabled = sp.enabledCheck ? sp.enabledCheck->isChecked() : true;
        srv.type = sp.typeCombo
            ? static_cast<ServerType>(sp.typeCombo->currentData().toInt())
            : ServerType::Belabox;
        srv.name = sp.nameEdit ? sp.nameEdit->text().toStdString() : "";
        srv.statsUrl = sp.urlEdit ? sp.urlEdit->text().toStdString() : "";
        srv.key = sp.keyEdit ? sp.keyEdit->text().toStdString() : "";
        srv.priority = sp.prioritySpin ? sp.prioritySpin->value() : 0;
        config_->servers.push_back(srv);
    }
    config_->sortServersByPriority();

    config_->chat.enabled = chatEnabledCheckbox_->isChecked();
    config_->chat.platform = static_cast<ChatPlatform>(chatPlatformCombo_->currentIndex());
    config_->chat.channel = chatChannelEdit_->text().toStdString();
    config_->chat.botUsername = chatBotUsernameEdit_->text().toStdString();
    config_->chat.oauthToken = chatOauthEdit_->text().toStdString();
    config_->chat.announceSceneChanges = chatAnnounceCheckbox_->isChecked();
    config_->chat.kickChannelId = kickChannelIdEdit_->text().trimmed().toULongLong();
    config_->chat.kickChatroomId = kickChatroomIdEdit_->text().trimmed().toULongLong();
    config_->chat.autoStopStreamOnRaid = chatAutoStopRaidCheckbox_->isChecked();
    config_->chat.announceRaidStop = chatAnnounceRaidStopCheckbox_->isChecked();

    config_->chat.admins.clear();
    QString adminsStr = chatAdminsEdit_->text();
    if (!adminsStr.isEmpty()) {
        QStringList list = adminsStr.split(',', Qt::SkipEmptyParts);
        for (const QString &a : list)
            config_->chat.admins.push_back(a.trimmed().toStdString());
    }

    config_->chat.cmdLive = cmdLiveEdit_->text().toStdString();
    config_->chat.cmdLow = cmdLowEdit_->text().toStdString();
    config_->chat.cmdBrb = cmdBrbEdit_->text().toStdString();
    config_->chat.cmdPrivacy = cmdPrivacyEdit_->text().toStdString();
    config_->chat.cmdRefresh = cmdRefreshEdit_->text().toStdString();
    config_->chat.cmdStatus = cmdStatusEdit_->text().toStdString();
    config_->chat.cmdTrigger = cmdTriggerEdit_->text().toStdString();
    config_->chat.cmdFix = cmdFixEdit_->text().toStdString();
    config_->chat.cmdSwitchScene = cmdSwitchSceneEdit_->text().toStdString();
    config_->chat.cmdStart = cmdStartEdit_->text().toStdString();
    config_->chat.cmdStop = cmdStopEdit_->text().toStdString();

    config_->messages.switchedToLive = msgSwitchedLiveEdit_->text().toStdString();
    config_->messages.switchedToLow = msgSwitchedLowEdit_->text().toStdString();
    config_->messages.switchedToOffline = msgSwitchedOfflineEdit_->text().toStdString();
    config_->messages.statusResponse = msgStatusEdit_->text().toStdString();
    config_->messages.statusOffline = msgStatusOfflineEdit_->text().toStdString();
    config_->messages.refreshing = msgRefreshingEdit_->text().toStdString();
    config_->messages.fixAttempt = msgFixEdit_->text().toStdString();
    config_->messages.streamStarted = msgStreamStartedEdit_->text().toStdString();
    config_->messages.streamStopped = msgStreamStoppedEdit_->text().toStdString();
    config_->messages.raidStop = msgRaidStopEdit_->text().toStdString();
    config_->messages.sceneSwitched = msgSceneSwitchedEdit_->text().toStdString();

    config_->customCommands.clear();
    for (int row = 0; row < customCmdTable_->rowCount(); row++) {
        CustomChatCommand cmd;
        QCheckBox *chk = qobject_cast<QCheckBox *>(customCmdTable_->cellWidget(row, 0));
        cmd.enabled = chk ? chk->isChecked() : true;
        QTableWidgetItem *ti = customCmdTable_->item(row, 1);
        cmd.trigger = ti ? ti->text().toStdString() : "";
        QTableWidgetItem *ri = customCmdTable_->item(row, 2);
        cmd.response = ri ? ri->text().toStdString() : "";
        if (!cmd.trigger.empty())
            config_->customCommands.push_back(cmd);
    }

    config_->unlockWrite();
}

// ────────────────────────────────────────────────────────────
// Slots
// ────────────────────────────────────────────────────────────

void SettingsDialog::onApply()
{
    saveSettings();
    if (switcher_) {
        switcher_->reloadServers();
        switcher_->requestChatReconnect();
    }
    updateStreamingFieldStates();
}

void SettingsDialog::onSave()
{
    onApply();
    accept();
}

void SettingsDialog::updateChatPlatformUi()
{
    bool twitch = chatPlatformCombo_->currentIndex() == static_cast<int>(ChatPlatform::Twitch);
    twitchBotLabel_->setVisible(twitch);
    chatBotUsernameEdit_->setVisible(twitch);
    twitchOauthLabel_->setVisible(twitch);
    chatOauthEdit_->setVisible(twitch);
    kickChannelLabel_->setVisible(!twitch);
    kickChannelIdEdit_->setVisible(!twitch);
    kickChatroomLabel_->setVisible(!twitch);
    kickChatroomIdEdit_->setVisible(!twitch);
    chatAnnounceRaidStopCheckbox_->setEnabled(twitch);
}

void SettingsDialog::updateStreamingFieldStates()
{
    bool streaming = switcher_ && switcher_->isCurrentlyStreaming();
    chatEnabledCheckbox_->setEnabled(!streaming);
    chatPlatformCombo_->setEnabled(!streaming);
    chatChannelEdit_->setEnabled(!streaming);
    chatBotUsernameEdit_->setEnabled(!streaming);
    chatOauthEdit_->setEnabled(!streaming);
    kickChannelIdEdit_->setEnabled(!streaming);
    kickChatroomIdEdit_->setEnabled(!streaming);
}

void SettingsDialog::refreshStatus()
{
    if (!switcher_) {
        statusLabel_->setText("Status: Not initialized");
        statusLabel_->setStyleSheet("color: #f38ba8; font-weight: bold; font-size: 13px;");
        bitrateLabel_->setText("Bitrate: --");
        return;
    }

    statusLabel_->setText("Status: " + QString::fromStdString(switcher_->getStatusString()));

    auto info = switcher_->getCurrentBitrate();
    if (info.isOnline) {
        statusLabel_->setStyleSheet("color: #a6e3a1; font-weight: bold; font-size: 13px;");
        bitrateLabel_->setStyleSheet("color: #89dceb; font-weight: bold; font-size: 13px;");
        bitrateLabel_->setText(QString("Bitrate: %1 kbps").arg(info.bitrateKbps));
    } else {
        statusLabel_->setStyleSheet("color: #f38ba8; font-weight: bold; font-size: 13px;");
        bitrateLabel_->setStyleSheet("color: #f38ba8; font-weight: bold; font-size: 13px;");
        bitrateLabel_->setText("Bitrate: Offline");
    }

    updateStreamingFieldStates();
}

} // namespace BitrateSwitch
