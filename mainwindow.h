#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QStringList>
#include <QVector>
#include <QDateTime>
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class QListWidget;
class QStackedWidget;
class QLabel;
class QTextEdit;
class QLineEdit;
class QTableWidget;
class QPushButton;
class QCheckBox;
class QFileSystemWatcher;
class QTreeWidget;
class QComboBox;
class QProcess;
class QCloseEvent;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    Ui::MainWindow *ui;
    QSettings settings;
    QListWidget *nav = nullptr, *ancestors = nullptr;
    QStackedWidget *pages = nullptr;
    QLabel *status = nullptr, *featured = nullptr, *recordDetail = nullptr, *pathLabel = nullptr;
    QTextEdit *logs = nullptr, *publicPreview = nullptr, *sessionHistory = nullptr;
    QLineEdit *pathInput = nullptr, *treeInput = nullptr, *searchInput = nullptr;
    QFileSystemWatcher *watcher = nullptr;
    QCheckBox *manualApproval = nullptr;
    QJsonArray people, identifiers, relationships;
    QStringList eligibleNames;
    int featuredIndex = -1;
    bool hasData = false;
    QTreeWidget *chronicleTree = nullptr;
    QComboBox *chronicleRoot = nullptr;
    QLabel *chronicleSummary = nullptr;
    QDateTime stateSeen, treeSeen;
    QWidget *chronicleOverlay = nullptr;
    QTreeWidget *overlayTree = nullptr;
    QComboBox *overlayRoot = nullptr;
    QLabel *overlaySummary = nullptr;
    QTableWidget *familyTable = nullptr;
    QComboBox *storySelector = nullptr;
    QTextEdit *storyText = nullptr;
    QCheckBox *storyIsReal = nullptr;
    QWidget* familyPage();
    void refreshFamily();
    void updateTabRecommendations();
    QWidget* storyPage();
    void loadStory();
    void saveStory();
    bool prepareVibeHandoff(const QString &instructionFile);
    QWidget* vibePage();
    QWidget* publishingPage();
    QWidget* dragonPage();
    QWidget* modifyPage();
    QWidget* restorePage();
    QWidget* webPage();
    QWidget* botPage();
    void startBot();
    void stopBot();
    void updateBotStatus();
    QProcess *botProcess = nullptr;
    QLabel *botStatusLabel = nullptr;
    QString botDetail;
    void showChronicleOverlay();
    void refreshChronicleOverlay();
    QLabel *gameStatus = nullptr;
    QLabel *secretsStatus = nullptr;
    QTextEdit *secretsOutput = nullptr;
    bool secretsRunning = false;
    bool secretsAlert = false;
    bool secretsFlash = false;
    QString dataFile() const;
    QString treeFile() const;
    void makeUi();
    QWidget* pendingPage(const QString &name, const QString &message);
    QWidget* kinPage();
    QWidget* chroniclePage();
    void rebuildChronicle();
    void checkSourceUpdates();
    QWidget* diabloPage();
    QWidget* secretsPage();
    void runSecretsCheck();
    QWidget* settingsPage();
    QWidget* logPage();
    void loadKinMap();
    void rebuildPeople();
    void inspectPerson();
    void nextAncestor();
    void updateFeatured();
    void refreshPublicPreview();
    void writeCheckpoint(const QString &message);
    void persistSession(const QString &name);
    QString codeFor(const QString &name) const;
    QString personDetails(const QString &name) const;
    bool restricted(const QJsonObject &person) const;
    QJsonObject personRecord(const QString &name) const;
};
