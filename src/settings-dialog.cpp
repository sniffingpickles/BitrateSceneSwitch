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
#include <obs-frontend-api.h>

namespace BitrateSwitch {

SettingsDialog::SettingsDialog(Config *config, Switcher *switcher, QWidget *parent)
    : QDialog(parent)
    , config_(config)
    , switcher_(switcher)
{
    setWindowTitle("Bitrate Scene Switch Settings");
    setMinimumSize(750, 600);
    
    // Modern dark stylesheet with color accents
    setStyleSheet(
        "QDialog {"
        "  background-color: #1e1e2e;"
        "  color: #cdd6f4;"
        "}"
        "QTabWidget::pane {"
        "  border: 1px solid #313244;"
        "  background-color: #1e1e2e;"
        "  border-radius: 6px;"
        "}"
        "QTabBar::tab {"
        "  background-color: #313244;"
        "  color: #a6adc8;"
        "  padding: 8px 18px;"
        "  margin-right: 2px;"
        "  border-top-left-radius: 6px;"
        "  border-top-right-radius: 6px;"
        "  font-weight: bold;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #45475a;"
        "  color: #89b4fa;"
        "}"
        "QTabBar::tab:hover {"
        "  background-color: #45475a;"
        "}"
        "QGroupBox {"
        "  background-color: #181825;"
        "  border: 1px solid #313244;"
        "  border-radius: 8px;"
        "  margin-top: 14px;"
        "  padding-top: 14px;"
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
        "  min-height: 24px;"
        "  selection-background-color: #89b4fa;"
        "}"
        "QSpinBox:focus, QComboBox:focus, QLineEdit:focus {"
        "  border-color: #89b4fa;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  padding-right: 8px;"
        "}"
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
        "QPushButton:pressed {"
        "  background-color: #313244;"
        "}"
        "QTableWidget {"
        "  background-color: #181825;"
        "  color: #cdd6f4;"
        "  gridline-color: #313244;"
        "  border: 1px solid #313244;"
        "  border-radius: 6px;"
        "  selection-background-color: #45475a;"
        "}"
        "QTableWidget::item {"
        "  padding: 4px;"
        "}"
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
    );
    
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

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Status bar at top
    QGroupBox *statusGroup = new QGroupBox("Status", this);
    statusGroup->setStyleSheet(
        "QGroupBox { background-color: #11111b; border: 1px solid #313244; }"
        "QGroupBox::title { color: #f9e2af; }"
    );
    QHBoxLayout *statusLayout = new QHBoxLayout(statusGroup);
    statusLabel_ = new QLabel("Status: Unknown", this);
    statusLabel_->setStyleSheet("color: #a6e3a1; font-weight: bold; font-size: 13px;");
    bitrateLabel_ = new QLabel("Bitrate: --", this);
    bitrateLabel_->setStyleSheet("color: #89dceb; font-weight: bold; font-size: 13px;");
    statusLayout->addWidget(statusLabel_);
    statusLayout->addWidget(bitrateLabel_);
    statusLayout->addStretch();
    mainLayout->addWidget(statusGroup);

    // Tab widget for organized settings
    tabWidget_ = new QTabWidget(this);
    
    QWidget *generalTab = new QWidget();
    QWidget *triggersTab = new QWidget();
    QWidget *scenesTab = new QWidget();
    QWidget *serversTab = new QWidget();
    QWidget *advancedTab = new QWidget();
    QWidget *chatTab = new QWidget();
    QWidget *messagesTab = new QWidget();
    
    setupGeneralTab(generalTab);
    setupTriggersTab(triggersTab);
    setupScenesTab(scenesTab);
    setupServersTab(serversTab);
    setupAdvancedTab(advancedTab);
    setupChatTab(chatTab);
    setupMessagesTab(messagesTab);
    
    tabWidget_->addTab(generalTab, "General");
    tabWidget_->addTab(triggersTab, "Triggers");
    tabWidget_->addTab(scenesTab, "Scenes");
    tabWidget_->addTab(serversTab, "Servers");
    tabWidget_->addTab(chatTab, "Chat");
    tabWidget_->addTab(messagesTab, "Messages");
    tabWidget_->addTab(advancedTab, "Advanced");
    
    mainLayout->addWidget(tabWidget_);

    // Dialog buttons - custom styled
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    QPushButton *saveBtn = new QPushButton("Save", this);
    saveBtn->setStyleSheet(
        "QPushButton { background-color: #a6e3a1; color: #1e1e2e; border: none; padding: 10px 30px; font-size: 14px; }"
        "QPushButton:hover { background-color: #94e2d5; }"
        "QPushButton:pressed { background-color: #74c7ec; }"
    );
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    
    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #f38ba8; color: #1e1e2e; border: none; padding: 10px 30px; font-size: 14px; }"
        "QPushButton:hover { background-color: #eba0ac; }"
        "QPushButton:pressed { background-color: #f2cdcd; }"
    );
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    // Update notification (hidden by default)
    updateLabel_ = new QLabel(this);
    updateLabel_->setStyleSheet(
        "QLabel { background-color: #f9e2af; color: #1e1e2e; padding: 10px; border-radius: 6px; font-weight: bold; }");
    updateLabel_->setOpenExternalLinks(true);
    updateLabel_->setAlignment(Qt::AlignCenter);
    updateLabel_->setVisible(false);
    mainLayout->addWidget(updateLabel_);

