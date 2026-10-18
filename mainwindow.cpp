#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QLockFile>
#include <QSplitter>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QProcess>
#include <QCoreApplication>
#include <QTreeWidget>
#include <QComboBox>
#include <QSet>
#include <QMap>
#include <QMessageBox>
#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QDesktopServices>
#include <QUrl>
#include <QCryptographicHash>
#include <QTimeZone>
#include <QFileDevice>
#include <QDirIterator>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFileInfo>
#include <QClipboard>
#include <QProcess>
#include <QCloseEvent>

namespace {
const QString kEdward = QStringLiteral("TG307645");
const QStringList kProvenance = {"TG323932", "TG307645", "TG654890", "TG654147"};
QString stateDirectory() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.local/share/VibePedigree";
    QDir().mkpath(base);
    return base;
}
QString maskedLocalTime(const QDateTime &value) {
    if (!value.isValid()) return QStringLiteral("Unknown time");
    QDateTime d = value.toTimeZone(QTimeZone("America/Los_Angeles"));
    const int t = d.time().hour()*60 + d.time().minute();
    const int weekday = d.date().dayOfWeek();
    QString shown;
    if (t >= 23*60+55 || t < 3*60) shown = "11:55PM";
    else if ((weekday == 1 || weekday == 2) && t >= 7*60 && t < 10*60) shown = "7AM";
    else if ((weekday == 1 || weekday == 2) && t >= 11*60 && t < 14*60+30) shown.clear();
    else if ((weekday == 3 || weekday == 5) && t >= 7*60 && t < 11*60) shown = "7AM";
    else if ((weekday == 3 || weekday == 5) && t >= 11*60 && t < 17*60) shown.clear();
    else if (weekday == 4 && t >= 6*60+30 && t < 11*60) shown = "6:30AM";
    else if (weekday == 4 && t >= 11*60 && t < 16*60) shown.clear();
    else shown = d.time().toString("h:mmAP");
    return d.date().toString("yyyy-MM-dd") + (shown.isEmpty() ? QString() : " " + shown);
}
QString vpProjectRoot() {
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
}
QString vpInstructionsRoot() {
    return QDir(vpProjectRoot()).filePath("instructions");
}
QString dbPath() {
    const QString root = QDir::homePath()+"/.local/share/vibepedigree";
    QDir().mkpath(root); QFile::setPermissions(root, QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
    return root + "/phase2.db";
}
QString fixedTreePath() {
    return QDir::homePath()+"/.VibePedigree/kinmap_family_tree.json";
}
QLabel* paragraph(const QString &text) {
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    return label;
}
}

// VP_PEDIGREE_STARTUP_STATUS_V1
void writePedigreeStartupStatus() {
    // Round *up* to the next 5-minute boundary in local time (exact boundaries stay unchanged).
    const qint64 unixSeconds = QDateTime::currentDateTime().toSecsSinceEpoch();
    const qint64 roundedSeconds = ((unixSeconds + 299) / 300) * 300;
    const QString timeText = QDateTime::fromSecsSinceEpoch(roundedSeconds)
                                 .toLocalTime().toString("h:mmAP");
    const QString statusPath = QDir::tempPath() + "/pedigree_status.json";
    QLockFile lock(statusPath + ".qtlock");
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(3000)) { qWarning() << "Status file lock busy"; return; }
    QJsonObject payload;
    QFile existing(statusPath);
    if (existing.exists()) {
        if (QFileInfo(existing).isSymLink() || !existing.open(QIODevice::ReadOnly)) {
            qWarning() << "Unsafe status file; refusing overwrite"; return;
        }
        QJsonParseError error;
        const QJsonDocument old = QJsonDocument::fromJson(existing.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !old.isObject()) {
            qWarning() << "Invalid status JSON; refusing overwrite"; return;
        }
        payload = old.object();
    }
    payload.insert("VibePedigree_Main", timeText);
    QSaveFile file(statusPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write VibePedigree startup status:" << file.errorString();
        return;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Indented);
    if (file.write(body) != body.size() || !file.commit())
        qWarning() << "Cannot commit VibePedigree startup status:" << file.errorString();
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    writePedigreeStartupStatus();
    auto storyDb=QSqlDatabase::addDatabase("QSQLITE","stories");
    storyDb.setDatabaseName(dbPath());
    if(storyDb.open()) {QSqlQuery q(storyDb);q.exec("CREATE TABLE IF NOT EXISTS stories(code TEXT PRIMARY KEY,body TEXT NOT NULL,is_real INTEGER NOT NULL DEFAULT 0,edited_at TEXT NOT NULL)");
        QFile::setPermissions(dbPath(),QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    }
    makeUi();
    startBot();
    loadKinMap();
    watcher = new QFileSystemWatcher(this);
    if (QFile::exists(dataFile())) watcher->addPath(dataFile());
    connect(watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        status->setText("KinMap state has changed on disk. Click Reload KinMap to refresh safely.");
        writeCheckpoint("KinMap source changed; waiting for manual reload");
        if (QFile::exists(dataFile()) && !watcher->files().contains(dataFile())) watcher->addPath(dataFile());
    });
    writeCheckpoint("Application started; Edward McCabe NPC prototype available (not in game)");
    auto *syncTimer = new QTimer(this);
    connect(syncTimer, &QTimer::timeout, this, &MainWindow::checkSourceUpdates);
    syncTimer->start(15000);
    QTimer::singleShot(0, this, [this]{ runSecretsCheck(); });
    auto *secretTimer = new QTimer(this);
    connect(secretTimer, &QTimer::timeout, this, &MainWindow::runSecretsCheck);
    secretTimer->start(5 * 60 * 1000);
    auto *flashTimer = new QTimer(this);
    connect(flashTimer, &QTimer::timeout, this, [this]{
        if (!secretsAlert) return;
        secretsFlash = !secretsFlash;
        QString st=styleSheet();
        st.replace(" QMainWindow{border:7px solid #f52222;} ", "");
        if (secretsFlash) st += " QMainWindow{border:7px solid #f52222;} ";
        setStyleSheet(st);
    });
    flashTimer->start(700);
}
MainWindow::~MainWindow() { stopBot(); delete ui; }
void MainWindow::closeEvent(QCloseEvent *event) { stopBot(); QMainWindow::closeEvent(event); }
QString MainWindow::treeFile() const {
    return fixedTreePath();
}
QString MainWindow::dataFile() const {
    return "/var/lib/kinmap/kinmap_state.json";
}

