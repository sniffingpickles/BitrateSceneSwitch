#include "settings-dialog.hpp"
#include "switcher.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QHeaderView>
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
    setMinimumSize(700, 550);
    
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
    QHBoxLayout *statusLayout = new QHBoxLayout(statusGroup);
    statusLabel_ = new QLabel("Status: Unknown", this);
    bitrateLabel_ = new QLabel("Bitrate: --", this);
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
    
    setupGeneralTab(generalTab);
    setupTriggersTab(triggersTab);
    setupScenesTab(scenesTab);
    setupServersTab(serversTab);
    setupAdvancedTab(advancedTab);
    setupChatTab(chatTab);
    
    tabWidget_->addTab(generalTab, "General");
    tabWidget_->addTab(triggersTab, "Triggers");
    tabWidget_->addTab(scenesTab, "Scenes");
    tabWidget_->addTab(serversTab, "Servers");
    tabWidget_->addTab(chatTab, "Chat");
    tabWidget_->addTab(advancedTab, "Advanced");
    
    mainLayout->addWidget(tabWidget_);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::setupGeneralTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    QGroupBox *group = new QGroupBox("Switcher Settings", tab);
    QFormLayout *form = new QFormLayout(group);
    
    enabledCheckbox_ = new QCheckBox("Enable automatic scene switching", tab);
    onlyWhenStreamingCheckbox_ = new QCheckBox("Only switch when streaming", tab);
    instantRecoverCheckbox_ = new QCheckBox("Instantly switch on bitrate recovery", tab);
    autoNotifyCheckbox_ = new QCheckBox("Enable auto-switch notifications", tab);
    
    retryAttemptsSpinBox_ = new QSpinBox(tab);
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
}

void SettingsDialog::setupTriggersTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    QGroupBox *group = new QGroupBox("Bitrate Triggers", tab);
    QFormLayout *form = new QFormLayout(group);

    lowBitrateSpinBox_ = new QSpinBox(tab);
    lowBitrateSpinBox_->setRange(0, 50000);
    lowBitrateSpinBox_->setValue(800);
    lowBitrateSpinBox_->setSuffix(" kbps");
    lowBitrateSpinBox_->setToolTip("Switch to Low scene when bitrate drops below this");

    rttThresholdSpinBox_ = new QSpinBox(tab);
    rttThresholdSpinBox_->setRange(0, 10000);
    rttThresholdSpinBox_->setValue(2500);
    rttThresholdSpinBox_->setSuffix(" ms");
    rttThresholdSpinBox_->setToolTip("Switch to Low scene when RTT exceeds this (SRT only)");

    offlineBitrateSpinBox_ = new QSpinBox(tab);
    offlineBitrateSpinBox_->setRange(0, 10000);
    offlineBitrateSpinBox_->setValue(0);
    offlineBitrateSpinBox_->setSuffix(" kbps");
    offlineBitrateSpinBox_->setToolTip("Switch to Offline when bitrate drops below this (0 = server offline)");

    rttOfflineSpinBox_ = new QSpinBox(tab);
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
}

void SettingsDialog::setupScenesTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    // Main switching scenes
    QGroupBox *mainGroup = new QGroupBox("Switching Scenes", tab);
    QFormLayout *mainForm = new QFormLayout(mainGroup);

    normalSceneCombo_ = new QComboBox(tab);
    lowSceneCombo_ = new QComboBox(tab);
    offlineSceneCombo_ = new QComboBox(tab);

    populateSceneComboBox(normalSceneCombo_);
    populateSceneComboBox(lowSceneCombo_);
    populateSceneComboBox(offlineSceneCombo_);

    mainForm->addRow("Normal Scene:", normalSceneCombo_);
    mainForm->addRow("Low Bitrate Scene:", lowSceneCombo_);
    mainForm->addRow("Offline Scene:", offlineSceneCombo_);
    layout->addWidget(mainGroup);

    // Optional scenes
    QGroupBox *optGroup = new QGroupBox("Optional Scenes", tab);
    QFormLayout *optForm = new QFormLayout(optGroup);

    startingSceneCombo_ = new QComboBox(tab);
    endingSceneCombo_ = new QComboBox(tab);
    privacySceneCombo_ = new QComboBox(tab);
    refreshSceneCombo_ = new QComboBox(tab);

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
}

void SettingsDialog::setupServersTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);

    serverTable_ = new QTableWidget(0, 6, tab);
    serverTable_->setHorizontalHeaderLabels({"Enabled", "Type", "Name", "Stats URL", "Stream Key", "Priority"});
    serverTable_->horizontalHeader()->setStretchLastSection(false);
    serverTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    serverTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    serverTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    serverTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(serverTable_);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    addServerBtn_ = new QPushButton("Add Server", tab);
    removeServerBtn_ = new QPushButton("Remove Server", tab);
    testBtn_ = new QPushButton("Test Connection", tab);
    btnLayout->addWidget(addServerBtn_);
    btnLayout->addWidget(removeServerBtn_);
    btnLayout->addWidget(testBtn_);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(addServerBtn_, &QPushButton::clicked, this, &SettingsDialog::onAddServer);
    connect(removeServerBtn_, &QPushButton::clicked, this, &SettingsDialog::onRemoveServer);
    connect(testBtn_, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);
}