    // Branding footer
    QLabel *brandingLabel = new QLabel(this);
    brandingLabel->setText(
        QString("<div style='text-align: center; color: #585b70;'>"
        "Bitrate Scene Switch v%1 | Powered by "
        "<a href='https://irlhosting.com' style='color: #cba6f7;'>IRLHosting.com</a>"
        "</div>").arg(QString::fromStdString(UpdateChecker::getCurrentVersion())));
    brandingLabel->setOpenExternalLinks(true);
    brandingLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(brandingLabel);
    
    // Check for updates
    updateChecker_.checkForUpdates([this](const UpdateInfo& info) {
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
}

QWidget *SettingsDialog::wrapInScrollArea(QWidget *content, QWidget *parent)
{
    QScrollArea *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background-color: transparent; border: none; }");
    scroll->setWidget(content);
    
    QVBoxLayout *outerLayout = new QVBoxLayout(parent);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scroll);
    return content;
}

void SettingsDialog::setupGeneralTab(QWidget *tab)
{
    QWidget *content = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(content);
    
    QGroupBox *group = new QGroupBox("Switcher Settings", content);
    QFormLayout *form = new QFormLayout(group);
    
    enabledCheckbox_ = new QCheckBox("Enable automatic scene switching", content);
    onlyWhenStreamingCheckbox_ = new QCheckBox("Only switch when streaming", content);
    instantRecoverCheckbox_ = new QCheckBox("Instantly switch on bitrate recovery", content);
    autoNotifyCheckbox_ = new QCheckBox("Enable auto-switch notifications", content);
    
    retryAttemptsSpinBox_ = new QSpinBox(content);
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
    wrapInScrollArea(content, tab);
}

void SettingsDialog::setupTriggersTab(QWidget *tab)
{
    QWidget *content = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(content);
    
    QGroupBox *group = new QGroupBox("Bitrate Triggers", content);
    QFormLayout *form = new QFormLayout(group);

    lowBitrateSpinBox_ = new QSpinBox(content);
    lowBitrateSpinBox_->setRange(0, 50000);
    lowBitrateSpinBox_->setValue(800);
    lowBitrateSpinBox_->setSuffix(" kbps");
    lowBitrateSpinBox_->setToolTip("Switch to Low scene when bitrate drops below this");

    rttThresholdSpinBox_ = new QSpinBox(content);
    rttThresholdSpinBox_->setRange(0, 10000);
    rttThresholdSpinBox_->setValue(2500);
    rttThresholdSpinBox_->setSuffix(" ms");
    rttThresholdSpinBox_->setToolTip("Switch to Low scene when RTT exceeds this (SRT only)");

    offlineBitrateSpinBox_ = new QSpinBox(content);
    offlineBitrateSpinBox_->setRange(0, 10000);
    offlineBitrateSpinBox_->setValue(0);
    offlineBitrateSpinBox_->setSuffix(" kbps");
    offlineBitrateSpinBox_->setToolTip("Switch to Offline when bitrate drops below this (0 = server offline)");

    rttOfflineSpinBox_ = new QSpinBox(content);
    rttOfflineSpinBox_->setRange(0, 30000);
    rttOfflineSpinBox_->setValue(0);
    rttOfflineSpinBox_->setSuffix(" ms");
    rttOfflineSpinBox_->setToolTip("Switch to Offline when RTT exceeds this (0 = disabled)");

    form->addRow("Low Bitrate Threshold:", lowBitrateSpinBox_);
    form->addRow("RTT Threshold (Low):", rttThresholdSpinBox_);
    form->addRow("Offline Bitrate Threshold:", offlineBitrateSpinBox_);
    form->addRow("RTT Threshold (Offline):", rttOfflineSpinBox_);
    
    layout->addWidget(group);
    layout->addStretch();
    wrapInScrollArea(content, tab);
}