void MainWindow::makeUi() {
    auto *layout = new QHBoxLayout(ui->centralwidget);
    layout->setContentsMargins(12, 12, 12, 12);
    nav = new QListWidget(this);
    nav->setFixedWidth(205);
    const QStringList sections = {"Dashboard", "Mistral Vibe", "Diablo", "KinMap", "Family Chronicle", "Moltbook", "Publishing", "Dragon Pipe", "Privacy", "System Logs", "Settings", "Secrets Autorun", "Family", "Story", "Modify Game", "Restore", "Web", "Bot"};
    nav->addItems(sections);
    pages = new QStackedWidget(this);
    auto *dashboard = new QWidget(this);
    auto *dashLayout = new QVBoxLayout(dashboard);
    auto *heading = paragraph("VibePedigree — local control center");
    heading->setStyleSheet("font-size:24px;font-weight:700;");
    dashLayout->addWidget(heading);
    dashLayout->addWidget(paragraph("Family Chronicle: read-only KinMap synchronization, a floating windowed-game companion, and local DevilutionX launch. No ports, modifications or public transmissions."));
    status = paragraph("Checking genealogy data...");
    dashLayout->addWidget(status);
    dashLayout->addWidget(paragraph("TG metadata: " + kProvenance.join(", ")));
    dashLayout->addStretch();
    pages->addWidget(dashboard);
    pages->addWidget(vibePage());
    pages->addWidget(diabloPage());
    pages->addWidget(kinPage());
    pages->addWidget(chroniclePage());
    pages->addWidget(pendingPage("Moltbook", "Drafting, approvals and publishing are pending. No information is transmitted."));
    pages->addWidget(publishingPage());
    pages->addWidget(dragonPage());
    pages->addWidget(pendingPage("Privacy", "Public by default. Birth years 1981, 1960, 1926, 1928 are blocked. Publishing still requires explicit action.\nSee the KinMap tab for private and public previews."));
    pages->addWidget(logPage());
    pages->addWidget(settingsPage());
    pages->addWidget(secretsPage());
    pages->addWidget(familyPage());
    pages->addWidget(storyPage());
    pages->addWidget(modifyPage());
    pages->addWidget(restorePage());
    pages->addWidget(webPage());
    pages->addWidget(botPage());
    for (const QString &hidden : {"KinMap", "Family Chronicle", "Moltbook", "Privacy", "Settings"}) {
        for (int i=0;i<nav->count();++i) if(nav->item(i)->text()==hidden) nav->item(i)->setHidden(true);
    }

    layout->addWidget(nav);
    layout->addWidget(pages, 1);
    connect(nav, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    connect(nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && nav->item(row)->text() == QStringLiteral("Family")) {
            const QFileInfo journal(QDir::homePath() + "/.Diablo/runtime/family-drops.tsv");
            if (journal.exists()) settings.setValue("phase2/familyJournalReviewedMs", journal.lastModified().toMSecsSinceEpoch());
        }
        updateTabRecommendations();
    });
    auto *botTimer = new QTimer(this);
    connect(botTimer, &QTimer::timeout, this, &MainWindow::updateBotStatus);
    botTimer->start(2000);
    auto *cueTimer = new QTimer(this);
    connect(cueTimer, &QTimer::timeout, this, &MainWindow::updateTabRecommendations);
    cueTimer->start(15000);

    nav->setCurrentRow(0);
    updateTabRecommendations();
    setStyleSheet("QMainWindow,QWidget{background:#16202e;color:#e6edf6;} QListWidget,QTextEdit,QLineEdit{background:#222f42;color:#f2f7fc;border:1px solid #41536a;border-radius:6px;padding:7px;} QPushButton{background:#355b90;color:white;border:0;border-radius:6px;padding:9px;} QPushButton:hover{background:#4779bd;} QPushButton:disabled{background:#384353;color:#aab2bd;} QLabel{padding:3px;} QListWidget::item:selected{background:#355b90;}");
    setWindowTitle("VibePedigree — Phase 2");
    statusBar()->showMessage("VibePedigree Phase 2 · clipboard handoff only; Vibe never started by Qt");
}
void MainWindow::updateTabRecommendations() {
    if (!nav) return;
    // A recommendation is an actionable cue, not a prediction or an instruction.
    const QFileInfo journal(QDir::homePath() + "/.Diablo/runtime/family-drops.tsv");
    const qint64 lastWrite = journal.exists() ? journal.lastModified().toMSecsSinceEpoch() : 0;
    const qint64 lastReview = settings.value("phase2/familyJournalReviewedMs", 0).toLongLong();
    const bool familyNeedsReview = lastWrite > lastReview;

    // Suggest writing a story for a recovered scroll only when no local story exists.
    bool storyNeedsAttention = false;
    QFile input(journal.filePath());
    if (journal.exists() && input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QRegularExpression codeRx("\\b(?:TG[0-9]+|AKA_TE[0-9]+)\\b");
        QSet<QString> codes;
        while (!input.atEnd()) {
            const QString line = QString::fromUtf8(input.readLine());
            if (!line.contains("DROP", Qt::CaseInsensitive)) continue;
            const auto match = codeRx.match(line);
            if (match.hasMatch()) codes.insert(match.captured());
        }
        auto db = QSqlDatabase::database("stories", false);
        if (db.isOpen()) {
            QSqlQuery query(db);
            query.prepare("SELECT 1 FROM stories WHERE code=? AND TRIM(body)<>'' LIMIT 1");
            for (const QString &code : codes) {
                query.bindValue(0, code);
                if (query.exec() && !query.next()) { storyNeedsAttention = true; break; }
                query.finish();
            }
        }
    }
    for (int i=0; i<nav->count(); ++i) {
        auto *item = nav->item(i);
        const QString name = item->text();
        if (name == "Bot") {
            const bool live = botProcess && botProcess->state() == QProcess::Running;
            item->setBackground(QBrush(QColor(live ? "#36b76f" : "#e3575f")));
            item->setForeground(QBrush(QColor("#101923")));
            QFont font=item->font();font.setBold(true);item->setFont(font);
            item->setToolTip(live ? "Local Diablo watcher running" : "Local Diablo watcher stopped or failed");
            continue;
        }
        const bool prompt = (name == "Family" && familyNeedsReview) ||
                            (name == "Story" && storyNeedsAttention);
        const bool selected = nav->currentRow() == i;
        // Leave selected-item styling alone; selected navigation remains dark blue.
        item->setBackground(!selected && prompt ? QBrush(QColor("#83c9f4")) : QBrush());
        item->setForeground(!selected && prompt ? QBrush(QColor("#112438")) : QBrush());
        QFont font = item->font(); font.setBold(prompt); item->setFont(font);
        if (name == "Family") item->setToolTip(familyNeedsReview ?
            "New or updated book-drop journal since you last opened Family" : QString());
        if (name == "Story") item->setToolTip(storyNeedsAttention ?
            "At least one acquired book has no saved story yet" : QString());
    }
}

QWidget* MainWindow::pendingPage(const QString &name, const QString &message) {
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    auto *title = paragraph(name);
    title->setStyleSheet("font-size:23px;font-weight:bold;");
    v->addWidget(title);
    v->addWidget(paragraph(message));
    v->addWidget(paragraph("Status: Pending — this feature is intentionally not connected in Phase 1."));
    auto *btn = new QPushButton("Not implemented (Phase 1)", w);
    btn->setEnabled(false);
    v->addWidget(btn);
    v->addStretch();
    return w;
}
QWidget* MainWindow::kinPage() {
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    v->addWidget(paragraph("KinMap — local genealogy (read only)"));
    auto *line = new QHBoxLayout;
    searchInput = new QLineEdit(w);
    searchInput->setPlaceholderText("Search family members or TG code");
    auto *reload = new QPushButton("Reload KinMap", w);
    line->addWidget(searchInput, 1);
    line->addWidget(reload);
    v->addLayout(line);
    auto *split = new QSplitter(w);
    ancestors = new QListWidget(w);
    split->addWidget(ancestors);
    auto *detail = new QWidget(w);
    auto *dv = new QVBoxLayout(detail);
    featured = paragraph("Featured: waiting for genealogy data");
    dv->addWidget(featured);
    auto *next = new QPushButton("Next eligible ancestor", detail);
    dv->addWidget(next);
    recordDetail = paragraph("Select a family member to inspect their details and relationships.");
    dv->addWidget(recordDetail);
    manualApproval = new QCheckBox("Manually mark selected person private (local preview only)", detail);
    dv->addWidget(manualApproval);
    publicPreview = new QTextEdit(detail);
    publicPreview->setReadOnly(true);
    dv->addWidget(paragraph("Public preview (not transmitted):"));
    dv->addWidget(publicPreview);
    sessionHistory = new QTextEdit(detail);
    sessionHistory->setReadOnly(true);
    dv->addWidget(paragraph("Featured ancestor history (local):"));
    dv->addWidget(sessionHistory);
    split->addWidget(detail);
    split->setStretchFactor(1, 2);
    v->addWidget(split, 1);
    connect(reload, &QPushButton::clicked, this, &MainWindow::loadKinMap);
    connect(next, &QPushButton::clicked, this, &MainWindow::nextAncestor);
    connect(ancestors, &QListWidget::currentRowChanged, this, [this](int){ if(manualApproval) manualApproval->setChecked(false); inspectPerson(); });
    connect(manualApproval, &QCheckBox::toggled, this, &MainWindow::refreshPublicPreview);
    connect(searchInput, &QLineEdit::textChanged, this, &MainWindow::rebuildPeople);
    return w;
}