void SettingsDialog::setupAdvancedTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    QGroupBox *group = new QGroupBox("Advanced Options", tab);
    QFormLayout *form = new QFormLayout(group);

    offlineTimeoutSpinBox_ = new QSpinBox(tab);
    offlineTimeoutSpinBox_->setRange(0, 120);
    offlineTimeoutSpinBox_->setValue(0);
    offlineTimeoutSpinBox_->setSuffix(" min");
    offlineTimeoutSpinBox_->setToolTip("Auto-stop stream after X minutes offline (0 = disabled)");

    recordWhileStreamingCheckbox_ = new QCheckBox("Auto-record while streaming", tab);
    switchToStartingCheckbox_ = new QCheckBox("Switch to Starting scene on stream start", tab);
    switchFromStartingCheckbox_ = new QCheckBox("Auto-switch from Starting to Live when feed detected", tab);

    form->addRow("Offline Timeout:", offlineTimeoutSpinBox_);
    form->addRow(recordWhileStreamingCheckbox_);
    form->addRow(switchToStartingCheckbox_);
    form->addRow(switchFromStartingCheckbox_);
    
    layout->addWidget(group);
    layout->addStretch();
}

void SettingsDialog::setupChatTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);
    
    // Chat connection settings
    QGroupBox *connGroup = new QGroupBox("Chat Connection", tab);
    QFormLayout *connForm = new QFormLayout(connGroup);
    
    chatEnabledCheckbox_ = new QCheckBox("Enable chat integration", tab);
    chatEnabledCheckbox_->setToolTip("Connect to Twitch chat for bot commands");
    
    chatPlatformCombo_ = new QComboBox(tab);
    chatPlatformCombo_->addItems({"Twitch"});
    
    chatChannelEdit_ = new QLineEdit(tab);
    chatChannelEdit_->setPlaceholderText("your_channel_name");
    
    chatBotUsernameEdit_ = new QLineEdit(tab);
    chatBotUsernameEdit_->setPlaceholderText("(optional - uses channel name if empty)");
    
    chatOauthEdit_ = new QLineEdit(tab);
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
    QGroupBox *permGroup = new QGroupBox("Permissions", tab);
    QFormLayout *permForm = new QFormLayout(permGroup);
    
    chatAdminsEdit_ = new QLineEdit(tab);
    chatAdminsEdit_->setPlaceholderText("user1, user2, user3 (empty = channel owner only)");
    chatAdminsEdit_->setToolTip("Comma-separated list of users who can use commands");
    
    chatAnnounceCheckbox_ = new QCheckBox("Announce scene changes in chat", tab);
    
    permForm->addRow("Allowed Users:", chatAdminsEdit_);
    permForm->addRow(chatAnnounceCheckbox_);
    
    layout->addWidget(permGroup);
    
    // Available commands info
    QGroupBox *cmdGroup = new QGroupBox("Available Commands", tab);
    QVBoxLayout *cmdLayout = new QVBoxLayout(cmdGroup);
    QLabel *cmdLabel = new QLabel(
        "• !live - Switch to Live scene\n"
        "• !low - Switch to Low scene\n"
        "• !brb - Switch to BRB/Offline scene\n"
        "• !refresh - Refresh scene (fix issues)\n"
        "• !status - Show current status\n"
        "• !trigger - Force switch check\n"
        "• !fix - Alias for refresh", tab);
    cmdLayout->addWidget(cmdLabel);
    
    layout->addWidget(cmdGroup);
    layout->addStretch();
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

    // Load servers
    serverTable_->setRowCount(0);
    for (const auto &server : config_->servers) {
        int row = serverTable_->rowCount();
        serverTable_->insertRow(row);

        QCheckBox *enabledCheck = new QCheckBox();
        enabledCheck->setChecked(server.enabled);
        serverTable_->setCellWidget(row, 0, enabledCheck);

        QComboBox *typeCombo = new QComboBox();
        typeCombo->addItems({"BELABOX", "NGINX", "SRT Live Server", "MediaMTX", 
                            "Node Media Server", "Nimble", "RIST", "OpenIRL", "IRLHosting", "Xiu"});
        typeCombo->setCurrentIndex(static_cast<int>(server.type));
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

    // Save servers
    config_->servers.clear();
    for (int row = 0; row < serverTable_->rowCount(); row++) {
        StreamServerConfig server;
        
        QCheckBox *enabledCheck = qobject_cast<QCheckBox*>(serverTable_->cellWidget(row, 0));
        server.enabled = enabledCheck ? enabledCheck->isChecked() : true;

        QComboBox *typeCombo = qobject_cast<QComboBox*>(serverTable_->cellWidget(row, 1));
        server.type = typeCombo ? static_cast<ServerType>(typeCombo->currentIndex()) : ServerType::Belabox;

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
}

void SettingsDialog::onAddServer()
{
    int row = serverTable_->rowCount();
    serverTable_->insertRow(row);

    QCheckBox *enabledCheck = new QCheckBox();
    enabledCheck->setChecked(true);
    serverTable_->setCellWidget(row, 0, enabledCheck);

    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItems({"BELABOX", "NGINX", "SRT Live Server", "MediaMTX", 
                        "Node Media Server", "Nimble", "RIST", "OpenIRL", "IRLHosting", "Xiu"});
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
        bitrateLabel_->setText("Bitrate: --");
        return;
    }

    QString status = QString::fromStdString(switcher_->getStatusString());
    statusLabel_->setText("Status: " + status);

    auto info = switcher_->getCurrentBitrate();
    if (info.isOnline) {
        bitrateLabel_->setText(QString("Bitrate: %1 kbps").arg(info.bitrateKbps));
    } else {
        bitrateLabel_->setText("Bitrate: Offline");
    }
}

} // namespace BitrateSwitch