void SettingsDialog::setupScenesTab(QWidget *tab)
{
    QWidget *content = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(content);
    
    // Main switching scenes
    QGroupBox *mainGroup = new QGroupBox("Switching Scenes", content);
    QFormLayout *mainForm = new QFormLayout(mainGroup);

    normalSceneCombo_ = new QComboBox(content);
    lowSceneCombo_ = new QComboBox(content);
    offlineSceneCombo_ = new QComboBox(content);

    populateSceneComboBox(normalSceneCombo_);
    populateSceneComboBox(lowSceneCombo_);
    populateSceneComboBox(offlineSceneCombo_);

    mainForm->addRow("Normal Scene:", normalSceneCombo_);
    mainForm->addRow("Low Bitrate Scene:", lowSceneCombo_);
    mainForm->addRow("Offline Scene:", offlineSceneCombo_);
    layout->addWidget(mainGroup);

    // Optional scenes
    QGroupBox *optGroup = new QGroupBox("Optional Scenes", content);
    QFormLayout *optForm = new QFormLayout(optGroup);

    startingSceneCombo_ = new QComboBox(content);
    endingSceneCombo_ = new QComboBox(content);
    privacySceneCombo_ = new QComboBox(content);
    refreshSceneCombo_ = new QComboBox(content);

    populateSceneComboBox(startingSceneCombo_, true);
    populateSceneComboBox(endingSceneCombo_, true);
    populateSceneComboBox(privacySceneCombo_, true);
    populateSceneComboBox(refreshSceneCombo_, true);

    optForm->addRow("Starting Scene:", startingSceneCombo_);
    optForm->addRow("Ending Scene:", endingSceneCombo_);
    optForm->addRow("Privacy Scene:", privacySceneCombo_);
    optForm->addRow("Refresh Scene:", refreshSceneCombo_);
    layout->addWidget(optGroup);
    
    layout->addStretch();
    wrapInScrollArea(content, tab);
}

void SettingsDialog::setupServersTab(QWidget *tab)
{
    QWidget *content = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(content);

    serverTable_ = new QTableWidget(0, 6, content);
    serverTable_->setHorizontalHeaderLabels({"Enabled", "Type", "Name", "Stats URL", "Stream Key", "Priority"});
    serverTable_->verticalHeader()->setVisible(false);
    serverTable_->horizontalHeader()->setStretchLastSection(false);
    serverTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    serverTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    serverTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    serverTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    serverTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    serverTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    serverTable_->setColumnWidth(0, 60);
    serverTable_->setColumnWidth(1, 110);
    serverTable_->setColumnWidth(2, 100);
    serverTable_->setColumnWidth(4, 120);
    serverTable_->setColumnWidth(5, 60);
    serverTable_->verticalHeader()->setDefaultSectionSize(serverTable_->verticalHeader()->defaultSectionSize() + 15);
    serverTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    serverTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(serverTable_);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    addServerBtn_ = new QPushButton("+ Add Server", content);
    addServerBtn_->setStyleSheet(
        "QPushButton { background-color: #a6e3a1; color: #1e1e2e; border: none; padding: 8px 18px; }"
        "QPushButton:hover { background-color: #94e2d5; }"
    );
    removeServerBtn_ = new QPushButton("Remove", content);
    removeServerBtn_->setStyleSheet(
        "QPushButton { background-color: #f38ba8; color: #1e1e2e; border: none; padding: 8px 18px; }"
        "QPushButton:hover { background-color: #eba0ac; }"
    );
    testBtn_ = new QPushButton("Test Connection", content);
    testBtn_->setStyleSheet(
        "QPushButton { background-color: #89b4fa; color: #1e1e2e; border: none; padding: 8px 18px; }"
        "QPushButton:hover { background-color: #74c7ec; }"
    );
    btnLayout->addWidget(addServerBtn_);
    btnLayout->addWidget(removeServerBtn_);
    btnLayout->addWidget(testBtn_);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(addServerBtn_, &QPushButton::clicked, this, &SettingsDialog::onAddServer);
    connect(removeServerBtn_, &QPushButton::clicked, this, &SettingsDialog::onRemoveServer);
    connect(testBtn_, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);
    wrapInScrollArea(content, tab);
}

void SettingsDialog::setupAdvancedTab(QWidget *tab)
{
    QWidget *content = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(content);
    
    QGroupBox *group = new QGroupBox("Advanced Options", content);
    QFormLayout *form = new QFormLayout(group);

    offlineTimeoutSpinBox_ = new QSpinBox(content);
    offlineTimeoutSpinBox_->setRange(0, 120);
    offlineTimeoutSpinBox_->setValue(0);
    offlineTimeoutSpinBox_->setSuffix(" min");
    offlineTimeoutSpinBox_->setToolTip("Auto-stop stream after X minutes offline (0 = disabled)");

    recordWhileStreamingCheckbox_ = new QCheckBox("Auto-record while streaming", content);
    switchToStartingCheckbox_ = new QCheckBox("Switch to Starting scene on stream start", content);
    switchFromStartingCheckbox_ = new QCheckBox("Auto-switch from Starting to Live when feed detected", content);

    form->addRow("Offline Timeout:", offlineTimeoutSpinBox_);
    form->addRow(recordWhileStreamingCheckbox_);
    form->addRow(switchToStartingCheckbox_);
    form->addRow(switchFromStartingCheckbox_);
    
    layout->addWidget(group);

    // RIST stale frame fix
    QGroupBox *ristGroup = new QGroupBox("RIST Stale Frame Fix", content);
    QFormLayout *ristForm = new QFormLayout(ristGroup);

    ristStaleFrameFixSpinBox_ = new QSpinBox(content);
    ristStaleFrameFixSpinBox_->setRange(0, 300);
    ristStaleFrameFixSpinBox_->setValue(0);
    ristStaleFrameFixSpinBox_->setSuffix(" sec");
    ristStaleFrameFixSpinBox_->setToolTip(
        "When the stream goes offline, automatically run a media source fix after this many seconds "
        "to clear the frozen last frame that RIST leaves behind. Set to 0 to disable.");

    QLabel *ristHint = new QLabel(
        "RIST encoders leave a single frozen frame on the media source when the stream stops. "
        "This setting will automatically refresh the media source after the configured delay "
        "once the stream goes offline, clearing that stale frame.", content);
    ristHint->setWordWrap(true);
    ristHint->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; padding: 4px; }");

    ristForm->addRow("Auto-Fix Delay:", ristStaleFrameFixSpinBox_);
    ristForm->addRow(ristHint);

    layout->addWidget(ristGroup);
    layout->addStretch();
    wrapInScrollArea(content, tab);
}