QWidget* MainWindow::diabloPage() {
    auto *w = new QWidget(this); auto *v = new QVBoxLayout(w);
    v->addWidget(paragraph("DevilutionX — local launcher and NPC workshop"));
    v->addWidget(paragraph("Launches the existing custom local Diablo game in ~/.Diablo. The original Flatpak and saves are not modified."));
    gameStatus = paragraph("Checking installation..."); v->addWidget(gameStatus);
    auto *check = new QPushButton("Check DevilutionX",w);
    auto *launch = new QPushButton("Launch Diablo",w);
    check->hide();
    auto *chronicle = new QPushButton("Open floating Family Chronicle (windowed Diablo)",w);
    v->addWidget(check); v->addWidget(launch); v->addWidget(chronicle); chronicle->hide();
    v->addWidget(paragraph("The Chronicle is a local companion window, not yet an in-engine Diablo menu. Use windowed mode to see it alongside the game. No MPQs or saves are changed."));
    auto *workshop = new QPushButton("Edward McCabe — NPC dialogue workshop (prototype)",w);
    v->addWidget(workshop); workshop->hide();
    connect(workshop, &QPushButton::clicked, this, [this] {
        auto *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("Edward McCabe — local NPC workshop (NOT in Diablo)");
        dialog->resize(710,485);
        auto *dv = new QVBoxLayout(dialog);
        dv->addWidget(paragraph("Experimental dialogue for Edward McCabe (TG307645). This is a VibePedigree preview—not a character inside DevilutionX. Relationships come from KinMap; fantasy dialogue is fictional."));
        auto *response = new QTextEdit(dialog); response->setReadOnly(true); dv->addWidget(response,1);
        QMap<QString,QStringList> parents,children;
        for (const auto &entry : relationships) {
            const auto r=entry.toObject();
            QString p=r.value("parent").toString(), c=r.value("child").toString();
            if (!p.isEmpty() && !c.isEmpty()) { if(!parents[c].contains(p))parents[c]<<p; if(!children[p].contains(c))children[p]<<c; }
        }
        QString person;
        for (const auto &entry : people) if (codeFor(entry.toObject().value("display_name").toString()).compare(QStringLiteral("TG307645"),Qt::CaseInsensitive)==0) {person=entry.toObject().value("display_name").toString();break;}
        if(person.isEmpty()) person="Edward Mccabe";
        const QString selected=person;
        const auto describe=[selected,parents,children](const QString &action) {
            if(action=="family") {
                const QString parentText=parents.value(selected).isEmpty()?"No parents are documented in the loaded source.":parents.value(selected).join(", ");
                const QString childText=children.value(selected).isEmpty()?"No children are documented in the loaded source.":children.value(selected).join(", ");
                return QString("KINMAP RECORDS\nPerson: %1\nParents: %2\nChildren: %3\n\nThese are recorded connections, not fictional gameplay events.").arg(selected,parentText,childText);
            }
            if(action=="story")return QString("FICTIONAL GAME DIALOGUE (NOT HISTORICAL EVIDENCE)\n%1: 'Traveler, every generation leaves a trail. Find the names that connect us, and the Chronicle will guide your path.'").arg(selected);
            return QString("FICTIONAL GAME DIALOGUE\n%1: 'Greetings, Sorcerer we6jbo. I am recorded in your family's Chronicle as TG307645. Ask about my kin, or follow the story.'").arg(selected);
        };
        response->setPlainText(describe("hello"));
        auto *buttons=new QHBoxLayout();
        auto *greet=new QPushButton("Greet",dialog),*family=new QPushButton("Ask about family",dialog),*story=new QPushButton("Ask for a quest",dialog);
        buttons->addWidget(greet);buttons->addWidget(family);buttons->addWidget(story);dv->addLayout(buttons);
        connect(greet,&QPushButton::clicked,dialog,[response,describe]{response->setPlainText(describe("hello"));});
        connect(family,&QPushButton::clicked,dialog,[response,describe]{response->setPlainText(describe("family"));});
        connect(story,&QPushButton::clicked,dialog,[response,describe]{response->setPlainText(describe("story"));});
        auto *exportButton=new QPushButton("Export local NPC prototype JSON (not for publishing)",dialog);
        dv->addWidget(exportButton);
        connect(exportButton,&QPushButton::clicked,dialog,[this,dialog,selected,parents,children,describe,response]{
            QString path=stateDirectory()+"/npc-prototypes";
            QDir().mkpath(path);
            path+="/edward-mccabe.json";
            QJsonObject obj{{"schema_version",1},{"prototype_only",true},{"engine_integrated",false},
                {"character_name",selected},{"tg_code",QStringLiteral("TG307645")},
                {"parents",QJsonArray::fromStringList(parents.value(selected))},
                {"children",QJsonArray::fromStringList(children.value(selected))},
                {"greeting",describe("hello")},{"fictional_quest",describe("story")},
                {"publication_status",QStringLiteral("REVIEW REQUIRED")}};
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented))<0 || !file.commit()) {
                QMessageBox::warning(dialog,"Export failed","Could not write local prototype JSON.");return;
            }
            response->append("\nLOCAL PROTOTYPE EXPORTED TO:\n"+path+"\nPublication still requires individual review.");
            writeCheckpoint("Edward McCabe local NPC prototype exported; no engine/game files modified");
        });
        dialog->show();
        writeCheckpoint("Edward McCabe local NPC dialogue prototype opened; no Diablo changes");
    });

    v->addStretch();
    connect(chronicle,&QPushButton::clicked,this,&MainWindow::showChronicleOverlay);
    auto verify=[this]() {
        auto *p=new QProcess(this);
        connect(p, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this,p](int rc,QProcess::ExitStatus es){
            gameStatus->setText(rc==0 && es==QProcess::NormalExit ?
                "Original Flatpak detected: " + QString::fromUtf8(p->readAllStandardOutput()).trimmed() :
                "Original Flatpak not detected; custom launcher is independent.");p->deleteLater();});
        p->start("flatpak",{"info","--user","--show-version","org.diasurgical.DevilutionX"});
    };
    connect(check,&QPushButton::clicked,this,verify);
    connect(launch,&QPushButton::clicked,this,[this] {
        if (QProcess::startDetached("bash",{QDir::homePath()+"/.Diablo/launch_edward_v23_debug.sh"},QDir::homePath()+"/.Diablo")) {
            gameStatus->setText("Custom Diablo launch requested; switch to its window to verify gameplay.");
            writeCheckpoint("User launched custom local Diablo launcher");
        } else gameStatus->setText("Unable to launch custom Diablo script. Check ~/.Diablo launcher.");
    });
    QTimer::singleShot(0,this,verify);
    return w;
}
QWidget* MainWindow::chroniclePage() {
    auto *w=new QWidget(this);auto *v=new QVBoxLayout(w);
    v->addWidget(paragraph("Family Chronicle — connected family branches (read only)"));
    v->addWidget(paragraph("Choose a family member to explore their documented parents recursively. The tree never invents links. People with multiple parental connections can appear on multiple branches."));
    auto *line=new QHBoxLayout();
    chronicleRoot=new QComboBox(w);chronicleRoot->setEditable(false);
    auto *refresh=new QPushButton("Refresh Chronicle",w);
    auto *overlayButton=new QPushButton("Floating Chronicle",w);
    line->addWidget(chronicleRoot,1);line->addWidget(refresh);line->addWidget(overlayButton);
    connect(overlayButton,&QPushButton::clicked,this,&MainWindow::showChronicleOverlay);
    v->addLayout(line);
    chronicleSummary=paragraph("Loading ancestry...");v->addWidget(chronicleSummary);
    chronicleTree=new QTreeWidget(w);chronicleTree->setHeaderLabels({"Family member (child → parents)","TG / AKA", "Visibility"});
    chronicleTree->setColumnWidth(0,340);v->addWidget(chronicleTree,1);
    connect(chronicleRoot,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){rebuildChronicle();});
    connect(refresh,&QPushButton::clicked,this,&MainWindow::loadKinMap);
    return w;
}
void MainWindow::rebuildChronicle() {
    if(!chronicleTree || !chronicleRoot || !chronicleSummary) return;
    QString previous=chronicleRoot->currentData().toString();
    chronicleRoot->blockSignals(true);chronicleRoot->clear();
    QSet<QString> names;
    for(auto value:people){QString n=value.toObject().value("display_name").toString();if(!n.isEmpty())names.insert(n);}
    for(auto value:relationships){auto r=value.toObject();
        if(!r.value("parent").toString().isEmpty()) names.insert(r.value("parent").toString());
        if(!r.value("child").toString().isEmpty()) names.insert(r.value("child").toString());
    }
    QStringList ordered=names.values();ordered.sort(Qt::CaseInsensitive);
    for(const auto &n:ordered)chronicleRoot->addItem(n+" — "+codeFor(n),n);
    int selected=chronicleRoot->findData(previous);
    if(selected<0)selected=chronicleRoot->findData("Edward Mccabe");
    if(selected<0 && chronicleRoot->count())selected=0;
    chronicleRoot->setCurrentIndex(selected);chronicleRoot->blockSignals(false);
    chronicleTree->clear();
    if(selected<0){chronicleSummary->setText("No family records loaded.");return;}
    QMap<QString,QStringList> parentMap;
    QMap<QString,QSet<QString>> neighbors;
    int valid=0;
    for(auto value:relationships){auto r=value.toObject();
        QString p=r.value("parent").toString(),c=r.value("child").toString();
        if(p.isEmpty()||c.isEmpty())continue;
        valid++;if(!parentMap[c].contains(p))parentMap[c].append(p);
        neighbors[p].insert(c);neighbors[c].insert(p);
    }
    int components=0;QSet<QString> covered;
    for(const QString &n:ordered){if(covered.contains(n))continue;components++;
        QStringList queue{n};covered.insert(n);
        for(int i=0;i<queue.size();++i)for(const auto &next:neighbors.value(queue[i]))if(!covered.contains(next)){covered.insert(next);queue.append(next);}
    }
    auto *root = new QTreeWidgetItem(chronicleTree);
    QString focus=chronicleRoot->currentData().toString();
    std::function<void(QTreeWidgetItem*,const QString&,QSet<QString>,int)> grow;
    grow=[this,&grow,&parentMap](QTreeWidgetItem *item,const QString &name,QSet<QString> visited,int depth){
        item->setText(0,name);item->setText(1,codeFor(name));
        auto record=personRecord(name);
        item->setText(2,restricted(record)?"PRIVATE (protected birth year)":"PUBLIC eligible");
        if(depth>=25){new QTreeWidgetItem(item,{"Depth limit (25)","",""});return;}
        visited.insert(name);
        for(const auto &parent:parentMap.value(name)){
            auto *child=new QTreeWidgetItem(item);
            if(visited.contains(parent)){child->setText(0,parent+" (cycle detected)");continue;}
            grow(child,parent,visited,depth+1);
        }
    };
    grow(root,focus,{},0);chronicleTree->expandToDepth(2);
    chronicleSummary->setText(QString("%1 named records · %2 recorded parent/child links · %3 connected component(s). Unlinked people are included in the selector. Updates checked every 15 seconds. No game NPCs created yet.")
        .arg(ordered.size()).arg(valid).arg(components));
    refreshChronicleOverlay();
}
void MainWindow::showChronicleOverlay() {
    if (!chronicleOverlay) {
        auto *dialog=new QDialog(this,Qt::Window | Qt::WindowStaysOnTopHint);
        dialog->setAttribute(Qt::WA_DeleteOnClose,false);
        dialog->setWindowTitle("VibePedigree — Diablo Family Chronicle (local)");
        dialog->resize(730,530);
        auto *layout=new QVBoxLayout(dialog);
        auto *intro=paragraph("Local read-only genealogy companion. Select an ancestor to examine documented parents. No data is sent to the game or the internet.");
        layout->addWidget(intro);
        overlayRoot=new QComboBox(dialog);
        layout->addWidget(overlayRoot);
        overlaySummary=paragraph("");layout->addWidget(overlaySummary);
        overlayTree=new QTreeWidget(dialog);
        overlayTree->setHeaderLabels({"Family member (child → parents)","TG / AKA","Visibility"});
        overlayTree->setColumnWidth(0,325);layout->addWidget(overlayTree,1);
        auto *hide=new QPushButton("Hide Chronicle",dialog);
        layout->addWidget(hide);
        connect(hide,&QPushButton::clicked,dialog,&QDialog::hide);
        connect(overlayRoot,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int n){
            if(n<0 || !chronicleRoot) return;
            const int mainIndex=chronicleRoot->findData(overlayRoot->itemData(n));
            if(mainIndex>=0)chronicleRoot->setCurrentIndex(mainIndex);
            // If index did not change, still refresh the visual copy.
            refreshChronicleOverlay();
        });
        chronicleOverlay=dialog;
    }
    refreshChronicleOverlay();
    chronicleOverlay->show();chronicleOverlay->raise();chronicleOverlay->activateWindow();
    writeCheckpoint("User opened local floating Family Chronicle; game assets untouched");
}
void MainWindow::refreshChronicleOverlay() {
    if(!overlayRoot || !overlayTree || !chronicleRoot || !chronicleTree || !overlaySummary)return;
    const QString current=chronicleRoot->currentData().toString();
    overlayRoot->blockSignals(true);overlayRoot->clear();
    for(int i=0;i<chronicleRoot->count();++i)
        overlayRoot->addItem(chronicleRoot->itemText(i),chronicleRoot->itemData(i));
    overlayRoot->setCurrentIndex(overlayRoot->findData(current));
    overlayRoot->blockSignals(false);
    overlaySummary->setText(chronicleSummary->text());
    overlayTree->clear();
    for(int i=0;i<chronicleTree->topLevelItemCount();++i)
        overlayTree->addTopLevelItem(chronicleTree->topLevelItem(i)->clone());
    overlayTree->expandToDepth(2);
}
void MainWindow::checkSourceUpdates() {
    const QDateTime state=QFileInfo(dataFile()).lastModified();
    const QDateTime tree=QFileInfo(treeFile()).lastModified();
    if(state!=stateSeen || tree!=treeSeen){
        writeCheckpoint("KinMap change detected; refreshing Family Chronicle from read-only source");
        loadKinMap();
        if(watcher && QFile::exists(dataFile()) && !watcher->files().contains(dataFile()))watcher->addPath(dataFile());
    }
}
QWidget* MainWindow::settingsPage() {
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    v->addWidget(paragraph("Settings — KinMap is read-only; local state lives under the standard per-user app data folder."));
    pathInput = new QLineEdit(dataFile(), w);
    auto *choose = new QPushButton("Browse JSON", w);
    auto *save = new QPushButton("Save path and reload", w);
    v->addWidget(paragraph("KinMap state JSON (read-only):"));
    v->addWidget(pathInput);
    v->addWidget(paragraph("Family-tree export JSON (read-only):"));
    treeInput = new QLineEdit(treeFile(), w);
    v->addWidget(treeInput);
    v->addWidget(choose);
    v->addWidget(save);
    pathLabel = paragraph("Current source: " + dataFile());
    v->addWidget(pathLabel);
    v->addStretch();
    connect(choose, &QPushButton::clicked, this, [this] {
        QString choice = QFileDialog::getOpenFileName(this, "Select KinMap JSON", QFileInfo(dataFile()).absolutePath(), "JSON (*.json)");
        if (!choice.isEmpty()) pathInput->setText(choice);
    });
    connect(save, &QPushButton::clicked, this, [this] {
        settings.setValue("kinmap/path", pathInput->text().trimmed());
        settings.setValue("kinmap/tree_path", treeInput->text().trimmed());
        settings.sync();
        pathLabel->setText("Current source: " + dataFile());
        writeCheckpoint("Saved KinMap path setting");
        loadKinMap();
        if (watcher) { watcher->removePaths(watcher->files()); if(QFile::exists(dataFile())) watcher->addPath(dataFile()); }
    });
    return w;
}
QWidget* MainWindow::logPage() {
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    v->addWidget(paragraph("Persistent local checkpoints — " + stateDirectory() + "/checkpoints.log"));
    logs = new QTextEdit(w);
    logs->setReadOnly(true);
    QFile file(stateDirectory()+"/checkpoints.log");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) logs->setPlainText(QString::fromUtf8(file.readAll()));
    v->addWidget(logs);
    auto *reload = new QPushButton("Refresh log", w);
    v->addWidget(reload);
    connect(reload, &QPushButton::clicked, this, [this] {
        QFile f(stateDirectory()+"/checkpoints.log");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) logs->setPlainText(QString::fromUtf8(f.readAll()));
    });
    return w;
}
void MainWindow::writeCheckpoint(const QString &message) {
    QString entry = QDateTime::currentDateTime().toString(Qt::ISODate) + " | " + message;
    qInfo().noquote() << "VibePedigree:" << entry;
    QFile log(stateDirectory() + "/checkpoints.log");
    if (log.open(QIODevice::Append | QIODevice::Text)) { log.write((entry+"\n").toUtf8()); log.close(); }
    if (logs) logs->append(entry.toHtmlEscaped());
    QSaveFile checkpoint(stateDirectory()+"/last-checkpoint.json");
    if (checkpoint.open(QIODevice::WriteOnly)) {
        QJsonObject obj{{"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)}, {"message", message}, {"project", "vibepedigree"}};
        checkpoint.write(QJsonDocument(obj).toJson());
        checkpoint.commit();
    }
}
void MainWindow::loadKinMap() {
    auto readObject = [](const QString &path, QJsonObject &root, QString &error) -> bool {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {error = "Cannot open " + path; return false;}
        QJsonParseError parse;
        auto doc = QJsonDocument::fromJson(f.readAll(), &parse);
        if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
            error = "Invalid JSON at " + path + ": " + parse.errorString(); return false;
        }
        root = doc.object(); return true;
    };
    QJsonObject tree, state;
    QString error;
    bool treeOk = readObject(treeFile(), tree, error);
    QString treeError = error; error.clear();
    bool stateOk = readObject(dataFile(), state, error);
    QString stateError = error;
    people = QJsonArray(); identifiers = QJsonArray(); relationships = QJsonArray();
    if (treeOk) {
        people = tree.value("people").toArray();
        identifiers = tree.value("identifiers").toArray();
        relationships = tree.value("relationships").toArray();
    }
    int locallyAdded = 0;
    if (stateOk) {
        QJsonArray local = state.value("local_people").toArray();
        for (auto value : local) {
            auto p = value.toObject();
            QString name = p.value("display_name").toString();
            if (name.isEmpty()) continue;
            bool alreadyThere = false;
            for (auto pv : people) if (pv.toObject().value("display_name").toString() == name) {alreadyThere = true; break;}
            if (!alreadyThere) { people.append(p); locallyAdded++; }
            auto codes = p.value("tg_codes").toArray();
            for (auto code : codes) identifiers.append(QJsonObject{{"display_name",name},{"kind","TG"},{"code",code.toString()}});
        }
        for (auto value : state.value("local_relationships").toArray()) relationships.append(value);
    }
    hasData=treeOk || stateOk;
    eligibleNames.clear();
    // Selection is LOCAL ONLY. Never interpret eligibility as publication permission.
    for (auto value : people) {
        auto person = value.toObject();
        auto name = person.value("display_name").toString();
        if (!name.isEmpty() && !restricted(person)) eligibleNames.append(name);
    }
    featuredIndex=-1;
    for (int i=0;i<eligibleNames.size();++i) if(codeFor(eligibleNames[i])==kEdward) {featuredIndex=i;break;}
    if (featuredIndex<0 && !eligibleNames.isEmpty()) featuredIndex=0;
    rebuildPeople(); updateFeatured();
    stateSeen = QFileInfo(dataFile()).lastModified();
    treeSeen = QFileInfo(treeFile()).lastModified();
    rebuildChronicle();
    QString report = QString("KinMap merged: %1 people, %2 relationships, %3 local additions, %4 candidate ancestors.\nState: %5\nTree: %6")
        .arg(people.size()).arg(relationships.size()).arg(locallyAdded).arg(eligibleNames.size())
        .arg(stateOk ? "loaded" : stateError).arg(treeOk ? "loaded" : treeError);
    status->setText(report);
    writeCheckpoint(QString("KinMap read: tree=%1 state=%2 people=%3 local=%4 relationships=%5")
        .arg(treeOk).arg(stateOk).arg(people.size()).arg(locallyAdded).arg(relationships.size()));
}
QString MainWindow::codeFor(const QString &name) const {
    for (const auto &item: identifiers) {
        auto obj=item.toObject();
        if(obj.value("display_name").toString().compare(name,Qt::CaseInsensitive)==0 && obj.value("kind").toString()=="TG") return obj.value("code").toString();
    }
    for (const auto &item: identifiers) {
        auto obj=item.toObject();
        if(obj.value("display_name").toString().compare(name,Qt::CaseInsensitive)==0) return obj.value("code").toString();
    }
    return "(no recorded identifier)";
}
QJsonObject MainWindow::personRecord(const QString &name) const {
    for(const auto &item:people) {auto p=item.toObject(); if(p.value("display_name").toString()==name) return p;}
    return {};
}
QString MainWindow::personDetails(const QString &name) const {
    for(const auto &item:people) {
        auto p=item.toObject();
        if(p.value("display_name").toString()==name) {
            QString d=p.value("details").toString();
            if(!p.value("birth_date").toString().isEmpty()) d += "\nBorn: " + p.value("birth_date").toString() + " " + p.value("birth_place").toString();
            if(!p.value("death_date").toString().isEmpty()) d += "\nDied: " + p.value("death_date").toString() + " " + p.value("death_place").toString();
            return d;
        }
    }
    return {};
}
bool MainWindow::restricted(const QJsonObject &person) const {
    QString details=person.value("details").toString();
    QString birthText=person.value("birth_date").toString();
    if (birthText.isEmpty()) {
        QRegularExpression marker(R"((?:^|\s)B\s*:\s*([^\n]*?)(?=\s+D\s*:|$))", QRegularExpression::CaseInsensitiveOption);
        auto m=marker.match(details); if(m.hasMatch()) birthText=m.captured(1);
    }
    if(birthText.isEmpty()) return false;
    // Conservative birth-year detection: first year in the stored details.
    // No year, an excluded year, or an unparseable first year => private.
    QRegularExpression birth(R"((1[6789]\d{2}|20\d{2})(?!\d))");
    auto match=birth.match(birthText);
    if(!match.hasMatch()) return false;
    int year=match.captured(1).toInt();
    return year==1981 || year==1960 || year==1926 || year==1928;
}
void MainWindow::rebuildPeople() {
    if(!ancestors) return;
    ancestors->clear();
    for(const auto &item:people) {
        QString name=item.toObject().value("display_name").toString();
        QString code=codeFor(name);
        if (name.contains(searchInput->text(),Qt::CaseInsensitive) || code.contains(searchInput->text(),Qt::CaseInsensitive)) {
            ancestors->addItem(name + " — " + code);
            ancestors->item(ancestors->count()-1)->setData(Qt::UserRole,name);
        }
    }
    if (ancestors->count()>0) ancestors->setCurrentRow(0);
}
void MainWindow::inspectPerson() {
    auto *item=ancestors->currentItem();
    if (!item) {recordDetail->setText("No selected family member.");refreshPublicPreview();return;}
    QString name=item->data(Qt::UserRole).toString();
    QStringList parents, children;
    for(const auto &value:relationships){
        QJsonObject r=value.toObject();
        if(r.value("child").toString()==name) parents << r.value("parent").toString();
        if(r.value("parent").toString()==name) children << r.value("child").toString();
    }
    QString details=personDetails(name);
    recordDetail->setText("Name: " + name + "\nCode: " + codeFor(name) + "\nRecorded details: " + details +
        "\nParents: " + parents.join(", ") + "\nChildren: " + children.join(", ") +
        "\nPrivacy: " + (restricted(personRecord(name))?"PRIVATE — protected birth year":"PUBLIC by default"));
    refreshPublicPreview();
}
void MainWindow::refreshPublicPreview() {
    if(!publicPreview || !ancestors) return;
    auto *item=ancestors->currentItem();
    if(!item) {publicPreview->setPlainText("REDACTED — no record selected");return;}
    QString name=item->data(Qt::UserRole).toString();
    if(restricted(personRecord(name)) || manualApproval->isChecked()) {
        publicPreview->setPlainText("REDACTED — protected birth year or manually hidden. Not transmitted.");
        return;
    }
    publicPreview->setPlainText("Public-eligible local preview (not transmitted):\n"+name+"\n"+codeFor(name)+"\n"+personDetails(name)+"\n\nNOT TRANSMITTED");
}
void MainWindow::persistSession(const QString &name) {
    QString filePath=stateDirectory()+"/featured-history.json";
    QJsonArray previous;
    QFile old(filePath);
    if(old.open(QIODevice::ReadOnly)){auto d=QJsonDocument::fromJson(old.readAll());if(d.isArray()) previous=d.array();}
    QJsonObject entry{{"name",name},{"code",codeFor(name)},{"timestamp",QDateTime::currentDateTime().toString(Qt::ISODate)}};
    previous.append(entry);
    QSaveFile out(filePath);
    if(out.open(QIODevice::WriteOnly)){out.write(QJsonDocument(previous).toJson());out.commit();}
    if(sessionHistory) {
        QStringList lines;
        for (const auto &v:previous) {auto e=v.toObject();lines<<e.value("timestamp").toString()+" — "+e.value("name").toString()+" ("+e.value("code").toString()+")";}
        sessionHistory->setPlainText(lines.join("\n"));
    }
}
void MainWindow::updateFeatured() {
    if(featuredIndex<0 || featuredIndex>=eligibleNames.size()){featured->setText("Featured: no eligible recorded ancestor");return;}
    QString name=eligibleNames.at(featuredIndex);
    featured->setText("Featured (private local session): " + name + " — " + codeFor(name));
    // Show persisted history without creating another session on every reload.
    QFile history(stateDirectory()+"/featured-history.json");
    if(history.open(QIODevice::ReadOnly)) {
        QJsonDocument d=QJsonDocument::fromJson(history.readAll());QStringList lines;
        for (const auto &e:d.array()) { auto o=e.toObject(); lines<<o.value("timestamp").toString()+" — "+o.value("name").toString(); }
        if(sessionHistory) sessionHistory->setPlainText(lines.join("\n"));
    }
}
void MainWindow::nextAncestor(){
    if(eligibleNames.isEmpty()) {writeCheckpoint("No eligible ancestor available for rotation");return;}
    featuredIndex=(featuredIndex+1)%eligibleNames.size();
    QString name=eligibleNames.at(featuredIndex);
    updateFeatured();
    persistSession(name);
    writeCheckpoint("Local featured ancestor advanced: "+name+" ("+codeFor(name)+")");
}

QWidget* MainWindow::secretsPage() {
    auto *w = new QWidget(this);
    auto *v = new QVBoxLayout(w);
    auto *title = paragraph("Secrets Autorun — local asset integrity and publication guard");
    title->setStyleSheet("font-size:22px;font-weight:700;");
    v->addWidget(title);
    v->addWidget(paragraph("Runs at application startup and every five minutes while VibePedigree is open. Checks local MPQ integrity and scans known project/publishing staging areas; does not upload or decrypt game assets."));
    secretsStatus = paragraph("Checking...");
    v->addWidget(secretsStatus);
    secretsOutput = new QTextEdit(w);
    secretsOutput->setReadOnly(true);
    v->addWidget(secretsOutput, 1);
    auto *button = new QPushButton("Recheck now", w);
    v->addWidget(button);
    connect(button, &QPushButton::clicked, this, &MainWindow::runSecretsCheck);
    return w;
}
void MainWindow::runSecretsCheck() {
    if (secretsRunning) return;
    secretsRunning = true;
    auto *proc = new QProcess(this);
    const QString script = QCoreApplication::applicationDirPath() + "/../secrets_guard.py";
    QString sourceScript = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../secrets_guard.py");
    if (!QFile::exists(sourceScript)) sourceScript = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../secrets_guard.py");
    if (!QFile::exists(sourceScript)) sourceScript = QStringLiteral("/home/we6jbo/Projects/VibePedigree/secrets_guard.py");
    proc->setProgram("python3");
    proc->setArguments({sourceScript, "check"});
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this,proc](int code, QProcess::ExitStatus state){
            const QString result = QString::fromUtf8(proc->readAllStandardOutput()+proc->readAllStandardError());
            if (secretsOutput) secretsOutput->setPlainText(result);
            const bool critical = (state != QProcess::NormalExit || code >= 2 || code < 0);
            const bool warning = (!critical && code == 1);
            if (secretsStatus) secretsStatus->setText(critical ? "CRITICAL — stop publishing and investigate immediately" :
                    warning ? "WARNING — local GOG installer recognized; do not publish it" : "SAFE — monitored checks completed");
            secretsAlert = critical;
            if (critical) {
                secretsFlash = false;
                statusBar()->showMessage("RED ALERT: Secrets Autorun detected a critical issue.");
            } else {
                QString st=styleSheet();st.replace(" QMainWindow{border:7px solid #f52222;} ","");setStyleSheet(st);
                statusBar()->showMessage(warning ? "Secrets Autorun WARNING: private GOG installer in Downloads" : "Secrets Autorun SAFE: local monitored checks passed");
            }
            writeCheckpoint(critical ? "Secrets Autorun CRITICAL (details in local report)" : warning ? "Secrets Autorun WARNING (local installer retained)" : "Secrets Autorun SAFE");
            secretsRunning=false;proc->deleteLater();
        });
    proc->start();
    if (!proc->waitForStarted(1500)) {
        secretsRunning=false;
        if(secretsStatus) secretsStatus->setText("ALERT — Python diagnostic could not start");
        secretsAlert=true;
        proc->deleteLater();
    }
}