void SettingsDialog::setupChatTab(QWidget *tab)
{
    QWidget *content = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(content);
    
    // Chat connection settings
    QGroupBox *connGroup = new QGroupBox("Chat Connection", content);
    QFormLayout *connForm = new QFormLayout(connGroup);
    connForm->setVerticalSpacing(10);
    connForm->setContentsMargins(12, 24, 12, 12);
    
    chatEnabledCheckbox_ = new QCheckBox("Enable chat integration", content);
    chatEnabledCheckbox_->setToolTip("Connect to Twitch chat for bot commands");
    
    chatPlatformCombo_ = new QComboBox(content);
    chatPlatformCombo_->addItems({"Twitch"});
    
    chatChannelEdit_ = new QLineEdit(content);
    chatChannelEdit_->setPlaceholderText("your_channel_name");
    
    chatBotUsernameEdit_ = new QLineEdit(content);
    chatBotUsernameEdit_->setPlaceholderText("(optional - uses channel name if empty)");
    
    chatOauthEdit_ = new QLineEdit(content);
    chatOauthEdit_->setEchoMode(QLineEdit::Password);
    chatOauthEdit_->setPlaceholderText("oauth:xxxxxxxxxxxxxx");
    chatOauthEdit_->setToolTip("Get your OAuth token from https://twitchapps.com/tmi/");
    
    connForm->addRow(chatEnabledCheckbox_);
    connForm->addRow("Platform:", chatPlatformCombo_);
    connForm->addRow("Channel:", chatChannelEdit_);
    connForm->addRow("Bot Username:", chatBotUsernameEdit_);
    connForm->addRow("OAuth Token:", chatOauthEdit_);
    
    layout->addWidget(connGroup);
    
    // Chat permissions
    QGroupBox *permGroup = new QGroupBox("Permissions", content);
    QFormLayout *permForm = new QFormLayout(permGroup);
    permForm->setVerticalSpacing(10);
    permForm->setContentsMargins(12, 24, 12, 12);
    
    chatAdminsEdit_ = new QLineEdit(content);
    chatAdminsEdit_->setPlaceholderText("user1, user2, user3 (empty = channel owner only)");
    chatAdminsEdit_->setToolTip("Comma-separated list of users who can use commands");
    
    chatAnnounceCheckbox_ = new QCheckBox("Announce scene changes in chat", content);
    
    permForm->addRow("Allowed Users:", chatAdminsEdit_);
    permForm->addRow(chatAnnounceCheckbox_);
    
    layout->addWidget(permGroup);
    
    // Available commands info
    QGroupBox *cmdGroup = new QGroupBox("Available Commands", content);
    QVBoxLayout *cmdLayout = new QVBoxLayout(cmdGroup);
    QLabel *cmdLabel = new QLabel(
        "• !live - Switch to Live scene\n"
        "• !low - Switch to Low scene\n"
        "• !brb - Switch to BRB/Offline scene\n"
        "• !ss <name> - Switch to any scene (case-insensitive)\n"
        "• !refresh - Refresh scene (fix issues)\n"
        "• !status - Show current status\n"
        "• !trigger - Force switch check\n"
        "• !fix - Alias for refresh", content);
    cmdLayout->addWidget(cmdLabel);
    
    layout->addWidget(cmdGroup);
    layout->addStretch();
    wrapInScrollArea(content, tab);
}