QWidget* MainWindow::familyPage() {
    auto *w=new QWidget(this); auto *v=new QVBoxLayout(w);
    v->addWidget(paragraph("Family — individual TG and AKA book ledger (local only)"));
    v->addWidget(paragraph("A permanent check is recorded after a confirmed book DROP. Inventory and sale status require additional engine telemetry; unobserved entries remain unverified."));
    auto *reload=new QPushButton("Refresh family ledger",w);v->addWidget(reload);
    familyTable=new QTableWidget(w);
    familyTable->setColumnCount(5);
    familyTable->setHorizontalHeaderLabels({"Code","Name","Ever obtained","Current state","Last recorded event"});
    familyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    familyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->addWidget(familyTable,1);
    connect(reload,&QPushButton::clicked,this,&MainWindow::refreshFamily);
    QTimer::singleShot(0,this,&MainWindow::refreshFamily);
    auto *timer=new QTimer(w);connect(timer,&QTimer::timeout,this,&MainWindow::refreshFamily);timer->start(15000);
    return w;
}
void MainWindow::refreshFamily() {
    if(!familyTable) return;
    // The KinMap exported JSON is read-only and never packaged in a public build.
    QString tree=treeFile();QFile file(tree);
    QJsonArray ids;
    if(file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc=QJsonDocument::fromJson(file.readAll());
        ids=doc.object().value("identifiers").toArray();
    }
    struct Drop { QString when; QString location; };
    QMap<QString,Drop> observed;
    QFile journal(QDir::homePath()+"/.Diablo/runtime/family-drops.tsv");
    if(journal.open(QIODevice::ReadOnly|QIODevice::Text)) {
        while(!journal.atEnd()) {
            QString line=QString::fromUtf8(journal.readLine()).trimmed();
            QStringList cells=line.split('\t');
            // Check for DROP and exact TG/AKA tokens; do not infer sales or inventory.
            if(!line.contains("DROP",Qt::CaseInsensitive)) continue;
            const QRegularExpression codeRx("\\b(?:TG[0-9]+|AKA_TE[0-9]+)\\b");
            auto m=codeRx.match(line);if(!m.hasMatch()) continue;
            QString code=m.captured(0);
            // Journal layouts can differ; avoid presenting unvalidated raw timestamps/locations.
            observed[code]= {QString(),QString()};
        }
    }
    familyTable->setRowCount(ids.size());
    for(int i=0;i<ids.size();++i) {
        QJsonObject obj=ids.at(i).toObject();QString code=obj.value("code").toString();
        QString name=obj.value("display_name").toString();
        const bool got=observed.contains(code);
        QStringList values={code,name, got?"✓ Confirmed":"✕ Not yet confirmed",
                            got?"Dropped previously; current location unknown":"Unknown",
                            got?"Drop recorded (timestamp/location not yet validated)":"No confirmed event"};
        for(int c=0;c<values.size();++c) familyTable->setItem(i,c,new QTableWidgetItem(values.at(c)));
    }
    familyTable->setSortingEnabled(true);
}