void SettingsDialog::setupMessagesTab(QWidget *tab)
{
    QWidget *scrollContent = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(scrollContent);
    layout->setSpacing(12);
    
    // Placeholder reference
    QGroupBox *refGroup = new QGroupBox("Available Placeholders", scrollContent);
    QVBoxLayout *refLayout = new QVBoxLayout(refGroup);
    QLabel *refLabel = new QLabel(
        "<span style='color: #89b4fa;'>{bitrate}</span> - Current bitrate (kbps)&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{rtt}</span> - Round-trip time (ms)&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{scene}</span> - Current scene&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{prev_scene}</span> - Previous scene<br>"
        "<span style='color: #89b4fa;'>{server}</span> - Active server name&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{status}</span> - Online/Offline&nbsp;&nbsp;"
        "<span style='color: #89b4fa;'>{uptime}</span> - Stream state", scrollContent);
    refLabel->setWordWrap(true);
    refLabel->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; padding: 4px; }");
    refLayout->addWidget(refLabel);
    layout->addWidget(refGroup);

    // Auto-switch message templates
    QGroupBox *autoGroup = new QGroupBox("Auto-Switch Messages", scrollContent);
    QFormLayout *autoForm = new QFormLayout(autoGroup);
    autoForm->setVerticalSpacing(10);
    autoForm->setContentsMargins(12, 24, 12, 12);
    
    msgSwitchedLiveEdit_ = new QLineEdit(scrollContent);
    msgSwitchedLiveEdit_->setToolTip("Message sent when auto-switching to Live scene");
    msgSwitchedLowEdit_ = new QLineEdit(scrollContent);
    msgSwitchedLowEdit_->setToolTip("Message sent when auto-switching to Low scene");
    msgSwitchedOfflineEdit_ = new QLineEdit(scrollContent);
    msgSwitchedOfflineEdit_->setToolTip("Message sent when auto-switching to Offline scene");
    
    autoForm->addRow("Switched to Live:", msgSwitchedLiveEdit_);
    autoForm->addRow("Switched to Low:", msgSwitchedLowEdit_);
    autoForm->addRow("Switched to Offline:", msgSwitchedOfflineEdit_);
    layout->addWidget(autoGroup);
    
    // Command response templates
    QGroupBox *cmdGroup = new QGroupBox("Command Response Messages", scrollContent);
    QFormLayout *cmdForm = new QFormLayout(cmdGroup);
    cmdForm->setVerticalSpacing(10);
    cmdForm->setContentsMargins(12, 24, 12, 12);
    
    msgStatusEdit_ = new QLineEdit(scrollContent);
    msgStatusEdit_->setToolTip("Response for !status when online");
    msgStatusOfflineEdit_ = new QLineEdit(scrollContent);
    msgStatusOfflineEdit_->setToolTip("Response for !status when offline");
    msgRefreshingEdit_ = new QLineEdit(scrollContent);
    msgFixEdit_ = new QLineEdit(scrollContent);
    msgStreamStartedEdit_ = new QLineEdit(scrollContent);
    msgStreamStoppedEdit_ = new QLineEdit(scrollContent);
    msgSceneSwitchedEdit_ = new QLineEdit(scrollContent);
    msgSceneSwitchedEdit_->setToolTip("Used for !live, !low, !brb, !ss commands");
    
    cmdForm->addRow("Status (online):", msgStatusEdit_);
    cmdForm->addRow("Status (offline):", msgStatusOfflineEdit_);
    cmdForm->addRow("Refreshing:", msgRefreshingEdit_);
    cmdForm->addRow("Fix attempt:", msgFixEdit_);
    cmdForm->addRow("Stream started:", msgStreamStartedEdit_);
    cmdForm->addRow("Stream stopped:", msgStreamStoppedEdit_);
    cmdForm->addRow("Scene switched:", msgSceneSwitchedEdit_);
    layout->addWidget(cmdGroup);
    
    // Custom commands
    QGroupBox *customGroup = new QGroupBox("Custom Commands", scrollContent);
    QVBoxLayout *customLayout = new QVBoxLayout(customGroup);
    customLayout->setContentsMargins(12, 24, 12, 12);
    customLayout->setSpacing(8);
    
    QLabel *customHint = new QLabel(
        "Define custom chat commands. The response supports all placeholders above.", scrollContent);
    customHint->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; }");
    customLayout->addWidget(customHint);
    
    customCmdTable_ = new QTableWidget(0, 3, scrollContent);
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
    customLayout->addWidget(customCmdTable_);
    
    QHBoxLayout *customBtnLayout = new QHBoxLayout();
    addCustomCmdBtn_ = new QPushButton("+ Add Command", scrollContent);
    addCustomCmdBtn_->setStyleSheet(
        "QPushButton { background-color: #a6e3a1; color: #1e1e2e; border: none; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #94e2d5; }");
    removeCustomCmdBtn_ = new QPushButton("Remove", scrollContent);
    removeCustomCmdBtn_->setStyleSheet(
        "QPushButton { background-color: #f38ba8; color: #1e1e2e; border: none; padding: 6px 14px; }"
        "QPushButton:hover { background-color: #eba0ac; }");
    customBtnLayout->addWidget(addCustomCmdBtn_);
    customBtnLayout->addWidget(removeCustomCmdBtn_);
    customBtnLayout->addStretch();
    customLayout->addLayout(customBtnLayout);
    
    connect(addCustomCmdBtn_, &QPushButton::clicked, this, [this]() {
        int row = customCmdTable_->rowCount();
        customCmdTable_->insertRow(row);
        QCheckBox *enabledCheck = new QCheckBox();
        enabledCheck->setChecked(true);
        customCmdTable_->setCellWidget(row, 0, enabledCheck);
        customCmdTable_->setItem(row, 1, new QTableWidgetItem("!mycommand"));
        customCmdTable_->setItem(row, 2, new QTableWidgetItem("Bitrate: {bitrate} kbps | RTT: {rtt} ms"));
    });
    
    connect(removeCustomCmdBtn_, &QPushButton::clicked, this, [this]() {
        int row = customCmdTable_->currentRow();
        if (row >= 0) customCmdTable_->removeRow(row);
    });
    
    layout->addWidget(customGroup);
    
    wrapInScrollArea(scrollContent, tab);
}

void SettingsDialog::populateSceneComboBox(QComboBox *combo, bool allowEmpty)
{
    combo->clear();
    
    if (allowEmpty) {
        combo->addItem("(None)", "");
    }
    
    obs_frontend_source_list scenes = {};
    obs_frontend_get_scenes(&scenes);
    
    for (size_t i = 0; i < scenes.sources.num; i++) {
        obs_source_t *source = scenes.sources.array[i];
        const char *name = obs_source_get_name(source);
        if (name) {
            combo->addItem(QString::fromUtf8(name), QString::fromUtf8(name));
        }
    }
    
    obs_frontend_source_list_free(&scenes);
}

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

    // Main scenes
    int idx = normalSceneCombo_->findText(QString::fromStdString(config_->scenes.normal));
    if (idx >= 0) normalSceneCombo_->setCurrentIndex(idx);
    
    idx = lowSceneCombo_->findText(QString::fromStdString(config_->scenes.low));
    if (idx >= 0) lowSceneCombo_->setCurrentIndex(idx);
    
    idx = offlineSceneCombo_->findText(QString::fromStdString(config_->scenes.offline));
    if (idx >= 0) offlineSceneCombo_->setCurrentIndex(idx);

    // Optional scenes
    idx = startingSceneCombo_->findText(QString::fromStdString(config_->optionalScenes.starting));
    if (idx >= 0) startingSceneCombo_->setCurrentIndex(idx);
    
    idx = endingSceneCombo_->findText(QString::fromStdString(config_->optionalScenes.ending));
    if (idx >= 0) endingSceneCombo_->setCurrentIndex(idx);
    
    idx = privacySceneCombo_->findText(QString::fromStdString(config_->optionalScenes.privacy));
    if (idx >= 0) privacySceneCombo_->setCurrentIndex(idx);
    
    idx = refreshSceneCombo_->findText(QString::fromStdString(config_->optionalScenes.refresh));
    if (idx >= 0) refreshSceneCombo_->setCurrentIndex(idx);

    // Advanced options
    offlineTimeoutSpinBox_->setValue(config_->options.offlineTimeoutMinutes);
    recordWhileStreamingCheckbox_->setChecked(config_->options.recordWhileStreaming);
    switchToStartingCheckbox_->setChecked(config_->options.switchToStartingOnStreamStart);
    switchFromStartingCheckbox_->setChecked(config_->options.switchFromStartingToLive);
    ristStaleFrameFixSpinBox_->setValue(config_->options.ristStaleFrameFixSec);

    // Load servers
    serverTable_->setRowCount(0);
    for (const auto &server : config_->servers) {
        int row = serverTable_->rowCount();
        serverTable_->insertRow(row);

        QCheckBox *enabledCheck = new QCheckBox();
        enabledCheck->setChecked(server.enabled);
        serverTable_->setCellWidget(row, 0, enabledCheck);

        QComboBox *typeCombo = new QComboBox();
        typeCombo->addItem("BELABOX", static_cast<int>(ServerType::Belabox));
        typeCombo->addItem("NGINX", static_cast<int>(ServerType::Nginx));
        typeCombo->addItem("SRT Live Server", static_cast<int>(ServerType::SrtLiveServer));
        typeCombo->addItem("MediaMTX", static_cast<int>(ServerType::Mediamtx));
        typeCombo->addItem("Node Media Server", static_cast<int>(ServerType::NodeMediaServer));
        typeCombo->addItem("Nimble", static_cast<int>(ServerType::Nimble));
        typeCombo->addItem("RIST", static_cast<int>(ServerType::Rist));
        typeCombo->addItem("OpenIRL", static_cast<int>(ServerType::OpenIRL));
        typeCombo->addItem("Xiu", static_cast<int>(ServerType::Xiu));
        // Select the matching entry by ServerType data value
        for (int i = 0; i < typeCombo->count(); i++) {
            if (typeCombo->itemData(i).toInt() == static_cast<int>(server.type)) {
                typeCombo->setCurrentIndex(i);
                break;
            }
        }
        serverTable_->setCellWidget(row, 1, typeCombo);

        serverTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(server.name)));
        serverTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(server.statsUrl)));
        serverTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(server.key)));
        
        QSpinBox *prioritySpin = new QSpinBox();
        prioritySpin->setRange(0, 100);
        prioritySpin->setValue(server.priority);
        serverTable_->setCellWidget(row, 5, prioritySpin);
    }

    // Load chat settings
    chatEnabledCheckbox_->setChecked(config_->chat.enabled);
    chatPlatformCombo_->setCurrentIndex(static_cast<int>(config_->chat.platform));
    chatChannelEdit_->setText(QString::fromStdString(config_->chat.channel));
    chatBotUsernameEdit_->setText(QString::fromStdString(config_->chat.botUsername));
    chatOauthEdit_->setText(QString::fromStdString(config_->chat.oauthToken));
    chatAnnounceCheckbox_->setChecked(config_->chat.announceSceneChanges);
    
    // Convert admins vector to comma-separated string
    QString adminsStr;
    for (size_t i = 0; i < config_->chat.admins.size(); i++) {
        if (i > 0) adminsStr += ", ";
        adminsStr += QString::fromStdString(config_->chat.admins[i]);
    }
    chatAdminsEdit_->setText(adminsStr);

    // Load message templates
    msgSwitchedLiveEdit_->setText(QString::fromStdString(config_->messages.switchedToLive));
    msgSwitchedLowEdit_->setText(QString::fromStdString(config_->messages.switchedToLow));
    msgSwitchedOfflineEdit_->setText(QString::fromStdString(config_->messages.switchedToOffline));
    msgStatusEdit_->setText(QString::fromStdString(config_->messages.statusResponse));
    msgStatusOfflineEdit_->setText(QString::fromStdString(config_->messages.statusOffline));
    msgRefreshingEdit_->setText(QString::fromStdString(config_->messages.refreshing));
    msgFixEdit_->setText(QString::fromStdString(config_->messages.fixAttempt));
    msgStreamStartedEdit_->setText(QString::fromStdString(config_->messages.streamStarted));
    msgStreamStoppedEdit_->setText(QString::fromStdString(config_->messages.streamStopped));
    msgSceneSwitchedEdit_->setText(QString::fromStdString(config_->messages.sceneSwitched));

    // Load custom commands
    customCmdTable_->setRowCount(0);
    for (const auto &cmd : config_->customCommands) {
        int row = customCmdTable_->rowCount();
        customCmdTable_->insertRow(row);
        QCheckBox *enabledCheck = new QCheckBox();
        enabledCheck->setChecked(cmd.enabled);
        customCmdTable_->setCellWidget(row, 0, enabledCheck);
        customCmdTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(cmd.trigger)));
        customCmdTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(cmd.response)));
    }
}