QWidget* MainWindow::storyPage() {
    auto *w=new QWidget(this);auto *v=new QVBoxLayout(w);
    v->addWidget(paragraph("Story — private notes for each TG or AKA identifier"));
    storySelector=new QComboBox(w);v->addWidget(storySelector);
    storyText=new QTextEdit(w);storyText->setPlaceholderText("Write the story for the selected person...");v->addWidget(storyText,1);
    storyIsReal=new QCheckBox("This is a real-life story (copy to local real stories.db)",w);v->addWidget(storyIsReal);
    v->addWidget(paragraph("Checking 'real' is a local classification, NOT consent to publish or send to Moltbook."));
    auto *save=new QPushButton("Save story locally",w);v->addWidget(save);
    auto *dragon=new QPushButton("Copy Vibe story / dragon.html command",w);v->addWidget(dragon);
    connect(dragon,&QPushButton::clicked,this,[this]{prepareVibeHandoff("story.md");});
    QFile f(treeFile());if(f.open(QIODevice::ReadOnly)){
        const auto ids=QJsonDocument::fromJson(f.readAll()).object().value("identifiers").toArray();
        for(const auto &id:ids){auto ob=id.toObject(); storySelector->addItem(ob.value("display_name").toString()+" — "+ob.value("code").toString(),ob.value("code").toString());}
    }
    connect(storySelector,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){loadStory();});
    connect(save,&QPushButton::clicked,this,&MainWindow::saveStory);
    QTimer::singleShot(0,this,&MainWindow::loadStory);
    return w;
}
void MainWindow::loadStory() {
    if(!storyText || !storySelector)return;
    QSqlDatabase db=QSqlDatabase::database("stories");if(!db.isValid() || !db.isOpen())return;
    QSqlQuery q(db);q.prepare("SELECT body,is_real FROM stories WHERE code=?");q.addBindValue(storySelector->currentData().toString());
    if(q.exec() && q.next()){storyText->setPlainText(q.value(0).toString());storyIsReal->setChecked(q.value(1).toInt()==1);}
    else {storyText->clear();storyIsReal->setChecked(false);}
}
void MainWindow::saveStory() {
    QString code=storySelector->currentData().toString();if(code.isEmpty())return;
    QSqlDatabase db=QSqlDatabase::database("stories");if(!db.isOpen())return;
    QSqlQuery q(db);q.prepare("INSERT OR REPLACE INTO stories(code,body,is_real,edited_at) VALUES(?,?,?,?)");
    q.addBindValue(code);q.addBindValue(storyText->toPlainText());q.addBindValue(storyIsReal->isChecked()?1:0);
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if(!q.exec()){QMessageBox::warning(this,"Story not saved",q.lastError().text());return;}
    if(storyIsReal->isChecked()) {
        QString dir=QDir::homePath()+"/.family_story";QDir().mkpath(dir);
        QFile::setPermissions(dir,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        if(!QSqlDatabase::contains("realStories")) {
            auto real=QSqlDatabase::addDatabase("QSQLITE","realStories"); real.setDatabaseName(dir+"/real stories.db");
        }
        auto real=QSqlDatabase::database("realStories");
        if(!real.isOpen() && !real.open()){QMessageBox::warning(this,"Mirror failed",real.lastError().text());return;}
        QSqlQuery create(real);create.exec("CREATE TABLE IF NOT EXISTS real_stories(code TEXT PRIMARY KEY,body TEXT NOT NULL, edited_at TEXT NOT NULL, marked_real INTEGER NOT NULL)");
        QSqlQuery insert(real);insert.prepare("INSERT OR REPLACE INTO real_stories VALUES(?,?,?,1)");
        insert.addBindValue(code);insert.addBindValue(storyText->toPlainText());insert.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        if(!insert.exec()){QMessageBox::warning(this,"Mirror failed",insert.lastError().text());return;}
        QFile::setPermissions(dir+"/real stories.db",QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    } else {
        QString realPath=QDir::homePath()+"/.family_story/real stories.db";
        if(QFile::exists(realPath) && !QSqlDatabase::contains("realStories")) {
            auto realDb=QSqlDatabase::addDatabase("QSQLITE","realStories");realDb.setDatabaseName(realPath);realDb.open();
        }
        auto real=QSqlDatabase::database("realStories");if(real.isValid() && real.isOpen()){
            QSqlQuery q2(real);q2.prepare("UPDATE real_stories SET marked_real=0 WHERE code=?");q2.addBindValue(code);q2.exec();
        }
    }
    updateTabRecommendations();
    QMessageBox::information(this,"Saved","Story saved to private local storage. Nothing was published.");
}
bool MainWindow::prepareVibeHandoff(const QString &instructionFile) {
    const QString root=vpInstructionsRoot();
    if (!QDir(root).exists()) {
        QMessageBox::warning(this,"Instructions not found",root+" does not exist. Nothing was written.");return false;
    }
    const QStringList files={"init.md",instructionFile};
    for (const auto &f:files) {
        QFileInfo fi(root+"/"+f);
        if (!fi.isFile() || fi.isSymLink() || !fi.isReadable()) {
            QMessageBox::warning(this,"Missing instructions","Cannot read "+root+"/"+f+". Nothing was written.");return false;
        }
    }
    // Qt must NEVER launch Vibe or a terminal. Hand off a script for user-controlled execution.
    const QString script="/tmp/vibepedigree-vibe.sh";
    if (QFileInfo::exists(script) && QFileInfo(script).isSymLink()) {
        QMessageBox::warning(this,"Unsafe script path","Refusing to overwrite a symlink: "+script);return false;
    }
    const QString instruction="Read @"+root+"/init.md in full FIRST, before any other instructions or tool use. "
                              "Then read @"+root+"/"+instructionFile+" in full. "
                              "Follow both files in order; inspect and propose before modifying anything. "
                              "Do not modify frozen backups, original KinMap state or character saves. "
                              "Require user approval before external publication or restoration.";
    auto quote=[](QString a){return "'"+a.replace("'","'\\''")+"'";};
    QSaveFile f(script);
    if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) {QMessageBox::warning(this,"Script error",f.errorString());return false;}
    const QString body="#!/usr/bin/env bash\nset -euo pipefail\ncd "+quote(QDir::homePath()+"/.Diablo")+"\n"
        "[[ -r "+quote(root+"/init.md")+" && -r "+quote(root+"/"+instructionFile)+" ]] || { echo 'Required .md missing'; exit 1; }\n"
        "command -v vibe >/dev/null || { echo 'Mistral Vibe CLI not found on PATH'; exit 1; }\n"
        "exec vibe "+quote(instruction)+"\n";
    f.write(body.toUtf8());
    if(!f.commit()){QMessageBox::warning(this,"Script error","Cannot save temporary script");return false;}
    QFile::setPermissions(script,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
    const QString command="bash /tmp/vibepedigree-vibe.sh";
    QApplication::clipboard()->setText(command);
    QMessageBox::information(this,"Copied terminal command", "Overwrote "+script+" for "+instructionFile+".\n\nCopied to clipboard:\n"+command+"\n\nPaste into your own terminal. Qt has NOT started Vibe.");
    return true;
}
QWidget* MainWindow::vibePage() {
    auto *w=new QWidget(this);auto *v=new QVBoxLayout(w);
    v->addWidget(paragraph("Mistral Vibe — manual terminal handoff. Starts in ~/.Diablo. init.md is read first."));
    auto *b=new QPushButton("Copy Vibe initialization command",w);v->addWidget(b);
    connect(b,&QPushButton::clicked,this,[this]{prepareVibeHandoff("init.md");});v->addStretch();return w;
}
QWidget* MainWindow::publishingPage() {
    auto *w=new QWidget(this);auto *v=new QVBoxLayout(w);
    v->addWidget(paragraph("Publishing — review genuine project development through November 15, 2026. Produce drafts only; publication requires your approval."));
    auto *url=new QLineEdit(w);url->setPlaceholderText("Optional published j03.page URL (for social post updates)");
    url->setText(settings.value("phase2/blogUrl").toString());v->addWidget(url);
    auto *save=new QPushButton("Save blog URL locally",w);v->addWidget(save);
    connect(save,&QPushButton::clicked,this,[this,url]{settings.setValue("phase2/blogUrl",url->text().trimmed());QMessageBox::information(this,"URL saved","Stored locally. Mistral will read the URL from the publishing instructions or request it at review time.");});
    auto *b=new QPushButton("Copy Vibe publishing command",w);v->addWidget(b);
    connect(b,&QPushButton::clicked,this,[this]{prepareVibeHandoff("publishing.md");});v->addStretch();return w;
}
QWidget* MainWindow::dragonPage() {
    auto *w=new QWidget(this);auto *v=new QVBoxLayout(w);
    v->addWidget(paragraph("Dragon Pipe — prepare a privacy-reviewed Diablo 1 Let's Play for the Dragon-Kiehlbauch channel. No streaming starts automatically."));
    auto *b=new QPushButton("Copy Vibe Dragon Pipe command",w);v->addWidget(b);
    connect(b,&QPushButton::clicked,this,[this]{prepareVibeHandoff("dragon.md");});v->addStretch();return w;
}
QWidget* MainWindow::modifyPage() {
    auto *w=new QWidget(this);auto *v=new QVBoxLayout(w);
    v->addWidget(paragraph("Modify Game — Mistral Vibe working directory: ~/.Diablo. Read init.md FIRST, then mistral.md."));
    auto *b=new QPushButton("Copy Vibe Modify Game command",w);v->addWidget(b);
    connect(b,&QPushButton::clicked,this,[this]{prepareVibeHandoff("mistral.md");});v->addStretch();return w;
}
QWidget* MainWindow::restorePage() {
    auto *w=new QWidget(this);auto *v=new QVBoxLayout(w);
    v->addWidget(paragraph("Restore — safe snapshot comparison (read only)"));
    v->addWidget(paragraph("This Phase 2 implementation NEVER restores or deletes live files. It detects changed/missing reference files. Snapshot stays untouched."));
    auto *handoff=new QPushButton("Copy Vibe Restore command",w);v->addWidget(handoff);
    connect(handoff,&QPushButton::clicked,this,[this]{prepareVibeHandoff("restore.md");});
    auto *run=new QPushButton("Preview restore differences",w);v->addWidget(run);
    auto *details=new QTextEdit(w);details->setReadOnly(true);v->addWidget(details,1);
    connect(run,&QPushButton::clicked,this,[details]{
        const QString home=QDir::homePath(),snapshot=home+"/.local/share/vibepedigree-recovery/diablo-20261017-1325";
        const QString live=home+"/.Diablo";
        if(!QDir(snapshot).exists()){details->setText("Frozen local reference unavailable. Pi fallback requires a separate approved implementation.");return;}
        QDirIterator it(snapshot,QDir::Files|QDir::NoSymLinks,QDirIterator::Subdirectories);
        int total=0,missing=0,different=0;QStringList examples;
        while(it.hasNext()){
            QString src=it.next(),rel=QDir(snapshot).relativeFilePath(src),dst=live+"/"+rel;
            if(rel.startsWith("test-saves/") || rel.startsWith("test-config/"))continue;
            ++total;QFileInfo fi(dst);
            if(!fi.exists()){++missing;if(examples.size()<75)examples<<"MISSING "+rel;continue;}
            if(!fi.isFile()||fi.isSymLink()){++different;if(examples.size()<75)examples<<"UNSAFE TYPE "+rel;continue;}
            QFile a(src),b(dst);if(!a.open(QIODevice::ReadOnly)||!b.open(QIODevice::ReadOnly)){++different;continue;}
            QCryptographicHash ah(QCryptographicHash::Sha256),bh(QCryptographicHash::Sha256);
            ah.addData(&a);bh.addData(&b);
            if(ah.result()!=bh.result()){++different;if(examples.size()<75)examples<<"CHANGED "+rel;}
        }
        details->setText(QString("Reference: %1\\nScanned: %2\\nMissing: %3\\nChanged: %4\\n\\n%5\\n\\nNo files altered.")
                         .arg(snapshot).arg(total).arg(missing).arg(different).arg(examples.join("\n")));
    });return w;
}
QWidget* MainWindow::webPage() {
    auto *w=new QWidget(this);auto *v=new QVBoxLayout(w);
    v->addWidget(paragraph("Web — Raspberry Pi project status"));
    v->addWidget(paragraph("Read-only website: no unattended SSH writes or remote code execution enabled. Remote change requests require authenticated review."));
    auto *handoff=new QPushButton("Copy Vibe website update command",w);v->addWidget(handoff);
    connect(handoff,&QPushButton::clicked,this,[this]{prepareVibeHandoff("web.md");});
    auto *visit=new QPushButton("Open Pi project website",w);v->addWidget(visit);
    connect(visit,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("http://100.73.131.89:8080/vibepedigree/"));});
    v->addStretch();return w;
}