void SettingsDialog::saveSettings()
{
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

    // Save servers
    config_->servers.clear();
    for (int row = 0; row < serverTable_->rowCount(); row++) {
        StreamServerConfig server;
        
        QCheckBox *enabledCheck = qobject_cast<QCheckBox*>(serverTable_->cellWidget(row, 0));
        server.enabled = enabledCheck ? enabledCheck->isChecked() : true;

        QComboBox *typeCombo = qobject_cast<QComboBox*>(serverTable_->cellWidget(row, 1));
        server.type = typeCombo ? static_cast<ServerType>(typeCombo->currentData().toInt()) : ServerType::Belabox;

        QTableWidgetItem *nameItem = serverTable_->item(row, 2);
        server.name = nameItem ? nameItem->text().toStdString() : "";

        QTableWidgetItem *urlItem = serverTable_->item(row, 3);
        server.statsUrl = urlItem ? urlItem->text().toStdString() : "";

        QTableWidgetItem *keyItem = serverTable_->item(row, 4);
        server.key = keyItem ? keyItem->text().toStdString() : "";

        QSpinBox *prioritySpin = qobject_cast<QSpinBox*>(serverTable_->cellWidget(row, 5));
        server.priority = prioritySpin ? prioritySpin->value() : 0;

        config_->servers.push_back(server);
    }
    
    config_->sortServersByPriority();

    // Save chat settings
    config_->chat.enabled = chatEnabledCheckbox_->isChecked();
    config_->chat.platform = static_cast<ChatPlatform>(chatPlatformCombo_->currentIndex());
    config_->chat.channel = chatChannelEdit_->text().toStdString();
    config_->chat.botUsername = chatBotUsernameEdit_->text().toStdString();
    config_->chat.oauthToken = chatOauthEdit_->text().toStdString();
    config_->chat.announceSceneChanges = chatAnnounceCheckbox_->isChecked();
    
    // Parse admins from comma-separated string
    config_->chat.admins.clear();
    QString adminsStr = chatAdminsEdit_->text();
    if (!adminsStr.isEmpty()) {
        QStringList adminsList = adminsStr.split(',', Qt::SkipEmptyParts);
        for (const QString &admin : adminsList) {
            config_->chat.admins.push_back(admin.trimmed().toStdString());
        }
    }

    // Save message templates
    config_->messages.switchedToLive = msgSwitchedLiveEdit_->text().toStdString();
    config_->messages.switchedToLow = msgSwitchedLowEdit_->text().toStdString();
    config_->messages.switchedToOffline = msgSwitchedOfflineEdit_->text().toStdString();
    config_->messages.statusResponse = msgStatusEdit_->text().toStdString();
    config_->messages.statusOffline = msgStatusOfflineEdit_->text().toStdString();
    config_->messages.refreshing = msgRefreshingEdit_->text().toStdString();
    config_->messages.fixAttempt = msgFixEdit_->text().toStdString();
    config_->messages.streamStarted = msgStreamStartedEdit_->text().toStdString();
    config_->messages.streamStopped = msgStreamStoppedEdit_->text().toStdString();
    config_->messages.sceneSwitched = msgSceneSwitchedEdit_->text().toStdString();

    // Save custom commands
    config_->customCommands.clear();
    for (int row = 0; row < customCmdTable_->rowCount(); row++) {
        CustomChatCommand cmd;
        QCheckBox *enabledCheck = qobject_cast<QCheckBox*>(customCmdTable_->cellWidget(row, 0));
        cmd.enabled = enabledCheck ? enabledCheck->isChecked() : true;
        QTableWidgetItem *triggerItem = customCmdTable_->item(row, 1);
        cmd.trigger = triggerItem ? triggerItem->text().toStdString() : "";
        QTableWidgetItem *responseItem = customCmdTable_->item(row, 2);
        cmd.response = responseItem ? responseItem->text().toStdString() : "";
        if (!cmd.trigger.empty()) {
            config_->customCommands.push_back(cmd);
        }
    }
}

void SettingsDialog::onAddServer()
{
    int row = serverTable_->rowCount();
    serverTable_->insertRow(row);

    QCheckBox *enabledCheck = new QCheckBox();
    enabledCheck->setChecked(true);
    serverTable_->setCellWidget(row, 0, enabledCheck);

    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItem("BELABOX", static_cast<int>(ServerType::Belabox));
    typeCombo->addItem("NGINX", static_cast<int>(ServerType::Nginx));
    typeCombo->addItem("SRT Live Server", static_cast<int>(ServerType::SrtLiveServer));
    typeCombo->addItem("MediaMTX", static_cast<int>(ServerType::Mediamtx));
    typeCombo->addItem("Node Media Server", static_cast<int>(ServerType::NodeMediaServer));
    typeCombo->addItem("Nimble", static_cast<int>(ServerType::Nimble));
    typeCombo->addItem("RIST", static_cast<int>(ServerType::Rist));
    typeCombo->addItem("OpenIRL", static_cast<int>(ServerType::OpenIRL));
    typeCombo->addItem("Xiu", static_cast<int>(ServerType::Xiu));
    serverTable_->setCellWidget(row, 1, typeCombo);

    serverTable_->setItem(row, 2, new QTableWidgetItem("New Server"));
    serverTable_->setItem(row, 3, new QTableWidgetItem("http://"));
    serverTable_->setItem(row, 4, new QTableWidgetItem(""));
    
    QSpinBox *prioritySpin = new QSpinBox();
    prioritySpin->setRange(0, 100);
    prioritySpin->setValue(0);
    serverTable_->setCellWidget(row, 5, prioritySpin);
}

void SettingsDialog::onRemoveServer()
{
    int row = serverTable_->currentRow();
    if (row >= 0) {
        serverTable_->removeRow(row);
    }
}

void SettingsDialog::onTestConnection()
{
    int row = serverTable_->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "Test Connection", "Please select a server to test.");
        return;
    }

    QTableWidgetItem *urlItem = serverTable_->item(row, 3);
    if (!urlItem) return;

    QString url = urlItem->text();
    QMessageBox::information(this, "Test Connection", 
        QString("Testing connection to:\n%1\n\n(Full test available after saving)").arg(url));
}

void SettingsDialog::onSave()
{
    saveSettings();
    
    if (switcher_) {
        switcher_->reloadServers();
        
        // Handle chat connection based on settings
        if (config_->chat.enabled) {
            switcher_->connectChat();
        } else {
            switcher_->disconnectChat();
        }
    }

    accept();
}

void SettingsDialog::refreshStatus()
{
    if (!switcher_) {
        statusLabel_->setText("Status: Not initialized");
        statusLabel_->setStyleSheet("color: #f38ba8; font-weight: bold; font-size: 13px;");
        bitrateLabel_->setText("Bitrate: --");
        return;
    }

    QString status = QString::fromStdString(switcher_->getStatusString());
    statusLabel_->setText("Status: " + status);

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
}

} // namespace BitrateSwitch