void MainWindow::startBot() {
    const QString script = QDir(vpProjectRoot()).filePath("bot/bot.py");
    const QFileInfo fi(script);
    if (!fi.isFile() || fi.isSymLink() || !fi.isReadable()) {
        botDetail = "Bot could not start: missing, unreadable, or symlink script: " + script;
        updateBotStatus();
        return;
    }
    botProcess = new QProcess(this);
    botProcess->setWorkingDirectory(QDir::homePath() + "/.local/share/vibepedigree");
    botProcess->setProgram("python3");
    botProcess->setArguments({script});
    botProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(botProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError){
        botDetail = "Bot process error: " + botProcess->errorString(); updateBotStatus();
    });
    connect(botProcess, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), this,
       [this](int code, QProcess::ExitStatus){
         botDetail = QString("Bot stopped (exit code %1). %2").arg(code).arg(QString::fromUtf8(botProcess->readAll()).left(400));
         updateBotStatus();
       });
    botProcess->start();
    if (!botProcess->waitForStarted(2500)) botDetail = "Bot failed to start: " + botProcess->errorString();
    updateBotStatus();
}
void MainWindow::stopBot() {
    if (!botProcess || botProcess->state()==QProcess::NotRunning) return;
    botProcess->terminate();
    if (!botProcess->waitForFinished(3000)) {
        botProcess->kill(); botProcess->waitForFinished(1500);
    }
}
void MainWindow::updateBotStatus() {
    const bool running = botProcess && botProcess->state() == QProcess::Running;
    if (botStatusLabel) botStatusLabel->setText(running ?
         "RUNNING — local watcher process active" :
         "STOPPED — watcher not running. " + botDetail);
    if (nav) {
        for(int i=0;i<nav->count();++i) if(nav->item(i)->text()=="Bot") {
            auto *item=nav->item(i);
            item->setBackground(QBrush(QColor(running ? "#36b76f" : "#e3575f")));
            item->setForeground(QBrush(QColor("#101923")));
            QFont f=item->font();f.setBold(true);item->setFont(f);
            item->setToolTip(running ? "Watcher running" : botDetail);
        }
    }
}
QWidget* MainWindow::botPage() {
    auto *w = new QWidget(this); auto *v = new QVBoxLayout(w);
    auto *heading=paragraph("Bot — local Diablo observer");
    heading->setStyleSheet("font-size:23px;font-weight:bold;"); v->addWidget(heading);
    v->addWidget(paragraph("The watcher starts with VibePedigree and stops when VibePedigree closes. Green means its process is running; red means it is not. It does not prove that Diablo is currently running."));
    botStatusLabel=paragraph("Checking bot process...");v->addWidget(botStatusLabel);
    v->addWidget(paragraph("Watcher: "+QDir(vpProjectRoot()).filePath("bot/bot.py")+"\nInstructions: "+vpInstructionsRoot()+"/bot-status.md\nLocal records: ~/.local/share/vibepedigree/vibe_bot_info.md"));
    auto *handoff=new QPushButton("Copy Vibe Bot development command",w);v->addWidget(handoff);
    connect(handoff,&QPushButton::clicked,this,[this]{prepareVibeHandoff("bot-status.md");});
    auto *refresh=new QPushButton("Refresh bot status",w);v->addWidget(refresh);
    connect(refresh,&QPushButton::clicked,this,&MainWindow::updateBotStatus);
    v->addStretch();return w;
}
